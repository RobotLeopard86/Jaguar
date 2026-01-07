#include "libjaguar/Reader.hpp"
#include "Internal.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/Value.hpp"

#include <cstdint>
#include <stdexcept>

namespace libjaguar {
	Reader::Reader(std::unique_ptr<std::istream>&& istream) : stream(std::move(istream)) {}

	Reader::Reader(Reader&& other) : stream(std::move(other.stream)) {}

	Reader& Reader::operator=(Reader&& other) {
		if(this != &other) stream = std::move(other.stream);
		return *this;
	}

	std::istream* Reader::operator->() {
		return (stream ? stream.get() : nullptr);
	}

	std::istream* Reader::operator*() {
		return (stream ? stream.get() : nullptr);
	}

	uint64_t Reader::_ReadIntegerInternal(uint8_t bits) {
		if(!stream) return UINT64_MAX;

		//Read integer stored in little endian
		const uint8_t bytes = bits / 8;
		uint64_t work = 0;
		for(uint8_t i = 0; i < bytes; ++i) {
			//Read the next byte
			const uint8_t byte = stream->get();

			//Apply it to the appropriate position in the work value
			work |= (uint64_t(byte) << (i * 8));
		}

		//Return final value
		return work;
	}

	bool Reader::ReadBool() {
		if(!stream) return false;

		uint8_t byte = stream->get();
		if(byte > 1) throw std::runtime_error("Read byte is not a possible boolean value!");
		return byte == 1;
	}

	std::string Reader::ReadString(uint32_t length) {
		if(!stream) return "";

		//Setup string
		std::string data;
		data.resize(length);

		//Extract data
		stream->read(data.data(), length);

		//Check UTF-8 and return
		if(!internal::CheckUTF8(data)) throw std::runtime_error("Read string is not valid UTF-8!");
		return data;
	}

	std::vector<unsigned char> Reader::ReadBuffer(uint64_t byteCount) {
		if(!stream) return {};

		//Setup target buffer
		std::vector<unsigned char> data;
		data.resize(byteCount);

		//Extract data and return
		stream->read(reinterpret_cast<char*>(data.data()), byteCount);
		return data;
	}

	bool ValidateTypeTag(uint8_t tagByte) {
		if(tagByte < 0x0A || tagByte > 0x4B) return false;
		uint8_t lowerNibble = (tagByte & 0b0000'1111);
		uint8_t upperNibble = (tagByte & 0b1111'0000) >> 4;
		if(lowerNibble < 0xA) return false;
		if((upperNibble == 1 || upperNibble == 2) && lowerNibble > 0xD) return false;
		if(upperNibble == 4 && lowerNibble > 0xB) return false;
		if(tagByte == 0x3F) return false;
		return true;
	}

	ValueHeader Reader::ReadHeader() {
		if(!stream) return {};

		//Create result object
		ValueHeader header;

		//Read and validate type tag
		uint8_t tagByte = stream->get();
		if(!ValidateTypeTag(tagByte)) throw std::runtime_error("Read TypeTag is invalid!");
		uint8_t upperNibble = (tagByte & 0b1111'0000) >> 4;
		header.type = (TypeTag)tagByte;
		if(header.type == TypeTag::ScopeBoundary) return header;

		//Read and check name string
		uint8_t nameLen = _ReadIntegerInternal(8);
		if(nameLen == 0) throw std::runtime_error("Read name string is empty!");
		header.name.resize(nameLen);
		stream->read(header.name.data(), nameLen);
		if(!internal::CheckUTF8(header.name)) throw std::runtime_error("Read name string is not valid UTF-8!");

		//For simple types, we're done
		//We can check this easily using the tag byte
		if((upperNibble == 1 || upperNibble == 2) || header.type == TypeTag::Float32 || header.type == TypeTag::Float64 || header.type == TypeTag::Boolean) return header;

		//More complex data
		switch(header.type) {
			case TypeTag::List: {
				//Get element TypeTag
				uint8_t elemTagByte = stream->get();
				if(!ValidateTypeTag(elemTagByte)) throw std::runtime_error("Encountered invalid element TypeTag!");
				header.elementType = (TypeTag)elemTagByte;

				//Get element count
				header.size = (uint32_t)_ReadIntegerInternal(32);
				break;
			}
			case TypeTag::Vector: {
				//Get element TypeTag
				uint8_t elemTagByte = stream->get();
				if(!ValidateTypeTag(elemTagByte)) throw std::runtime_error("Encountered invalid element TypeTag!");
				header.elementType = (TypeTag)elemTagByte;

				//Get vector width
				header.width = (uint8_t)_ReadIntegerInternal(8);
				break;
			}
			case TypeTag::Matrix: {
				//Get element TypeTag
				uint8_t elemTagByte = stream->get();
				if(!ValidateTypeTag(elemTagByte)) throw std::runtime_error("Encountered invalid element TypeTag!");
				header.elementType = (TypeTag)elemTagByte;

				//Get matrix width and height
				header.width = (uint8_t)_ReadIntegerInternal(8);
				header.height = (uint8_t)_ReadIntegerInternal(8);
				break;
			}
			case TypeTag::StructuredObj:
			case TypeTag::StructuredObjTypeDecl: {
				//Read and check type ID string
				uint8_t typeIDLen = _ReadIntegerInternal(8);
				if(typeIDLen == 0) throw std::runtime_error("Encountered empty type ID string!");
				header.typeID.resize(typeIDLen);
				stream->read(header.typeID.data(), typeIDLen);
				if(!internal::CheckUTF8(header.typeID)) throw std::runtime_error("Encountered a type ID string that is not valid UTF-8!");

				//Break for StructuredObj (StructuredObjTypeDecl has same next field as UnstructuredObj so we intentionally fallthrough there)
				if(header.type == TypeTag::StructuredObj) break;
			}
			case TypeTag::UnstructuredObj:
				//Get field count
				header.fieldCount = (uint16_t)_ReadIntegerInternal(16);
				break;
			case TypeTag::String:
				//Get string size
				header.size = (uint32_t)_ReadIntegerInternal(32);
				break;
			case TypeTag::ByteBuffer:
			case TypeTag::Substream:
				//Get buffer size
				header.bufferSize = _ReadIntegerInternal(64);
				break;
			default: break;
		}
		return header;
	}
}