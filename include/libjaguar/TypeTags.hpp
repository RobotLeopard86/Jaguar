#pragma once

#include <cstdint>

#include "DllHelper.hpp"

namespace libjaguar {
	/**
	 * @brief All valid type specifiers in a Jaguar stream
	 */
	enum class TypeTag : uint8_t {
		String = 0x0A,				 ///<UTF-8 string
		ByteBuffer = 0x0B,			 ///<Raw bytes blob
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
		UnstructuredObj = 0x3B,		 ///<Object with no predefined layout (like a dictionary)
		StructuredObj = 0x3C,		 ///<Object with predefined layout
		StructuredObjTypeDecl = 0x3D,///<Declaration of an object type layout
		ScopeBoundary = 0x3E,		 ///<End of object scope marker
		Vector = 0x4A,				 ///<2, 3, or 4-component vector of numbers
		Matrix = 0x4B				 ///<Matrix of numbers, size from 2x2 to 4x4
	};

	/**
	 * @brief Validate that a given byte contains a TypeTag value
	 *
	 * @param tagByte The byte to check
	 *
	 * @return @c true if the byte is really a TypeTag (and thus can be cast to one), @c false otherwise
	 */
	LJAPI inline bool ValidateTypeTag(uint8_t tagByte) {
		if(tagByte < 0x0A || tagByte > 0x4B) return false;
		uint8_t lowerNibble = (tagByte & 0b0000'1111);
		uint8_t upperNibble = (tagByte & 0b1111'0000) >> 4;
		if(lowerNibble < 0xA) return false;
		if((upperNibble == 1 || upperNibble == 2) && lowerNibble > 0xD) return false;
		if(upperNibble == 4 && lowerNibble > 0xB) return false;
		if(tagByte == 0x3F) return false;
		return true;
	}

	/**
	 * @brief Check if a given TypeTag represents a value or a scope
	 *
	 * @param tag The tag to check
	 *
	 * @return @c true if the TypeTag is a value, @c false if it's a scope (including lists)
	 */
	LJAPI inline bool IsValue(TypeTag tag) {
		uint8_t asUint = static_cast<uint8_t>(tag);
		return (tag != TypeTag::ScopeBoundary) && ((asUint >> 4) != 0x3);
	}

	/**
	 * @brief Describes the special paramaters for math type values
	 */
	struct LJAPI MathTypeDescriptor {
		uint8_t width; ///<Number of components in a vector or columns in a matrix
		uint8_t height;///<Number of rows in a matrix (set to 1 for a vector)
		TypeTag type;  ///<Vector/matrix element type (int, float, etc.)
	};

	/**
	 * @brief Calculate the expected size in bytes of a value TypeTag (that is, one for which IsValue returns @c true)
	 *
	 * @param tag The type to analyze
	 * @param mathData The supplementary info about the math types (vector and matrix), if those types are selected
	 * @param buffSize The size of a buffer-type object (string and byte buffer), if those types are selected
	 *
	 * @return The size in bytes, or 0 if the provided type was not a value or the supplementary info is invalid
	 */
	LJAPI uint32_t CalcValueSize(TypeTag tag, MathTypeDescriptor mathData, uint32_t buffSize);
}