#pragma once

#include "DllHelper.hpp"
#include "MathTypes.hpp"
#include "Reader.hpp"
#include "Index.hpp"

#include <any>
#include <functional>
#include <istream>
#include <ostream>
#include <optional>
#include <typeindex>
#include <unordered_map>

namespace libjaguar {
	/**
	 * @brief A lazy interface for reading, modifying, and writing Jaguar data
	 *
	 * <b>This class is move-only!</b>
	 */
	class LJAPI Document {
	  public:
		/**
		 * @brief Create an empty initial document
		 */
		Document()
		  : reader(), streamState(StreamState::NoStream), index(std::nullopt) {}

		/**
		 * @brief Create a document sourcing initial data from an input stream
		 *
		 * @param stream The stream to read from
		 */
		Document(std::unique_ptr<std::istream>&& stream)
		  : reader(std::make_optional<Reader>(std::move(stream))), streamState(StreamState::Available), index(Index {}) {}

		///@cond
		Document(const Document&) = delete;
		Document& operator=(const Document&) = delete;
		Document(Document&&);
		Document& operator=(Document&&);
		///@endcond

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
		 * @brief A utility class for structured object converters to read values
		 */
		struct ObjReader {
		  public:
			/**
			 * @brief Get a value from the document
			 *
			 * @tparam T The object type to return the Jaguar data as; must match the type declared in the index
			 *
			 * @param id The ID of the field to get
			 *
			 * @return The current value stored in that field
			 *
			 * @throws std::runtime_error If no field exists in the document with the given ID and type or it is out-of-bounds for the scope
			 */
			template<typename T>
			T Get(uint64_t id) const;

		  private:
			ObjReader() {}
			friend class Document;

			Document* doc;
		};

		/**
		 * @brief A utility class for structured object converters to write values
		 */
		struct ObjWriter {
		  public:
			/**
			 * @brief Set a value in the object scope
			 *
			 * @tparam T The object type from which to derive the Jaguar data; must match the type declared in the index
			 *
			 * @param id The ID of the field to set
			 * @param value The value to store in the field
			 *
			 * @throws std::runtime_error If a field exists in the document with a type that does not match
			 */
			template<typename T>
			void Set(uint64_t id, const T& value);

		  private:
			ObjWriter() {}
			friend class Document;

			Document* doc;
		};

		/**
		 * @brief Register a converter for a structured object type
		 *
		 * @tparam T The type to convert to/from
		 *
		 * @param typeID The Jaguar type ID of the structured object type
		 * @param dec A function to convert from Jaguar representation to a T object
		 * @param enc A function to convert from a T object to Jaguar representation
		 *
		 * @throws If a converter has already been registered for T
		 */
		template<typename T>
			requires std::is_class_v<T>
		void RegisterStructuredObjConverter(const std::string& typeID, std::function<T(const ScopeEntry&, const ObjReader&)> dec, std::function<void(const T&, ObjWriter&)> enc) {
			if(converters.contains(typeid(T))) throw std::runtime_error("A converter has already been registered for this type!");
			structuredObjTypes[typeid(T)] = typeID;
			converters[typeid(T)] = std::make_pair([dec](const ScopeEntry& s, const ObjReader& o) -> std::any { return std::make_any(std::move(dec(s, o))); }, [enc](const T& t, ObjWriter& o) -> void { enc(std::any_cast<T>(t), o); });
		}

		/**
		 * @brief Query type information about a value field in the document by its path
		 *
		 * @param path The full path of the field to request
		 *
		 * @return The type data stored for this field
		 *
		 * @throws std::runtime_error If no value field exists in the document with the given path
		 */
		const ValueEntry QueryValueInfo(const std::string& path);

		/**
		 * @brief Query type information about a scope field in the document by its path
		 *
		 * @param path The full path of the field to request
		 *
		 * @return The type data stored for this field
		 *
		 * @throws std::runtime_error If no scope field exists in the document with the given path
		 */
		const ScopeEntry QueryScopeInfo(const std::string& path);

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
		void SetValue(const std::string& path, const T& value);

	  private:
		//Reading data
		std::optional<Reader> reader;
		enum class StreamState {
			NoStream,	///No stream is being used
			Unavailable,///A stream was being used but is now gone due to a move or other invalidating operation
			Available	///A stream is being used and is good to go
		} streamState;

		//Storage
		struct ValueStorage {
			bool materialized;		   ///<Whether or not the data has actually been loaded into memory
			std::vector<std::byte> mem;///<In-memory storage
			std::streampos inStream;   ///<Location in stream
		};
		std::optional<Index> index;
		std::unordered_map<std::type_index, std::string> structuredObjTypes;
		std::unordered_map<std::type_index, std::pair<std::function<std::any(const ScopeEntry&, const ObjReader&)>, std::function<void(const std::any&, ObjWriter&)>>> converters;
		std::unordered_map<uint64_t, ValueStorage> storage;

		void _Verify();

		friend struct ObjReader;
		friend struct ObjWriter;

		template<typename T>
		ValueStorage From(const T&) = delete;

		template<number T>
		ValueStorage From(const T& num);

		template<>
		ValueStorage From(const bool& val);

		template<>
		ValueStorage From(const std::string& val);

		template<byte_range T>
		ValueStorage From(const T& range);

		template<number T, uint8_t C>
		ValueStorage From(const Vector<T, C>& vec);

		template<number T, uint8_t W, uint8_t H>
		ValueStorage From(const Matrix<T, W, H>& mat);

		template<typename T>
			requires std::is_class_v<T>
		ValueStorage From(const T& obj);

		template<typename T>
		T To(const ValueStorage&) = delete;

		template<number T>
		T To(const ValueStorage& storage);

		template<>
		bool To(const ValueStorage& storage);

		template<>
		std::string To(const ValueStorage& storage);

		template<byte_range T>
		T To(const ValueStorage& storage);

		template<number T, uint8_t C>
		Vector<T, C> To(const ValueStorage& storage);

		template<number T, uint8_t W, uint8_t H>
		Matrix<T, W, H> To(const ValueStorage& storage);

		template<typename T>
			requires std::is_class_v<T>
		T To(const ValueStorage& storage);

		const ValueStorage& _QueryInternal(const std::string& path);
		void _SetInternal(const std::string& path, const ValueStorage& val);
		std::any _QueryObjInternal(const std::string& path);
		void _SetObjInternal(const std::string& path, const std::any& obj);
	};

	///@cond
	template<typename T>
	concept SingleVal = requires(Document& d, const T& t) {
		d.From<std::template remove_cvref_t<T>>(t);
		d.To(t)->std::template remove_cvref_t<T>;
	};

	template<typename T>
	T Document::QueryValue(const std::string& path) {
		if constexpr(SingleVal<T>) {
			return To<T>(_QueryInternal(path));
		} else {
			if(converters.contains(typeid(T))) {
				return std::any_cast<T>(_QueryObjInternal(path));
			}
			throw std::runtime_error("No valid type registered!");
		}
	}

	template<typename T>
	void Document::SetValue(const std::string& path, const T& value) {
		if constexpr(SingleVal<T>) {
			_SetInternal(path, From<T>(value));
		} else {
			if(converters.contains(typeid(T))) {
				_SetObjInternal(path, std::make_any(value));
			} else
				throw std::runtime_error("No valid type registered!");
		}
	}
	///@endcond
}