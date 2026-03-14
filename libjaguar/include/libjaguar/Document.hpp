#pragma once

#include "DllHelper.hpp"
#include "Reader.hpp"

#include <istream>
#include <ostream>
#include <type_traits>

namespace libjaguar {
	/**
	 * @brief A user-friendly interface for reading, modifying, and writing Jaguar data
	 */
	class LJAPI Document {
	  public:
		template<typename T>
			requires std::is_base_of_v<std::istream, T> && std::is_move_constructible_v<T>
		Document(T&& stream);

	  private:
		Reader reader;
		bool readerValid = true;
	};
}