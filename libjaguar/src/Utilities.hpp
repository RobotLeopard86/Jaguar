#pragma once

#include "libjaguar/TypeTags.hpp"
#include "libjaguar/Reader.hpp"

#include <cstdint>
#include <array>
#include <stdexcept>

constexpr inline uint32_t scopedViewChunkSize = 64 * 1024;//64 KiB (one KiB is 1024 bytes)

namespace libjaguar {
	uint32_t GetTypeSize(TypeTag type);

	class ScopedViewStreambuf : public std::streambuf {
	  public:
		ScopedViewStreambuf(ScopedView* srv) : view(srv) {
			//Check validity
			if(!view) throw std::runtime_error("Cannot create scoped view streambuf with a null view!");
			if(!view->IsValid() || view->GetBytesRemaining() == 0) throw std::runtime_error("Cannot create scoped view streambuf with invalid or exhausted view!");

			//Call underflow() to populate the buffer initially
			if(underflow() == EOF) throw std::runtime_error("Unexpected IO error during initial scoped view streambuf population!");
		}

	  protected:
		std::streamsize showmanyc() override {
			return okRange;
		}

		int underflow() override {
			if(!view->IsValid() || view->GetBytesRemaining() == 0) return EOF;

			//Read initial data
			okRange = std::min<uint32_t>(chunkBuffer.size(), view->GetBytesRemaining());
			view->Read(chunkBuffer, okRange);

			//Set get area
			char* chunkBufBasePtr = reinterpret_cast<char*>(chunkBuffer.data());
			setg(chunkBufBasePtr, chunkBufBasePtr, chunkBufBasePtr + okRange);

			return chunkBuffer[0];
		}

	  private:
		ScopedView* view;

		uint32_t okRange;
		std::array<unsigned char, scopedViewChunkSize> chunkBuffer;
	};
}