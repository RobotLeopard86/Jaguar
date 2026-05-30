#include <libjaguar/Document.hpp>
#include <libjaguar/TypeTags.hpp>
#include <libjaguar/StructuredTypeLayout.hpp>

#include <cassert>
#include <vector>

struct WholeLottaFields {
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
	int8_t s8;
	int16_t s16;
	int32_t s32;
	int64_t s64;
	float f;
	double d;
	bool b;
	libjaguar::Vector<float, 2> f2;
	libjaguar::Vector<float, 3> f3;
	libjaguar::Vector<float, 4> f4;
	libjaguar::Vector<unsigned int, 2> u32_2;
	libjaguar::Vector<unsigned int, 3> u32_3;
	libjaguar::Vector<unsigned int, 4> U32_4;
	libjaguar::Vector<double, 2> d2;
	libjaguar::Vector<double, 3> d3;
	libjaguar::Vector<double, 4> d4;
	libjaguar::Vector<int16_t, 2> s16_2;
	libjaguar::Vector<int16_t, 3> s16_3;
	libjaguar::Vector<int16_t, 4> s16_4;
	libjaguar::Matrix<double, 2, 4> d2x4;
};

int main() {
	using namespace libjaguar;
	Document doc;

	//Build layout for WholeLottaFields
	StructuredTypeLayout layout;
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "u8";
		f.type = TypeTag::UInt8;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "u16";
		f.type = TypeTag::UInt16;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "u32";
		f.type = TypeTag::UInt32;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "u64";
		f.type = TypeTag::UInt64;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "s8";
		f.type = TypeTag::SInt8;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "s16";
		f.type = TypeTag::SInt16;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "s32";
		f.type = TypeTag::SInt32;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "s64";
		f.type = TypeTag::SInt64;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "f";
		f.type = TypeTag::Float32;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "d";
		f.type = TypeTag::Float64;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "b";
		f.type = TypeTag::Boolean;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "f2";
		f.type = TypeTag::Vector;
		f.elementType = TypeTag::Float32;
		f.width = 2;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "f3";
		f.type = TypeTag::Vector;
		f.elementType = TypeTag::Float32;
		f.width = 3;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "f4";
		f.type = TypeTag::Vector;
		f.elementType = TypeTag::Float32;
		f.width = 4;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "d2";
		f.type = TypeTag::Vector;
		f.elementType = TypeTag::Float64;
		f.width = 2;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "d3";
		f.type = TypeTag::Vector;
		f.elementType = TypeTag::Float64;
		f.width = 3;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "d4";
		f.type = TypeTag::Vector;
		f.elementType = TypeTag::Float64;
		f.width = 4;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "s16_2";
		f.type = TypeTag::Vector;
		f.elementType = TypeTag::SInt16;
		f.width = 2;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "s16_3";
		f.type = TypeTag::Vector;
		f.elementType = TypeTag::SInt16;
		f.width = 3;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "s16_4";
		f.type = TypeTag::Vector;
		f.elementType = TypeTag::SInt16;
		f.width = 4;
	}
	{
		StructuredTypeLayout::Field& f = layout.fields.emplace_back();
		f.name = "d2x4";
		f.type = TypeTag::Matrix;
		f.elementType = TypeTag::Float64;
		f.width = 2;
		f.height = 4;
	}

	//dummy decoder and encoder
	auto dec = [](Document::ObjReader&) { return WholeLottaFields {}; };
	auto enc = [](const WholeLottaFields&, Document::ObjWriter&) {};

	doc.RegisterStructuredObjConverter<WholeLottaFields>("WholeLottaFields", layout, dec, enc);

	doc.CreateValue<WholeLottaFields>("lottafields");
	doc.CreateValue<std::vector<WholeLottaFields>>("evenmorefields");

	return 0;
}
