#include "libjaguar/Writer.hpp"

#include <istream>
#include <array>
#include <stdexcept>

namespace libjaguar {
	std::ostream* Writer::operator->() {
		return (moved ? nullptr : &stream);
	}

	std::ostream* Writer::operator*() {
		return (moved ? nullptr : &stream);
	}

	void Writer::_WriteIntegerInternal(uint64_t value, uint8_t bits) {
		if(!stream) return;

		//Write out integer in little endian
		const uint8_t bytes = bits / 8;
		uint64_t work = value;
		for(uint8_t i = 0; i < bytes; ++i) {
			//Get the lowest 8 bits of the work value and write it
			const uint8_t byte = (work & 0xFF);
			stream.put(byte);

			//Discard just-written bits and move everything else 8 bits right
			work >>= 8;
		}
	}

	void Writer::WriteBool(bool value) {
		if(!stream) return;

		uint8_t val = (value ? 1 : 0);
		stream.put(val);
	}

	void Writer::WriteString(const std::string& value) {
		if(!stream) return;

		stream.write(value.data(), value.size());
	}

	void Writer::WriteBuffer(const std::span<std::byte>& value) {
		if(!stream) return;

		stream.write(reinterpret_cast<const char*>(value.data()), value.size());
	}

	void Writer::WriteBufferFromStream(std::istream* istream, std::size_t length) {
		if(!stream) return;

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
			stream.write(reinterpret_cast<char*>(chunkBuffer.data()), bytesRead);

			//Update remaining quantity
			remaining -= bytesRead;
		}
	}
}