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
	//NOTE: replace with real encoded header once confirmed
	const char data[] = {
		0x00, 0x00, 0x00, 0x00};

	auto ss = std::make_unique<std::stringstream>(
		std::string(data, sizeof(data)),
		std::ios::binary | std::ios::in);

	Reader r(std::move(ss));

	try {
		ValueHeader h = r.ReadHeader();
		(void)h;
		Fail("Header fail not caught!");
	} catch(...) {
		//expected for malformed input
		return 0;
	}

	return 0;
}