#pragma once

#include <cstdint>

namespace libjaguar {
	/**
	 * @brief All valid type specifiers in a Jaguar stream
	 */
	enum class TypeTag : uint8_t {
		String = 0x0A,
		ByteBuffer = 0x0B,
		Substream = 0x0C,
		Boolean = 0x0D,
		Float32 = 0x0E,
		Float64 = 0x0F,
		SInt8 = 0x1A,
		SInt16 = 0x1B,
		SInt32 = 0x1C,
		SInt64 = 0x1D,
		UInt8 = 0x2A,
		UInt16 = 0x2B,
		UInt32 = 0x2C,
		UInt64 = 0x2D,
		List = 0x3A,
		UnstructuredObj = 0x3B,
		StructuredObj = 0x3C,
		StructuredObjTypeDecl = 0x3D,
		ScopeBoundary = 0x3E,
		Vector = 0x4A,
		Matrix = 0x4B
	};
}