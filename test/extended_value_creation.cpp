#include "libjaguar/MathTypes.hpp"
#include "libjaguar/Document.hpp"
#include "libjaguar/TypeTags.hpp"
#include <cassert>
#include <vector>
#include <string>

using namespace libjaguar;

int main() {
	Document doc;//empty document

	//primitive types
	doc.SetOrCreateValue<int32_t>("int32", 42);
	doc.SetOrCreateValue<std::vector<int32_t>>("int32_list", std::vector<int32_t> {1, 2, 3});

	doc.SetOrCreateValue<uint8_t>("uint8", 255);
	doc.SetOrCreateValue<std::vector<uint8_t>>("uint8_list", true, std::vector<uint8_t> {10, 20, 30});

	doc.SetOrCreateValue<bool>("bool", true);
	doc.SetOrCreateValue<std::vector<bool>>("bool_list", std::vector<bool> {true, false, true});

	doc.SetOrCreateValue<std::string>("string", std::string("hello"));
	doc.SetOrCreateValue<std::vector<std::string>>("string_list", std::vector<std::string> {"a", "b"});

	doc.SetOrCreateValue<float>("float32", 3.14f);
	doc.SetOrCreateValue<std::vector<float>>("float32_list", std::vector<float> {1.1f, 2.2f});

	doc.SetOrCreateValue<double>("float64", 6.28);
	doc.SetOrCreateValue<std::vector<double>>("float64_list", std::vector<double> {0.1, 0.2});

	//math types (vectors)
	libjaguar::Vector<float, 2> v2 {.x = 1.0f, .y = 2.0f};
	doc.SetOrCreateValue<libjaguar::Vector<float, 2>>("vec2", v2);
	doc.SetOrCreateValue<std::vector<libjaguar::Vector<float, 2>>>("vec2_list", std::vector<libjaguar::Vector<float, 2>> {v2, v2});

	libjaguar::Vector<float, 3> v3 {.x = 1.0f, .y = 2.0f, .z = 3.0f};
	doc.SetOrCreateValue<libjaguar::Vector<float, 3>>("vec3", v3);
	doc.SetOrCreateValue<std::vector<libjaguar::Vector<float, 3>>>("vec3_list", std::vector<libjaguar::Vector<float, 3>> {v3});

	libjaguar::Vector<float, 4> v4 {.x = 1.0f, .y = 2.0f, .z = 3.0f, .w = 4.0f};
	doc.SetOrCreateValue<libjaguar::Vector<float, 4>>("vec4", v4);
	doc.SetOrCreateValue<std::vector<libjaguar::Vector<float, 4>>>("vec4_list", std::vector<libjaguar::Vector<float, 4>> {v4});

	//matrix 2x2
	libjaguar::Matrix<float, 2, 2> mat;
	mat[0] = {1.0f, 3.14f};
	mat[1] = {2.313f, 9.214f};
	doc.SetOrCreateValue<libjaguar::Matrix<float, 2, 2>>("mat2x2", mat);
	doc.SetOrCreateValue<std::vector<libjaguar::Matrix<float, 2, 2>>>("mat2x2_list", std::vector<libjaguar::Matrix<float, 2, 2>> {mat});

	//all fields set; now query to confirm
	assert(doc.QueryValue<int32_t>("int32") == 42);
	auto int32list = doc.QueryValue<std::vector<int32_t>>("int32_list");
	assert(int32list.size() == 3 && int32list[1] == 2);

	assert(doc.QueryValue<std::string>("string") == std::string("hello"));
	auto strlist = doc.QueryValue<std::vector<std::string>>("string_list");
	assert(strlist.size() == 2 && strlist[0] == "a");

	return 0;
}
