#include "libjaguar/Decoder.hpp"

#include <stdexcept>

namespace libjaguar {
	Decoder::Decoder(Reader&& reader) : reader(std::move(reader)), readerValid(true) {}

	Decoder::Decoder(Decoder&& other) : reader(std::move(other.reader)), readerValid(true) {
		other.readerValid = false;
	}

	Decoder& Decoder::operator=(Decoder&& other) {
		if(this != &other) {
			reader = std::move(other.reader);
			readerValid = true;
			other.readerValid = false;
		}
		return *this;
	}

	Reader& Decoder::GetReader() {
		if(!readerValid) throw std::runtime_error("Decoder has no valid reader!");
		return reader;
	}
}