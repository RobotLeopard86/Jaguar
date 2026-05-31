#include "libjaguar/Document.hpp"
#include <sstream>
#include <vector>

int main() {
	try {
		libjaguar::Document doc;
		//basic types
		doc.SetOrCreateValue<bool>("flag", true);
		doc.SetOrCreateValue<int32_t>("int32", 42);
		doc.SetOrCreateValue<uint16_t>("uint16", 65535u);
		doc.SetOrCreateValue<float>("float32", 3.14f);
		doc.SetOrCreateValue<double>("float64", 2.71828);
		doc.SetOrCreateValue<std::string>("msg", std::string("hello"));
		//byte buffer
		std::vector<uint8_t> buf = {0x01, 0x02, 0x03, 0xFF};
		doc.CreateValue<std::vector<uint8_t>>("bytes", true);
		doc.SetValue<std::vector<uint8_t>>("bytes", buf);
		//vectors
		libjaguar::Vector<int64_t, 2> vec2 {.x = 1, .y = 2};
		libjaguar::Vector<float, 3> vec3 {.x = 1.1f, .y = 2.2f, .z = 3.3f};
		libjaguar::Vector<double, 4> vec4 {.x = 4.4, .y = 5.5, .z = 6.6, .w = 7.7};
		doc.SetOrCreateValue<libjaguar::Vector<int64_t, 2>>("vec2", vec2);
		doc.SetOrCreateValue<libjaguar::Vector<float, 3>>("vec3", vec3);
		doc.SetOrCreateValue<libjaguar::Vector<double, 4>>("vec4", vec4);

		//serialize
		std::stringstream ss;
		doc.ExportTo(ss);
		std::string data = ss.str();
		//load into new document
		auto iss = std::make_unique<std::istringstream>(data);
		libjaguar::Document d2(std::move(iss));

		//verify
		if(d2.QueryValue<bool>("flag") != true) return -1;
		if(d2.QueryValue<int32_t>("int32") != 42) return -1;
		if(d2.QueryValue<uint16_t>("uint16") != 65535u) return -1;
		if(std::abs(d2.QueryValue<float>("float32") - 3.14f) > 1e-6f) return -1;
		if(std::abs(d2.QueryValue<double>("float64") - 2.71828) > 1e-9) return -1;
		if(d2.QueryValue<std::string>("msg").compare("hello") != 0) return -1;
		if(d2.QueryValue<std::vector<uint8_t>>("bytes") != buf) return -1;
		if(auto v2 = d2.QueryValue<libjaguar::Vector<int64_t, 2>>("vec2"); v2.x != vec2.x || v2.y != vec2.y) return -1;
		if(auto v3 = d2.QueryValue<libjaguar::Vector<float, 3>>("vec3"); v3.x != vec3.x || v3.y != vec3.y || v3.z != vec3.z) return -1;
		if(auto v4 = d2.QueryValue<libjaguar::Vector<double, 4>>("vec4"); v4.x != vec4.x || v4.y != vec4.y || v4.z != vec4.z || v4.w != vec4.w) return -1;

		return 0;
	} catch(...) {
		return -1;
	}
}
