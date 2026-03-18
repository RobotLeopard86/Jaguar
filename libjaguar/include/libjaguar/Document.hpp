#pragma once

#include "DllHelper.hpp"
#include "Reader.hpp"
#include "libjaguar/Index.hpp"

#include <any>
#include <functional>
#include <istream>
#include <ostream>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

namespace libjaguar {
	/**
	 * @brief A user-friendly interface for reading, modifying, and writing Jaguar data
	 */
	class LJAPI Document {
	  public:
		/**
		 * @brief Create an empty initial document
		 */
		Document()
		  : reader(), readerUsed(false) {}

		/**
		 * @brief Create a document sourcing initial data from an input stream
		 *
		 * @tparam T The stream type
		 *
		 * @param stream The stream to read from
		 */
		template<typename T>
			requires std::is_base_of_v<std::istream, T> && std::is_move_constructible_v<T>
		static Document InitFromStream(T&& stream) {
			std::unique_ptr<T> streamPtr = std::make_unique<T>(std::move(stream));
			Document doc {};
			doc.reader = Reader(std::move(streamPtr));
			doc.readerUsed = true;
			return doc;
		}

		/**
		 * @brief Load all values from the Jaguar stream into memory
		 *
		 * @warning If the stream contains large buffer objects or just a lot of values in general, this may result in high memory usage
		 *
		 * @throws std::runtime_error If the document has no backing input stream
		 */
		void Materialize();

		/**
		 * @brief Export the contents of the document to a Jaguar stream
		 *
		 * @param out The output stream to write to
		 *
		 * @throws std::runtime_error If document encoding fails
		 */
		void ExportTo(std::ostream& out);

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
		void RegisterStructuredObjConverter(std::function<T()> dec, std::function<void(T)> enc);

		/**
		 * @brief Query type information about a value field in the document by its path
		 *
		 * @param path The full path of the field to request
		 *
		 * @return The type data stored for this field
		 *
		 * @throws std::runtime_error If no value field exists in the document with the given path
		 */
		template<typename T>
		const ValueEntry QueryTypeinfo(const std::string& path);

		/**
		 * @brief Query type information about a scope field in the document by its path
		 *
		 * @param path The full path of the field to request
		 *
		 * @return The type data stored for this field
		 *
		 * @throws std::runtime_error If no scope field exists in the document with the given path
		 */
		template<typename T>
		const ScopeEntry QueryTypeinfo(const std::string& path);

		/**
		 * @brief Query the value of a field in the document by its path
		 *
		 * @note You can only request "end values" (primitives like buffers, numbers, and math types, as well as structured objects and lists) using this method. This is to avoid requiring a recursive node structure.
		 *
		 * @tparam T The object type to return the Jaguar data as; must match the type declared in the index
		 *
		 * @param path The full path of the field to request
		 *
		 * @return The current value stored in that field
		 *
		 * @throws std::runtime_error If no field exists in the document with the given path and type
		 */
		template<typename T>
		T QueryValue(const std::string& path);

		/**
		 * @brief Set the value of a field to the document at a given path (created if nonexistent)
		 *
		 * @tparam T The object type to be converted to Jaguar data; must be able to be converted to a Jaguar type for storage
		 *
		 * @param path The full path of the field to modify
		 * @param value The data to store in the field
		 *
		 * @throws std::runtime_error If the provided path references nonexistent scopes
		 * @throws std::runtime_error If the provided type does not match an existing value at the same path
		 */
		template<typename T>
		void SetValue(const T& value);

	  private:
		//Reading data
		std::optional<Reader> reader;
		bool readerUsed;

		//Storage
		Index index;
		std::unordered_map<std::type_index, std::pair<std::function<std::any()>, std::function<std::any()>>> converters;
		//TODO: Value storage
	};
}