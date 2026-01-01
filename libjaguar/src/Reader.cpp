#include "libjaguar/Reader.hpp"

namespace libjaguar {
	std::istream* Reader::operator->() {
		return (moved ? nullptr : &stream);
	}

	std::istream* Reader::operator*() {
		return (moved ? nullptr : &stream);
	}
}