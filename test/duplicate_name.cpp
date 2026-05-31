#include "libjaguar/Document.hpp"

int main() {
	try {
		libjaguar::Document doc;
		doc.SetOrCreateValue<int32_t>("dup", 1);
		//attempt duplicate
		doc.CreateValue<int32_t>("dup");
		return -1;//should not reach
	} catch(const std::runtime_error& e) {
		//expected
		return 0;
	} catch(...) {
		return -1;
	}
}
