#include "libjaguar/Reader.hpp"
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>
#include <bit>
#include <cstdint>

using namespace libjaguar;

static int Fail(const char* msg) {
	std::cerr << msg << "\n";
	return -1;
}

template<typename T>
static void PushLE(std::vector<uint8_t>& out, T v) {
	for(size_t i = 0; i < sizeof(T); ++i)
		out.push_back((std::bit_cast<uint64_t>(v) >> (8 * i)) & 0xFF);
}

template<>
void PushLE<float>(std::vector<uint8_t>& out, float v) {
	uint32_t b = std::bit_cast<uint32_t>(v);
	for(size_t i = 0; i < 4; ++i)
		out.push_back((b >> (8 * i)) & 0xFF);
}

template<>
void PushLE<double>(std::vector<uint8_t>& out, double v) {
	uint64_t b = std::bit_cast<uint64_t>(v);
	for(size_t i = 0; i < 8; ++i)
		out.push_back((b >> (8 * i)) & 0xFF);
}

int main() {
	std::vector<uint8_t> data;

	PushLE<float>(data, 3.1415926f);
	PushLE<double>(data, 2.718281828459045);

	auto ss = std::make_unique<std::stringstream>(
		std::string((char*)data.data(), data.size()),
		std::ios::binary | std::ios::in);

	Reader r(std::move(ss));

	try {
		float f = r.ReadFloat<float>();
		double d = r.ReadFloat<double>();

		if(std::bit_cast<uint32_t>(f) != std::bit_cast<uint32_t>(3.1415926f))
			return Fail("float mismatch");

		if(std::bit_cast<uint64_t>(d) != std::bit_cast<uint64_t>(2.718281828459045))
			return Fail("double mismatch");
	} catch(const std::exception& e) {
		std::cerr << e.what() << "\n";
		return -1;
	}

	return 0;
}