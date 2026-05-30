#include <libjaguar/Document.hpp>
#include <vector>
#include <cmath>

int main() {
	try {
		libjaguar::Document doc;
		//int list
		doc.CreateValue<std::vector<int32_t>>("ints");
		std::vector<int32_t> intlist = {1, 2, 3, 4, 5};
		doc.SetValue<std::vector<int32_t>>("ints", intlist);
		auto got = doc.QueryValue<std::vector<int32_t>>("ints");
		if(got != intlist) return -1;
		//float list
		doc.CreateValue<std::vector<float>>("floats");
		std::vector<float> flist = {1.1f, 2.2f, 3.3f};
		doc.SetValue<std::vector<float>>("floats", flist);
		auto gotf = doc.QueryValue<std::vector<float>>("floats");
		for(size_t i = 0; i < flist.size(); ++i) {
			if(std::abs(gotf[i] - flist[i]) > 1e-6f) return -1;
		}
		//vector list
		doc.CreateValue<std::vector<libjaguar::Vector<int64_t, 2>>>("vecs");
		std::vector<libjaguar::Vector<int64_t, 2>> vlist;
		vlist.push_back({.x = 1, .y = 2});
		vlist.push_back({.x = 3, .y = 4});
		doc.SetValue<std::vector<libjaguar::Vector<int64_t, 2>>>("vecs", vlist);
		auto gotv = doc.QueryValue<std::vector<libjaguar::Vector<int64_t, 2>>>("vecs");
		if(gotv.size() != vlist.size()) return -1;
		for(size_t i = 0; i < vlist.size(); ++i) {
			if(gotv[i].x != vlist[i].x || gotv[i].y != vlist[i].y) return -1;
		}
		return 0;
	} catch(...) {
		return -1;
	}
}
