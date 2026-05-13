#include "libjaguar/Reader.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/ValueHeader.hpp"
#include "Utilities.hpp"

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <cmath>

#define READ_ERROR(msg)                                                                              \
	{                                                                                                \
		std::stringstream ss;                                                                        \
		ss << "[Read Error] At offset 0x" << std::hex << stream->tellg() << std::dec << ": " << msg; \
		throw std::runtime_error(ss.str());                                                          \
	}

#define STREAMCHECK                                            \
	if(stream->eof()) READ_ERROR("Unexpected EOF in stream!"); \
	if(!stream->good()) READ_ERROR("Unexpected stream IO error!")

#define VIEW_STREAMCHECK                                                                     \
	if(stream->eof()) {                                                                      \
		valid = false;                                                                       \
		READ_ERROR("Unexpected EOF in stream! View is now invalid; do not continue use.");   \
	}                                                                                        \
	if(!stream->good()) {                                                                    \
		valid = false;                                                                       \
		READ_ERROR("Unexpected stream IO error! View is now invalid; do not continue use."); \
	}

namespace libjaguar {
	Reader::Reader(std::unique_ptr<std::istream>&& istream) : stream(std::move(istream)) {}

	Reader::Reader(Reader&& other) : stream(std::move(other.stream)) {}

	Reader& Reader::operator=(Reader&& other) {
		if(this != &other) stream = std::move(other.stream);
		return *this;
	}

	void Reader::VerifyOk() {
		//Check stream integrity
		if(!stream) throw std::runtime_error("Cannot perform operations without a backing stream!");
		if(!stream->eof() && !stream->good()) throw std::runtime_error("Cannot perform operations with a broken stream!");

		//Check read view state
		if(view) {
			if(view->GetBytesRemaining() == 0 || !view->valid) {
				//The view is exhausted or invalid, we can destroy it and proceed
				view->valid = false;
				*viewState = false;
				view.reset();
			} else {
				//The view is still active - operation not allowed
				throw std::runtime_error("Cannot perform operations while a ScopedView is active!");
			}
		}
	}

	std::istream* Reader::operator->() {
		try {
			VerifyOk();
		} catch(...) {
			return nullptr;
		}
		return stream.get();
	}

	std::istream* Reader::operator*() {
		try {
			VerifyOk();
		} catch(...) {
			return nullptr;
		}
		return stream.get();
	}

	uint64_t Reader::_ReadIntegerInternal(uint8_t bits) {
		VerifyOk();

		//Read integer stored in little endian
		const uint8_t bytes = bits / 8;
		uint64_t work = 0;
		for(uint8_t i = 0; i < bytes; ++i) {
			//Read the next byte
			const uint8_t byte = stream->get();
			STREAMCHECK;

			//Apply it to the appropriate position in the work value
			work |= (uint64_t(byte) << (i * 8));
		}

		//Return final value
		return work;
	}

	bool Reader::ReadBool() {
		VerifyOk();

		uint8_t byte = stream->get();
		STREAMCHECK;
		if(byte > 1) READ_ERROR("Read byte is not a possible boolean value!");
		return byte == 1;
	}

	std::string Reader::ReadString(uint32_t length) {
		VerifyOk();
		if(length >= std::pow(2, 24)) READ_ERROR("String is longer than maximum legal size!");

		//Setup string
		std::string data;
		data.resize(length);

		//Extract data
		stream->read(data.data(), length);
		STREAMCHECK;

		//Check UTF-8 and return
		if(!CheckUTF8(data)) READ_ERROR("Read string is not valid UTF-8!");
		return data;
	}

	SVHandle Reader::ReadBuffer(uint32_t length) {
		VerifyOk();

		//Reset view state
		if(view) *viewState = false;
		view.reset(new ScopedView(stream.get(), length));
		viewState = std::make_shared<bool>(true);

		//Make handle and return
		SVHandle svh;
		svh.valid = viewState;
		svh.view = view.get();
		return svh;
	}

