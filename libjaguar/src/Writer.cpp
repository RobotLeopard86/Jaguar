#include "libjaguar/Writer.hpp"
#include "Internal.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/Value.hpp"

#include <istream>
#include <array>
#include <stdexcept>
#include <algorithm>

namespace libjaguar {
	Writer::Writer(std::unique_ptr<std::ostream>&& ostream) : stream(std::move(ostream)) {}

	Writer::Writer(Writer&& other) : stream(std::move(other.stream)) {}

	Writer& Writer::operator=(Writer&& other) {
		if(this != &other) stream = std::move(other.stream);
		return *this;
	}

	std::ostream* Writer::operator->() {
		return (stream ? stream.get() : nullptr);
	}

	std::ostream* Writer::operator*() {
		return (stream ? stream.get() : nullptr);
	}

	void Writer::_WriteIntegerInternal(uint64_t value, uint8_t bits) {
		if(!stream) return;

		//Write out integer in little endian
		const uint8_t bytes = bits / 8;
		uint64_t work = value;
		for(uint8_t i = 0; i < bytes; ++i) {
			//Get the lowest 8 bits of the work value and write it
			const uint8_t byte = (work & 0xFF);
			stream->put(byte);

			//Discard just-written bits and move everything else 8 bits right
			work >>= 8;
		}
	}

	void Writer::WriteBool(bool value) {
		if(!stream) return;

		uint8_t val = (value ? 1 : 0);
		stream->put(val);
	}

	void Writer::WriteString(const std::string& value) {
		if(!stream) return;
		if(!internal::CheckUTF8(value)) throw std::runtime_error("String is not valid UTF-8!");

		stream->write(value.data(), value.size());
	}

	void Writer::WriteBuffer(const std::span<std::byte>& value) {
		if(!stream) return;

		stream->write(reinterpret_cast<const char*>(value.data()), value.size());
	}

	void Writer::WriteBufferFromStream(std::istream* istream, std::size_t length) {
		if(!stream) return;
		if(istream == nullptr) throw std::runtime_error("Cannot write buffer from a null source stream!");
		if(!(*istream)) throw std::runtime_error("Cannot write buffer from an invalid source stream!");

		//Read and write data in chunks
		constexpr std::size_t chunkSize = 64 * 1024;//64 KiB (one KiB is 1024 bytes)
		std::array<unsigned char, chunkSize> chunkBuffer;
		std::size_t remaining = length;
		while(remaining > 0) {
			//Read data
			std::size_t toRead = std::min(chunkSize, remaining);
			istream->read(reinterpret_cast<char*>(chunkBuffer.data()), toRead);

			//Write data back
			std::size_t bytesRead = istream->gcount();
			if(bytesRead == 0) throw std::runtime_error("Failed to read chunk for buffer stream transfer!");
			stream->write(reinterpret_cast<char*>(chunkBuffer.data()), bytesRead);

			//Update remaining quantity
			remaining -= bytesRead;
		}
	}

	void Writer::WriteHeader(const ValueHeader& header, bool noIdentifier) {
		if(!stream) return;
		if(header.name.size() < 1 || header.name.size() > UINT8_MAX) throw std::runtime_error("Header name string is invalid length!");
		if(!internal::CheckUTF8(header.name)) throw std::runtime_error("Header name string is not valid UTF-8!");
		if(header.type == TypeTag::StructuredObj || header.type == TypeTag::StructuredObjTypeDecl) {
			if(header.typeID.size() < 1 || header.typeID.size() > UINT8_MAX) throw std::runtime_error("Header type ID string is invalid length!");
			if(!internal::CheckUTF8(header.typeID)) throw std::runtime_error("Header type ID string is not valid UTF-8!");
		}

		//Write identifier
		if(!noIdentifier) {
			//Write type tag
			stream->put(static_cast<uint8_t>(header.type));

			//Write name string
			_WriteIntegerInternal(header.name.size(), 8);
			WriteString(header.name);
		}

		//Write type-specific data
		switch(header.type) {
			case TypeTag::List:
				stream->put(static_cast<uint8_t>(header.elementType));
				_WriteIntegerInternal(header.size, bits_v<decltype(header.size)>);
				break;
			case TypeTag::Vector:
				stream->put(static_cast<uint8_t>(header.elementType));
				_WriteIntegerInternal(header.width, bits_v<decltype(header.width)>);
				break;
			case TypeTag::Matrix:
				stream->put(static_cast<uint8_t>(header.elementType));
				_WriteIntegerInternal(header.width, bits_v<decltype(header.width)>);
				_WriteIntegerInternal(header.height, bits_v<decltype(header.height)>);
				break;
			case TypeTag::StructuredObj:
			case TypeTag::StructuredObjTypeDecl:
				_WriteIntegerInternal(header.typeID.size(), 8);
				WriteString(header.typeID);
				//Intentional fall-through since the below part is common to StructruedObjTypeDecl and UnstructuredObj, but not StructuredObj
				if(header.type != TypeTag::StructuredObjTypeDecl) break;
			case TypeTag::UnstructuredObj:
				_WriteIntegerInternal(header.fieldCount, bits_v<decltype(header.fieldCount)>);
				break;
			case TypeTag::String:
				_WriteIntegerInternal(header.size, bits_v<decltype(header.size)>);
				break;
			case TypeTag::ByteBuffer:
			case TypeTag::Substream:
				_WriteIntegerInternal(header.bufferSize, bits_v<decltype(header.bufferSize)>);
				break;
			default: break;
		}
	}
}