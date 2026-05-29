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
	char data[] = "abcdefghij";

	auto ss = std::make_unique<std::stringstream>(
		std::string(data, sizeof(data) - 1),
		std::ios::binary | std::ios::in);

	Reader r(std::move(ss));

	try {
		auto view = r.ReadBuffer(5);

		//Expect first 5 bytes accessible
		//exact behavior depends on ScopedView API
		if(!view.IsHandleValid())
			return Fail("null view");
	} catch(...) {
		return Fail("exception");
	}

	return 0;
}