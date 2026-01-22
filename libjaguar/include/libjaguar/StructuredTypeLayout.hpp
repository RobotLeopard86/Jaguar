#pragma once

#include "DllHelper.hpp"
#include "TypeTags.hpp"

#include <string>
#include <vector>

namespace libjaguar {
	/**
	 * @brief Layout description of a structured object type
	 */
	struct LJAPI StructuredTypeLayout {
		/**
		 * @brief Description of a field in a type layout
		 */
		struct LJAPI Field {
			///@name Generic properties for all fields
			///@{
			TypeTag type;	 ///<The type of the value (may not be scope boundary or type declaration)
			std::string name;///<UTF-8 encoded field name

			///@}

			///@name Type-specific properties
			///@{
			TypeTag elementType;	  ///<Type of contained element (for vectors, matrices, and lists)
			std::string elementTypeID;///<Type ID for a structured object or a list containing structured objects
			uint8_t width;			  ///<Number of components in a vector or columns in a matrix
			uint8_t height;			  ///<Number of rows in a matrix

			///@}
		};

		std::string typeID;		  ///<Type name (UTF-8 encoded)
		std::vector<Field> fields;///<List of fields
	};

	/**
	 * @brief Check if the provided type layout is valid
	 */
	bool ValidateTypeLayout(const StructuredTypeLayout& layout);
}