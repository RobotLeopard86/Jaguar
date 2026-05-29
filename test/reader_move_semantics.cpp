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
	auto ss = std::make_unique<std::stringstream>("", std::ios::binary);

	Reader r1(std::move(ss));
	Reader r2 = std::move(r1);

	if(r1.operator->() != nullptr)
		return Fail("moved-from not null expected");

	try {
		if(r2.operator->() == nullptr)
			return Fail("valid moved-to null");
	} catch(...) {
		return Fail("exception");
	}

	return 0;
}