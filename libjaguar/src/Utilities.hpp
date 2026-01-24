#pragma once

#include "libjaguar/TypeTags.hpp"
#include "libjaguar/ScopedView.hpp"

#include <cstdint>
#include <array>
#include <stdexcept>

constexpr inline uint32_t scopedViewChunkSize = 64 * 1024;//64 KiB (one KiB is 1024 bytes)

namespace libjaguar {
	uint32_t GetTypeSize(TypeTag type);

	class SVstreambuf : public std::streambuf {
	  public:
		SVstreambuf(SVHandle&& handle) : handle(std::move(handle)) {
			//Check validity
			if(!handle.IsHandleValid()) throw std::runtime_error("Cannot create scoped view streambuf with a null view!");
			if(!handle->IsValid() || handle->GetBytesRemaining() == 0) throw std::runtime_error("Cannot create scoped view streambuf with invalid or exhausted view!");

			//Call underflow() to populate the buffer initially
			if(underflow() == EOF) throw std::runtime_error("Unexpected IO error during initial scoped view streambuf population!");
		}

	  protected:
		std::streamsize showmanyc() override {
			return okRange;
		}

		int underflow() override {
			if(!handle.IsHandleValid() || !handle->IsValid() || handle->GetBytesRemaining() == 0) {
				okRange = 0;
				return EOF;
			}

			//Read initial data
			okRange = std::min<uint32_t>(chunkBuffer.size(), handle->GetBytesRemaining());
			handle->Read(chunkBuffer, okRange);

			//Set get area
			char* chunkBufBasePtr = reinterpret_cast<char*>(chunkBuffer.data());
			setg(chunkBufBasePtr, chunkBufBasePtr, chunkBufBasePtr + okRange);

			return chunkBuffer[0];
		}

	  private:
		SVHandle handle;

		uint32_t okRange;
		std::array<unsigned char, scopedViewChunkSize> chunkBuffer;
	};

	class SVistream : public std::istream {
	  public:
		SVistream(SVHandle&& handle)
		  : std::istream(bufInit(std::move(handle))) {
			rdbuf(buf.get());
			init(buf.get());
		}

	  private:
		std::unique_ptr<SVstreambuf> buf;

		SVstreambuf* bufInit(SVHandle&& handle) {
			buf = std::make_unique<SVstreambuf>(std::move(handle));
			return buf.get();
		}
	};

	inline bool CheckUTF8(const std::string& string) {
		//Keep track of expected continuation bytes (to prevent overlong encodings)
		uint8_t expectedContinuations = 0;

		//Check characters
		for(char character : string) {
			//Convert to byte
			uint8_t byte = static_cast<uint8_t>(character);

			//Check if this is leading byte/ASCII or a continuation byte?
			if(expectedContinuations == 0) {
				//ASCII: 0xxx'xxxx
				if((byte & 0b1000'0000) == 0b0000'0000) {
					continue;
				}

				//Start of 2-byte sequence: 110x'xxxx
				if((byte & 0b1110'0000) == 0b1100'0000) {
					expectedContinuations = 1;
					continue;
				}

				//Start of 3-byte sequence: 1110'xxxx
				if((byte & 0b1111'0000) == 0b1110'0000) {
					expectedContinuations = 2;
					continue;
				}

				//Start of 4-byte sequence: 1111'0xxx
				if((byte & 0b1111'1000) == 0b1111'0000) {
					expectedContinuations = 3;
					continue;
				}

				//Invalid leading byte
				return false;
			} else {
				//Continuation bytes must be 10xx'xxxx
				if((byte & 0b1100'0000) == 0b1000'0000) {
					--expectedContinuations;
					continue;
				} else {
					return false;
				}
			}
		}

		//All characters passed, string is valid as long as we don't have outstanding continuation bytes
		return expectedContinuations == 0;
	}
}