#pragma once

#include <cstdint>

namespace libjaguar {
	/**
	 * @brief All valid type specifiers in a Jaguar stream
	 */
	enum class TypeTag : uint8_t {
		String = 0x0A,				 ///<UTF-8 string
		ByteBuffer = 0x0B,			 ///<Raw bytes blob
		Substream = 0x0C,			 ///<Embedded independent Jaguar stream
		Boolean = 0x0D,				 ///<True/false (bool)
		Float32 = 0x0E,				 ///<Single-precision (32-bit) IEEE 754 LE floating-point number (float)
		Float64 = 0x0F,				 ///<Double-precision (64-bit) IEEE 754 LE floating-point number (double)
		SInt8 = 0x1A,				 ///<8-bit signed integer (int8_t)
		SInt16 = 0x1B,				 ///<16-bit signed integer (int16_t)
		SInt32 = 0x1C,				 ///<32-bit signed integer (int32_t)
		SInt64 = 0x1D,				 ///<64-bit signed integer (int64_t)
		UInt8 = 0x2A,				 ///<8-bit unsigned integer (uint8_t)
		UInt16 = 0x2B,				 ///<16-bit unsigned integer (uint16_t)
		UInt32 = 0x2C,				 ///<32-bit unsigned integer (uint32_t)
		UInt64 = 0x2D,				 ///<64-bit unsigned integer (uint64_t)
		List = 0x3A,				 ///<List of other values
		UnstructuredObj = 0x3B,		 ///<Object with no predefined layout, dictionary
		StructuredObj = 0x3C,		 ///<Object with predefined layout
		StructuredObjTypeDecl = 0x3D,///<Declaration of an object type layout
		ScopeBoundary = 0x3E,		 ///<End of object scope marker
		Vector = 0x4A,				 ///<2, 3, or 4-component vector of numbers
		Matrix = 0x4B				 ///<Matrix of numbers, size from 2x2 to 4x4
	};
}