#include "libjaguar/Document.hpp"
#include "libjaguar/StructuredTypeLayout.hpp"
#include <sstream>
#include "libjaguar/TypeTags.hpp"
#include <vector>

struct Inner {
	int32_t a;
};

struct Outer {
	Inner inner;
	std::string name;
};

int main() {
	try {
		libjaguar::Document doc;
		//inner layout
		libjaguar::StructuredTypeLayout innerLay;
		libjaguar::StructuredTypeLayout::Field af;
		af.name = "a";
		af.type = libjaguar::TypeTag::SInt32;
		innerLay.fields.push_back(af);
		//register inner
		doc.RegisterStructuredObjConverter<Inner>(
			"inner",
			innerLay,
			[](libjaguar::Document::ObjReader& r) {
                Inner i; i.a = r.Query<int32_t>("a"); return i; },
			[](const Inner& i, libjaguar::Document::ObjWriter& w) { w.Set<int32_t>("a", i.a); });
		//outer layout
		libjaguar::StructuredTypeLayout outerLay;
		libjaguar::StructuredTypeLayout::Field inf;
		inf.name = "inner";
		inf.type = libjaguar::TypeTag::StructuredObj;
		inf.typeID = "inner";
		outerLay.fields.push_back(inf);
		libjaguar::StructuredTypeLayout::Field nf;
		nf.name = "name";
		nf.type = libjaguar::TypeTag::String;
		outerLay.fields.push_back(nf);
		doc.RegisterStructuredObjConverter<Outer>(
			"outer",
			outerLay,
			[](libjaguar::Document::ObjReader& r) {
                Outer o; o.inner = r.Query<Inner>("inner"); o.name = r.Query<std::string>("name"); return o; },
			[](const Outer& o, libjaguar::Document::ObjWriter& w) {w.Set<Inner>("inner", o.inner); w.Set<std::string>("name", o.name); });
		//create outer instance
		Outer oo {{42}, "test"};
		doc.SetOrCreateValue<Outer>("outer", oo);
		//serialize
		std::stringstream ss;
		doc.ExportTo(ss);
		std::string data = ss.str();
		auto iss = std::make_unique<std::istringstream>(data);
		libjaguar::Document d2(std::move(iss));
		d2.RegisterStructuredObjConverter<Inner>(
			"inner",
			innerLay,
			[](libjaguar::Document::ObjReader& r) {
                Inner i; i.a = r.Query<int32_t>("a"); return i; },
			[](const Inner& i, libjaguar::Document::ObjWriter& w) { w.Set<int32_t>("a", i.a); });
		d2.RegisterStructuredObjConverter<Outer>(
			"outer",
			outerLay,
			[](libjaguar::Document::ObjReader& r) {
                Outer o; o.inner = r.Query<Inner>("inner"); o.name = r.Query<std::string>("name"); return o; },
			[](const Outer& o, libjaguar::Document::ObjWriter& w) {w.Set<Inner>("inner", o.inner); w.Set<std::string>("name", o.name); });
		Outer res = d2.QueryValue<Outer>("outer");
		if(res.inner.a != oo.inner.a || res.name != oo.name) return -1;
		return 0;
	} catch(...) {
		return -1;
	}
}
