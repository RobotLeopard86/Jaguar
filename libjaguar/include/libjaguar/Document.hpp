#pragma once

#include "DllHelper.hpp"
#include "Reader.hpp"

#include <functional>
#include <istream>
#include <ostream>
#include <type_traits>

namespace libjaguar {
	/**
	 * @brief A user-friendly interface for reading, modifying, and writing Jaguar data
	 */
	class LJAPI Document {
	  public:
		/**
		 * @brief Create a document sourcing initial data from an input stream
		 *
		 * @tparam T The stream type
		 *
		 * @param stream The stream to read from
		 */
		template<typename T>
			requires std::is_base_of_v<std::istream, T> && std::is_move_constructible_v<T>
		Document(T&& stream);

		/**
		 * @brief Load all values from the Jaguar stream into memory
		 *
		 * @warning If the stream contains large buffer objects or just a lot of values in general, this may cause high memory usage
		 *
		 * @throws std::runtime_error If the document has no backing input stream
		 */
		void Materialize();

		/**
		 * @brief Register a converter for a structured object type
		 *
		 * @tparam T The type to convert to/from
		 *
		 * @param dec A function to convert from Jaguar representation to a T object (TODO: Actual intake type)
		 * @param enc A function to convert from a T object to Jaguar representation (TODO: Actual output type)
		 *
		 * @throws If a converter has already been registered for T
		 */
		template<typename T>
		void RegisterStructuredObjConverter(std::function<T()> dec, std::function<void(T)>);

	  private:
		Reader reader;
		///@cond
		enum class RState {
			Unused,
			OK,
			Bad
		} rstate;
		///@endcond
	};
}