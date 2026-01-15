#include "libjaguar/Encoder.hpp"

namespace libjaguar {
	Encoder::Encoder(Writer&& writer) : writer(std::move(writer)), writerValid(true) {}

	Encoder::Encoder(Encoder&& other) : writer(std::move(other.writer)), writerValid(true) {
		other.writerValid = false;
	}

	Encoder& Encoder::operator=(Encoder&& other) {
		if(this != &other) {
			writer = std::move(other.writer);
			writerValid = true;
			other.writerValid = false;
		}
		return *this;
	}

	std::ostream* Encoder::operator->() {
		return (writerValid ? *writer : nullptr);
	}

	std::ostream* Encoder::operator*() {
		return (writerValid ? *writer : nullptr);
	}
}