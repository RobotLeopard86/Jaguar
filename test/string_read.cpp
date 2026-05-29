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
	//"hello" in UTF-8 + length 5
	const char data[] = {
		'h', 'e', 'l', 'l', 'o'};

	auto ss = std::make_unique<std::stringstream>(
		std::string(data, 5),
		std::ios::binary | std::ios::in);

	Reader r(std::move(ss));

	try {
		auto s = r.ReadString(5);
		if(s != "hello")
			return Fail("string mismatch");
	} catch(const std::exception& e) {
		std::cerr << e.what() << "\n";
		return -1;
	}

	return 0;
}