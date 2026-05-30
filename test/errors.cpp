#include <libjaguar/Document.hpp>
#include <iostream>
#include <libjaguar/TypeTags.hpp>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

int main() {
	try {
		libjaguar::Document doc;
		//duplicate path
		doc.SetOrCreateValue<int32_t>("dup", 1);
		try {
			doc.CreateValue<int32_t>("dup");
			return -1;
		} catch(const std::runtime_error&) { /* ok */
		}
		//non-existent query
		try {
			doc.QueryValue<int32_t>("none");
			return -1;
		} catch(const std::runtime_error&) { /* ok */
		}
		//wrong type query
		doc.SetOrCreateValue<int32_t>("num", 5);
		try {
			doc.QueryValue<float>("num");
			return -1;
		} catch(const std::runtime_error&) { /* ok */
		}
		//list element mismatch
		doc.CreateValue<int32_t>("list");
		std::vector<double> badlist = {1.0, 2.0};
		try {
			doc.SetValue<std::vector<double>>("list", badlist);
			return -1;
		} catch(const std::runtime_error&) { /* ok */
		}
		return 0;
	} catch(...) { return -1; }
}
