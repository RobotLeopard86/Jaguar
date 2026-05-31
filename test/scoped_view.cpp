#include "libjaguar/Document.hpp"
#include "libjaguar/Reader.hpp"
#include <vector>
#include <sstream>
#include <iostream>

int main() {
	try {
		std::vector<uint8_t> buf = {0x01, 0x02, 0xFF, 0x00};
		std::string bufStr(reinterpret_cast<char*>(buf.data()), buf.size());
		std::unique_ptr<std::istream> is(std::make_unique<std::istringstream>(bufStr));
		libjaguar::Reader reader(std::move(is));
		auto handle = reader.ReadBuffer(static_cast<uint32_t>(buf.size()));
		if(!handle.IsHandleValid()) return -1;
		std::vector<uint8_t> out(buf.size());
		handle->Read(out, static_cast<uint32_t>(buf.size()));
		if(out != buf) return -1;
		handle->DiscardAll();
		return 0;
	} catch(...) { return -1; }
}
