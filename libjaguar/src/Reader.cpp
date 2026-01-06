#include "libjaguar/Reader.hpp"

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
}