#include <libjaguar/Document.hpp>
#include <string>
#include <sstream>

int main() {
	try {
		libjaguar::Document doc;
		//invalid UTF-8 string (0xFF is not valid start)
		std::string bad = "\xFF";
		doc.SetOrCreateValue<std::string>("bad", bad);
		std::stringstream ss;
		doc.ExportTo(ss);//should throw
		return -1;
	} catch(const std::runtime_error&) {
		return 0;//expected
	} catch(...) {
		return -1;
	}
}
