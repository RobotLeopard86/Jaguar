#include "libjaguar/Reader.hpp"

#include <iostream>
#include <memory>
#include <sstream>
#include <vector>
#include <cstdint>

using namespace libjaguar;

static int Fail(const char* msg) {
	std::cerr << msg << "\n";
	return -1;
}

template<typename T>
static void PushLE(std::vector<uint8_t>& out, T v) {
	for(size_t i = 0; i < sizeof(T); ++i)
		out.push_back((v >> (8 * i)) & 0xFF);
}

int main() {
	std::vector<uint8_t> data;

	PushLE<int8_t>(data, -8);
	PushLE<uint8_t>(data, 200);
	PushLE<int16_t>(data, -1234);
	PushLE<uint16_t>(data, 54321);
	PushLE<int32_t>(data, -123456789);
	PushLE<uint32_t>(data, 3456789012u);
	PushLE<int64_t>(data, -1234567890123456789ll);
	PushLE<uint64_t>(data, 12345678901234567890ull);

	auto ss = std::make_unique<std::stringstream>(
		std::string((char*)data.data(), data.size()),
		std::ios::binary | std::ios::in);

	Reader r(std::move(ss));

	try {
		if(r.ReadInteger<int8_t>() != -8) return Fail("int8");
		if(r.ReadInteger<uint8_t>() != 200) return Fail("uint8");
		if(r.ReadInteger<int16_t>() != -1234) return Fail("int16");
		if(r.ReadInteger<uint16_t>() != 54321) return Fail("uint16");
		if(r.ReadInteger<int32_t>() != -123456789) return Fail("int32");
		if(r.ReadInteger<uint32_t>() != 3456789012u) return Fail("uint32");
		if(r.ReadInteger<int64_t>() != -1234567890123456789ll) return Fail("int64");
		if(r.ReadInteger<uint64_t>() != 12345678901234567890ull) return Fail("uint64");
	} catch(const std::exception& e) {
		std::cerr << "exception: " << e.what() << "\n";
		return -1;
	}

	return 0;
}