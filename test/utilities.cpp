#include "libjaguar/Index.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/StructuredTypeLayout.hpp"
#include <cassert>
#include <stdexcept>

using namespace libjaguar;

int main() {
	try {
		//GenIndexID invalid UTF-8
		try {
			libjaguar::GenIndexID("\xC0\x00");
			return -1;
		} catch(const std::runtime_error&) {}

		//CalcValueSize tests
		assert(CalcValueSize(TypeTag::String, {}, 5) == 5);
		try {
			CalcValueSize(TypeTag::String, {}, (1u << 24));
			return -1;
		} catch(const std::runtime_error&) {}
		assert(CalcValueSize(TypeTag::ByteBuffer, {}, 10) == 10);
		assert(CalcValueSize(TypeTag::SInt8, {}, 0) == 1);
		assert(CalcValueSize(TypeTag::UInt16, {}, 0) == 2);
		assert(CalcValueSize(TypeTag::Float32, {}, 0) == 4);
		assert(CalcValueSize(TypeTag::SInt64, {}, 0) == 8);
		//Vector of Float32: width=3, height=1 - expected 12 per implementation
		MathTypeDescriptor vec {3, 1, TypeTag::Float32};
		assert(CalcValueSize(TypeTag::Vector, vec, 0) == 12);
		//Matrix of Float32: 2x2 - 64
		MathTypeDescriptor mat {2, 2, TypeTag::Float32};
		assert(CalcValueSize(TypeTag::Matrix, mat, 0) == 16);
		//Invalid vector height
		MathTypeDescriptor vecInvalid {3, 2, TypeTag::Float32};
		assert(CalcValueSize(TypeTag::Vector, vecInvalid, 0) == 0);
		//Invalid matrix height <2
		MathTypeDescriptor matLow {2, 1, TypeTag::Float32};
		assert(CalcValueSize(TypeTag::Matrix, matLow, 0) == 0);
		//Invalid matrix width <2
		MathTypeDescriptor matThin {1, 2, TypeTag::Float32};
		assert(CalcValueSize(TypeTag::Matrix, matThin, 0) == 0);
		//Non-value type returns 0
		assert(CalcValueSize(TypeTag::StructuredObj, {}, 0) == 0);

		//StructuredTypeLayout equality tests
		StructuredTypeLayout a, b;
		StructuredTypeLayout::Field f {TypeTag::UInt32, "foo", TypeTag::UInt32, TypeTag::UInt32, "", 0, 0};
		a.fields.push_back(f);
		b.fields.push_back(f);
		assert(a == b);
		//Modify b - inequality
		b.fields[0].name = "bar";
		assert(!(a == b));
		//Invalid layout: duplicate field names
		StructuredTypeLayout invalid;
		StructuredTypeLayout::Field dup1 {TypeTag::UInt32, "dup", TypeTag::UInt32, TypeTag::UInt32, "", 0, 0};
		StructuredTypeLayout::Field dup2 {TypeTag::UInt32, "dup", TypeTag::UInt32, TypeTag::UInt32, "", 0, 0};
		invalid.fields.push_back(dup1);
		invalid.fields.push_back(dup2);
		//a == a should be false because layout invalid
		assert(!(invalid == invalid));

		return 0;
	} catch(...) {
		return -1;
	}
}