	ValueHeader Reader::ReadHeader() {
		VerifyOk();

		//Create result object
		ValueHeader header = {};

		//Read and validate type tag
		uint8_t tagByte = stream->get();
		STREAMCHECK;
		if(!ValidateTypeTag(tagByte)) READ_ERROR("Read TypeTag is invalid!");
		uint8_t upperNibble = (tagByte & 0b1111'0000) >> 4;
		uint8_t lowerNibble = (tagByte & 0b0000'1111);
		header.type = (TypeTag)tagByte;
		if(header.type == TypeTag::ScopeBoundary) return header;

		//Read and check name string
		uint8_t nameLen = _ReadIntegerInternal(8);
		if(nameLen == 0) READ_ERROR("Read name string is empty!");
		header.name.resize(nameLen);
		stream->read(header.name.data(), nameLen);
		STREAMCHECK;
		if(!CheckUTF8(header.name)) READ_ERROR("Read name string is not valid UTF-8!");

		//For simple types, we're done
		//We can check this easily using the tag byte
		if(upperNibble == 1 || upperNibble == 2 || (upperNibble == 0 && lowerNibble > 0xC)) return header;

		//More complex data
		switch(header.type) {
			case TypeTag::List: {
				//Get element TypeTag
				uint8_t elemTagByte = stream->get();
				STREAMCHECK;
				if(!ValidateTypeTag(elemTagByte)) READ_ERROR("Encountered invalid element TypeTag!");
				header.elementType = (TypeTag)elemTagByte;

				//Special type handling
				if(header.elementType == TypeTag::StructuredObj) {
					//Structured object
					uint8_t typeIDLen = _ReadIntegerInternal(8);
					if(typeIDLen == 0) READ_ERROR("Encountered empty type ID string for list of structured objects!");
					header.typeID.resize(typeIDLen);
					stream->read(header.typeID.data(), typeIDLen);
					STREAMCHECK;
					if(!CheckUTF8(header.typeID)) READ_ERROR("Encountered a type ID string that is not valid UTF-8!");
				} else if(static_cast<uint8_t>(header.elementType) > 0x40) {
					//Vector/matrix

					//Get nested TypeTag
					uint8_t nestedTagByte = stream->get();
					STREAMCHECK;
					if(!ValidateTypeTag(nestedTagByte)) READ_ERROR("Encountered invalid vector/matrix element TypeTag!");
					header.nestedElementType = (TypeTag)nestedTagByte;

					//Get size(s)
					header.width = (uint8_t)_ReadIntegerInternal(8);
					if(header.elementType == TypeTag::Matrix) header.height = (uint8_t)_ReadIntegerInternal(8);
				}

				//Get element count
				header.size = (uint32_t)_ReadIntegerInternal(32);
				break;
			}
			case TypeTag::Vector: {
				//Get element TypeTag
				uint8_t elemTagByte = stream->get();
				STREAMCHECK;
				if(!ValidateTypeTag(elemTagByte)) READ_ERROR("Encountered invalid element TypeTag!");
				header.elementType = (TypeTag)elemTagByte;

				//Get vector width
				header.width = (uint8_t)_ReadIntegerInternal(8);
				break;
			}
			case TypeTag::Matrix: {
				//Get element TypeTag
				uint8_t elemTagByte = stream->get();
				STREAMCHECK;
				if(!ValidateTypeTag(elemTagByte)) READ_ERROR("Encountered invalid element TypeTag!");
				header.elementType = (TypeTag)elemTagByte;

				//Get matrix width and height
				header.width = (uint8_t)_ReadIntegerInternal(8);
				header.height = (uint8_t)_ReadIntegerInternal(8);
				break;
			}
			case TypeTag::StructuredObj: {
				//Read and check type ID string
				uint8_t typeIDLen = _ReadIntegerInternal(8);
				if(typeIDLen == 0) READ_ERROR("Encountered empty type ID string!");
				header.typeID.resize(typeIDLen);
				stream->read(header.typeID.data(), typeIDLen);
				STREAMCHECK;
				if(!CheckUTF8(header.typeID)) READ_ERROR("Encountered a type ID string that is not valid UTF-8!");
				break;
			}
			case TypeTag::StructuredObjTypeDecl:
			case TypeTag::UnstructuredObj:
				//Get field count
				header.fieldCount = (uint16_t)_ReadIntegerInternal(16);
				break;
			case TypeTag::String:
			case TypeTag::ByteBuffer:
				//Get buffer size
				header.size = _ReadIntegerInternal(32);
				break;
			default: break;
		}
		return header;
	}

	ScopedView::ScopedView(std::istream* streamPtr, std::streamoff size)
	  : stream(streamPtr), end(stream->tellg() + size), valid(true), eof(false) {}

	void ScopedView::_ReadInternal(std::span<unsigned char>& out, uint32_t byteCount) {
		if(!valid || eof) READ_ERROR("Cannot perform operations on an invalid scoped read view!");
		if(byteCount > out.size_bytes()) READ_ERROR("Byte read count exceeds the size of the output buffer!");
		if(byteCount > GetBytesRemaining()) READ_ERROR("Byte read count exceeds number of remaining bytes!");

		stream->read(reinterpret_cast<char*>(out.data()), byteCount);
		VIEW_STREAMCHECK;

		if(GetBytesRemaining() == 0) eof = true;
	}

	uint32_t ScopedView::GetBytesRemaining() const {
		if(eof) return 0;
		if(!valid) READ_ERROR("Cannot perform operations on an invalid scoped read view!");
		return end - stream->tellg();
	}

	void ScopedView::Discard(uint32_t byteCount) {
		if(!valid || eof) READ_ERROR("Cannot perform operations on an invalid scoped read view!");
		if(byteCount > GetBytesRemaining()) READ_ERROR("Byte discard count exceeds number of remaining bytes!");

		stream->ignore(byteCount);
		VIEW_STREAMCHECK;

		if(GetBytesRemaining() == 0) eof = true;
	}

	void ScopedView::DiscardAll() {
		if(!valid || eof) READ_ERROR("Cannot perform operations on an invalid scoped read view!");
		Discard(GetBytesRemaining());
	}
}