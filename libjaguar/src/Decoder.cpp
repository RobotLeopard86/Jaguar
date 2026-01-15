#include "libjaguar/Decoder.hpp"

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

	std::istream* Decoder::operator->() {
		return (readerValid ? *reader : nullptr);
	}

	std::istream* Decoder::operator*() {
		return (readerValid ? *reader : nullptr);
	}
}