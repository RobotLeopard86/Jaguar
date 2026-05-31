#include "libjaguar/Document.hpp"
#include "libjaguar/StructuredTypeLayout.hpp"
#include "libjaguar/MathTypes.hpp"
#include "libjaguar/TypeTags.hpp"
#include <sstream>

struct Point {
	int32_t x;
	int32_t y;
};

int main() {
	try {
		libjaguar::Document doc;
		//layout for Point
		libjaguar::StructuredTypeLayout layout;
		libjaguar::StructuredTypeLayout::Field xf;
		xf.type = libjaguar::TypeTag::SInt32;
		xf.name = "x";
		layout.fields.push_back(xf);
		libjaguar::StructuredTypeLayout::Field yf;
		yf.type = libjaguar::TypeTag::SInt32;
		yf.name = "y";
		layout.fields.push_back(yf);
		//register converter
		doc.RegisterStructuredObjConverter<Point>(
			"point",
			layout,
			[](libjaguar::Document::ObjReader& reader) {
				Point p;
				p.x = reader.Query<int32_t>("x");
				p.y = reader.Query<int32_t>("y");
				return p;
			},
			[](const Point& p, libjaguar::Document::ObjWriter& writer) {
				writer.Set<int32_t>("x", p.x);
				writer.Set<int32_t>("y", p.y);
			});
		//create instance
		Point pt {10, 20};
		doc.SetOrCreateValue<Point>("point", pt);
		std::vector<Point> pts = {
			{3, 5},
			{1, 6},
			{2, -9}};
		doc.SetOrCreateValue<std::vector<Point>>("many_points", pts);
		//serialize
		std::stringstream ss;
		doc.ExportTo(ss);
		std::string data = ss.str();
		auto iss = std::make_unique<std::istringstream>(data);
		libjaguar::Document d2(std::move(iss));
		d2.RegisterStructuredObjConverter<Point>(
			"point",
			layout,
			[](libjaguar::Document::ObjReader& reader) {
				Point p;
				p.x = reader.Query<int32_t>("x");
				p.y = reader.Query<int32_t>("y");
				return p;
			},
			[](const Point& p, libjaguar::Document::ObjWriter& writer) {
				writer.Set<int32_t>("x", p.x);
				writer.Set<int32_t>("y", p.y);
			});
		Point p2 = d2.QueryValue<Point>("point");
		if(p2.x != pt.x || p2.y != pt.y) return -1;
		std::vector<Point> pts2 = d2.QueryValue<std::vector<Point>>("many_points");
		if(pts2[0].x != pts[0].x || pts2[0].y != pts[0].y || pts2[1].x != pts[1].x || pts2[1].y != pts[1].y || pts2[2].x != pts[2].x || pts2[2].y != pts[2].y) return -1;
		return 0;
	} catch(...) {
		return -1;
	}
}
