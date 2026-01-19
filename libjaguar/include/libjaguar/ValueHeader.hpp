#pragma once

#include "DllHelper.hpp"
#include "TypeTags.hpp"

#include <string>

namespace libjaguar {
	/**
	 * @brief Header for a Jaguar value, declaring metadata needed to read the contents
	 */
	struct LJAPI ValueHeader {
		///@name Generic data for all headers (the "value identifier")
		///@{
		TypeTag type;	 ///<The type of the value
		std::string name;///<UTF-8 encoded field name

		///@}

		///@name Type-specific data
		///@{
		TypeTag elementType;///<Type of contained element (for vectors, matrices, and lists)
		uint32_t size;		///<Number of elements in a list, or size of a buffer object (string, byte buffer, substream); string size must be less than 24-bit integer limit
		uint8_t width;		///<Number of components in a vector or columns in a matrix
		uint8_t height;		///<Number of rows in a matrix
		uint16_t fieldCount;///<Number of fields in an unstructured object or a structured object type declaration
		std::string typeID; ///<Structured object type ID (for freestanding structured object or list with a structured object element type)

		///@}
	};
}