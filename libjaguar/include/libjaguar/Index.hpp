#pragma once

#include "DllHelper.hpp"
#include "StructuredTypeLayout.hpp"
#include "TypeTags.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>

namespace libjaguar {
	/**
	 * @brief A base entry in the Index
	 */
	struct LJAPI Entry {
		std::string name;				   ///<Item name
		uint64_t id;					   ///<Internal reference ID derived from path data
		std::streampos streamBeginPosition;///<Location in the stream where the node begins
	};

	/**
	 * @brief An index entry representing a value
	 */
	struct LJAPI ValueEntry : public Entry {
		TypeTag type;		///<Type of value
		TypeTag elementType;///<Type of contained elements (for vectors, matrices, and lists)
		uint32_t size;		///<Number of elements in a list, or size of a buffer object (string, byte buffer, substream); string size must be less than 24-bit integer limit
		uint8_t width;		///<Number of components in a vector or columns in a matrix
		uint8_t height;		///<Number of rows in a matrix
		std::string typeID; ///<Structured object type ID (for a list with a structured object element type)
	};

	/**
	 * @brief An index entry representing a new scope
	 */
	struct LJAPI ScopeEntry : public Entry {
		std::string typeID;				  ///<Type ID for a structured object (leave empty to denote unstructured)
		std::vector<ScopeEntry> subscopes;///<Child scope list
		std::vector<ValueEntry> subvalues;///<Child value list
	};

	/**
	 * @brief An index describing the structure of the Jaguar stream
	 */
	struct LJAPI Index {
		std::unordered_map<std::string, StructuredTypeLayout> types;///<List of recognized structured object types
		ScopeEntry root;											///<Root scope entry
	};
}