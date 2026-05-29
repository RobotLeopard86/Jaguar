#include "libjaguar/Reader.hpp"
#include <iostream>
#include <memory>
#include <sstream>

using namespace libjaguar;

static int Fail(const char* msg) {
	std::cerr << msg << "\n";
	return -1;
}

int main() {
	char data[] = {0x01, 0x00, 0x01};

	auto ss = std::make_unique<std::stringstream>(
		std::string(data, sizeof(data)),
		std::ios::binary | std::ios::in);

	Reader r(std::move(ss));

	try {
		if(!r.ReadBool()) return Fail("true1");
		if(r.ReadBool()) return Fail("false");
		if(!r.ReadBool()) return Fail("true2");
	} catch(...) {
		return Fail("exception");
	}

	return 0;
}