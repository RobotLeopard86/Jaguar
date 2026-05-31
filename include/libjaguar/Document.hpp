#pragma once

#include "DllHelper.hpp"
#include "MathTypes.hpp"
#include "Reader.hpp"
#include "Index.hpp"
#include "Traits.hpp"
#include "TypeTags.hpp"
#include "StructuredTypeLayout.hpp"

#include <algorithm>
#include <any>
#include <bit>
#include <cmath>
#include <format>
#include <functional>
#include <istream>
#include <iterator>
#include <ostream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

namespace libjaguar {
	///@cond
	bool CheckUTF8(const std::string_view& string);
	///@endcond

	///@brief An empty class used for template arguments when creating unstructured objects
	struct UnstructuredObjTag {};

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
		  : reader(), streamState(StreamState::NoStream), index(Index {}) {}

		/**
		 * @brief Create a document sourcing initial data from an input stream
		 *
		 * @param stream The stream to read from
		 *
		 * @warning Be sure that the stream being used was opened in binary mode (@c std::ios::binary), or decoding issues may occur
		 */
		Document(std::unique_ptr<std::istream>&& stream)
		  : reader(std::make_optional<Reader>(std::move(stream))), streamState(StreamState::Available), index(std::nullopt) {}

		///@cond
		Document(const Document&) = delete;
		Document& operator=(const Document&) = delete;
		Document(Document&&);
		Document& operator=(Document&&);
		///@endcond

		/**
		 * @brief Load a value from the Jaguar stream into memory
		 *
		 * @warning If the specified object is a large buffer, this may result in high memory usage
		 *
		 * @param path The path of the value to materialize
		 *
		 * @throws std::runtime_error If the document has no backing input stream or the provided value path does not exist
		 */
		void Materialize(const std::string& path) {
			Materialize(GenIndexID(path));
		}

		/**
		 * @brief Load a value from the Jaguar stream into memory
		 *
		 * @warning If the specified object is a large buffer, this may result in high memory usage
		 *
		 * @param id The ID of the value to materialize
		 *
		 * @throws std::runtime_error If the document has no backing input stream or there is no value with the provided ID
		 */
		void Materialize(uint64_t id);

		/**
		 * @brief Load all values from the Jaguar stream into memory
		 *
		 * @warning If the stream contains large buffer objects or just a lot of values in general, this may result in high memory usage
		 *
		 * @throws std::runtime_error If the document has no backing input stream
		 */
		void MaterializeAll();

		/**
		 * @brief Export the contents of the document to a Jaguar stream
		 *
		 * @param out The output stream to write to
		 *
		 * @throws std::runtime_error If document encoding fails
		 */
		void ExportTo(std::ostream& out);

		/**
		 * @brief Release the stream if there is one
		 *
		 * @note Before the stream is released, @c MaterializeAll will be called to ensure that all values stay accessible without the stream
		 *
		 * @return A pointer to the stream object if it is avilable, @c nullptr otherwise. Once returned, the pointer's lifetime is the responsibility of the caller
		 */
		std::istream* ReleaseStream();

		/**
		 * @brief A utility class for structured object converters to read values
		 */
		struct ObjReader {
		  public:
			/**
			 * @brief Query the value of a field in the object scope
			 *
			 * @tparam T The object type to return the Jaguar data as; must match the type declared in the index
			 *
			 * @param field The name of the field to retrieve
			 *
			 * @return The current value stored in that field
			 *
			 * @throws std::runtime_error If no field exists in the object scope with the given ID and type
			 */
			template<typename T>
			T Query(const std::string& field) {
				return doc->QueryValue<T>(basePath + "." + field);
			}

			/**
			 * @brief Check if the object scope contains a field of the given name
			 *
			 * @param field The name of the field whose existence to check
			 *
			 * @return If the field exists
			 */
			bool Has(const std::string& field) {
				return doc->HasValue(basePath + "." + field);
			}

			/**
			 * @brief Query type information about a value field in the object scope by its name
			 *
			 * @param field The name of the field to request
			 *
			 * @return The type data stored for this field
			 *
			 * @throws std::runtime_error If no value field exists in the object scope with the given name
			 */
			const ValueEntry& QueryValueInfo(const std::string& field) {
				return doc->QueryValueInfo(basePath + "." + field);
			}

			/**
			 * @brief Query type information about a scope field in the object scope by its name
			 *
			 * @param field The name of the field to request
			 *
			 * @return The type data stored for this field
			 *
			 * @throws std::runtime_error If no scope field exists in the object scope with the given name
			 */
			const ScopeEntry& QueryScopeInfo(const std::string& field) {
				return doc->QueryScopeInfo(basePath + "." + field);
			}

		  private:
			ObjReader() {}
			friend class Document;

			Document* doc;
			std::string basePath;
		};

		/**
		 * @brief A utility class for structured object converters to write values
		 */
		struct ObjWriter {
		  public:
			/**
			 * @brief Set the value of a field in the object scope
			 *
			 * @tparam T The object type to be converted to Jaguar data; must match the type declared in the index
			 *
			 * @param field The name of the field to set
			 * @param value The value to store in the field
			 *
			 * @throws std::runtime_error If no field exists in the object scope with the given path and type or it is not an "end value"
			 */
			template<typename T>
			void Set(const std::string& field, const T& value) {
				doc->SetValue<T>(basePath + "." + field, value);
			}

			/**
			 * @brief Create a new field in the object scope
			 *
			 * @tparam T The type of data stored in the field; must be able to be converted to a Jaguar type for storage
			 *
			 * @param field The name of the field to create
			 *
			 * @throws std::runtime_error If a field already exists with the same path
			 */
			template<typename T>
				requires(!is_byte_range_v<T>)
			void Create(const std::string& field) {
				doc->CreateValue<T>(basePath + "." + field);
			}

			/**
			 * @brief Create a new field in the object scope
			 *
			 * @tparam T The type of data stored in the field; must be able to be converted to a Jaguar type for storage
			 *
			 * @param field The name of the field to create
			 * @param list If the field is to be represented by a List with element type UInt8 or a ByteBuffer
			 *
			 * @throws std::runtime_error If a field already exists with the same path
			 * @throws std::runtime_error If the path references nonexistent scopes
			 */
			template<byte_range T>
			void Create(const std::string& field, bool list) {
				doc->CreateValue<T>(basePath + "." + field, list);
			}

			/**
			 * @brief Set the value of a field in the object scope, creating if if it does not exist
			 *
			 * @tparam T The type of data stored in the field; must be able to be converted to a Jaguar type for storage
			 *
			 * @param field The name of the field to modify/create
			 * @param value The value to store in the field
			 *
			 * @throws std::runtime_error If the field already exists with non-matching type information
			 * @throws std::runtime_error If the path references nonexistent scopes
			 */
			template<typename T>
				requires(!is_byte_range_v<T>)
			void SetOrCreate(const std::string& field, const T& value) {
				doc->SetOrCreateValue<T>(basePath + "." + field, value);
			}

			/**
			 * @brief Set the value of a field in the object scope, creating if if it does not exist
			 *
			 * @tparam T The type of data stored in the field; must be able to be converted to a Jaguar type for storage
			 *
			 * @param field The name of the field to modify/create
			 * @param list If the field is to be represented by a List with element type UInt8 or a ByteBuffer
			 * @param value The value to store in the field
			 *
			 * @throws std::runtime_error If the field already exists with non-matching type information
			 * @throws std::runtime_error If the path references nonexistent scopes
			 */
			template<byte_range T>
			void SetOrCreate(const std::string& field, bool list, const T& value) {
				doc->SetOrCreateValue<T>(basePath + "." + field, list, value);
			}

		  private:
			ObjWriter() {}
			friend class Document;

			Document* doc;
			std::string basePath;
		};

		/**
		 * @brief Register a converter for a structured object type
		 *
		 * @tparam T The type to convert to/from
		 *
		 * @param typeID The Jaguar type ID of the structured object type
		 * @param layout The description of the structured object's layout in Jaguar stream format
		 * @param dec A function to convert from Jaguar representation to a T object
		 * @param enc A function to convert from a T object to Jaguar representation
		 *
		 * @throws If a converter has already been registered for T
		 */
		template<typename T>
			requires std::is_class_v<T>
		void RegisterStructuredObjConverter(const std::string& typeID, const StructuredTypeLayout& layout, std::function<T(ObjReader&)> dec, std::function<void(const T&, ObjWriter&)> enc) {
			//Wrap decoder/encoder functions and pass them through
			auto dec2 = [dec](ObjReader& o) -> std::any { return std::make_any<T>(std::move(dec(o))); };
			auto enc2 = [enc](const std::any& t, ObjWriter& o) -> void { enc(std::any_cast<T>(t), o); };
			auto cvt = std::make_pair<std::function<std::any(ObjReader&)>, std::function<void(const std::any&, ObjWriter&)>>(dec2, enc2);
			auto createWrap = [this](const std::string& path, bool listOf) { if(listOf) this->CreateValue<std::vector<T>>(path); else this->CreateValue<T>(path); };
			_RegisterConverterInternal(typeID, layout, typeid(T), cvt, createWrap);
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
		const ValueEntry& QueryValueInfo(const std::string& path);

		/**
		 * @brief Query type information about a scope field in the document by its path
		 *
		 * @param path The full path of the field to request
		 *
		 * @return The type data stored for this field
		 *
		 * @throws std::runtime_error If no scope field exists in the document with the given path
		 */
		const ScopeEntry& QueryScopeInfo(const std::string& path);

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
		 * @throws std::runtime_error If no field exists in the document with the given path and type or it is not an "end value"
		 */
		template<typename T>
		T QueryValue(const std::string& path);

		/**
		 * @brief Set the value of a field to the document at a given path
		 *
		 * @tparam T The object type to be converted to Jaguar data; must be able to be converted to a Jaguar type for storage
		 *
		 * @param path The full path of the field to modify
		 * @param value The data to store in the field
		 *
		 * @throws std::runtime_error If no field exists in the document with the given path and type or it is not an "end value"
		 */
		template<typename T>
		void SetValue(const std::string& path, const T& value);

		/**
		 * @brief Create a new field in the document at a given path
		 *
		 * @tparam T The type of data stored in the field; must be able to be converted to a Jaguar type for storage
		 *
		 * @param path The full path of the field to create
		 *
		 * @throws std::runtime_error If a field already exists with the same path
		 */
		template<typename T>
			requires(!is_byte_range_v<T>)
		void CreateValue(const std::string& path);

		/**
		 * @brief Create a new field in the document at a given path
		 *
		 * @tparam T The type of data stored in the field; must be able to be converted to a Jaguar type for storage
		 *
		 * @param path The full path of the field to create
		 * @param list If the field is to be represented by a List with element type UInt8 or a ByteBuffer
		 *
		 * @throws std::runtime_error If a field already exists with the same path
		 * @throws std::runtime_error If the path references nonexistent scopes
		 */
		template<byte_range T>
		void CreateValue(const std::string& path, bool list);

		/**
		 * @brief Set the value of a field in the document at a given path, creating if if it does not exist
		 *
		 * @tparam T The type of data stored in the field; must be able to be converted to a Jaguar type for storage
		 *
		 * @param path The full path of the field to modify/create
		 * @param value The data to store in the field
		 *
		 * @throws std::runtime_error If the field already exists with non-matching type information
		 * @throws std::runtime_error If the path references nonexistent scopes
		 */
		template<typename T>
			requires(!is_byte_range_v<T>)
		void SetOrCreateValue(const std::string& path, const T& value);

		/**
		 * @brief Set the value of a field in the document at a given path, creating if if it does not exist
		 *
		 * @tparam T The type of data stored in the field; must be able to be converted to a Jaguar type for storage
		 *
		 * @param path The full path of the field to modify/create
		 * @param list If the field is to be represented by a List with element type UInt8 or a ByteBuffer
		 * @param value The data to store in the field
		 *
		 * @throws std::runtime_error If the field already exists with non-matching type information
		 * @throws std::runtime_error If the path references nonexistent scopes
		 */
		template<byte_range T>
		void SetOrCreateValue(const std::string& path, bool list, const T& value);

		/**
		 * @brief Check if the document contains a field at the given path
		 *
		 * @param path The full path of the field whose existence to check
		 *
		 * @return If the field exists
		 */
		bool HasValue(const std::string& path);

		/**
		 * @brief Delete a field in the document
		 *
		 * @note You cannot delete individual fields from structured objects or individual items in lists
		 *
		 * @param path The full path of the field to delete
		 *
		 * @throws std::runtime_error If no field exists in the document with the given path or the field is not allowed to be deleted
		 */
		void DeleteValue(const std::string& path);

		///@cond
		struct ValueStorage {
			bool materialized;			   //Whether or not the data has actually been loaded into memory
			std::vector<unsigned char> mem;//In-memory storage
			std::streampos inStream;	   //Location in stream
		};
		///@endcond

	  private:
		//Reading data
		std::optional<Reader> reader;
		enum class StreamState {
			NoStream,	///No stream is being used
			Unavailable,///A stream was being used but is now gone due to a move, all values being materialized, or another invalidating operation
			Available	///A stream is being used and is good to go
		} streamState;

		//Storage
		std::optional<Index> index;
		std::unordered_map<std::string, std::type_index> structuredObjTypes;
		std::unordered_map<std::type_index, std::pair<std::function<std::any(ObjReader&)>, std::function<void(const std::any&, ObjWriter&)>>> converters;
		std::unordered_map<std::type_index, std::function<void(const std::string&, bool)>> createValWraps;
		std::unordered_map<uint64_t, ValueStorage> storage;

		bool _Verify();

		friend struct ObjReader;
		friend struct ObjWriter;

		template<typename T>
		ValueStorage From(const T&) = delete;

		template<number T>
		ValueStorage From(const T& num) {
			with_bits_t<bits_v<T>> work = std::bit_cast<with_bits_t<bits_v<T>>, T>(num);
			std::vector<unsigned char> mem(bits_v<T>);
			for(uint8_t i = 0; i < (bits_v<T> / 8); ++i) {
				mem[i] = static_cast<unsigned char>(work & 0xFF);
				if constexpr(bits_v<T> > 8) work >>= 8;
			}
			return ValueStorage {.materialized = true, .mem = mem, .inStream = 0};
		}

		template<>
		ValueStorage From(const bool& val) {
			ValueStorage vs = {.materialized = true, .mem = std::vector<unsigned char> {1}, .inStream = 0};
			vs.mem[0] = static_cast<unsigned char>(val ? 1 : 0);
			return vs;
		}

		template<>
		ValueStorage From<std::string>(const std::string& val);

		template<byte_range T>
		ValueStorage From(const T& range) {
			return ValueStorage {.materialized = true, .mem = std::vector<unsigned char>(range.begin(), range.end()), .inStream = 0};
		}

		template<vec_c<2> V>
		ValueStorage From(const V& vec) {
			using T = vec_subtype_t<V>;
			ValueStorage vs {.materialized = true, .mem = std::vector<unsigned char>(bits_v<T> / 4), .inStream = 0};
			with_bits_t<bits_v<T>> x = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.x);
			for(uint8_t i = 0; i < (bits_v<T> / 8); ++i) {
				vs.mem[i] = (x & 0xFF);
				x >>= 8;
			}
			with_bits_t<bits_v<T>> y = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.y);
			for(uint8_t i = 0; i < (bits_v<T> / 8); ++i) {
				vs.mem[(bits_v<T> / 8) + i] = (y & 0xFF);
				y >>= 8;
			}
			return vs;
		}

		template<vec_c<3> V>
		ValueStorage From(const V& vec) {
			using T = vec_subtype_t<V>;
			ValueStorage vs {.materialized = true, .mem = std::vector<unsigned char>(bits_v<T> / 8 * 3), .inStream = 0};
			with_bits_t<bits_v<T>> x = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.x);
			for(uint8_t i = 0; i < (bits_v<T> / 8); ++i) {
				vs.mem[i] = (x & 0xFF);
				x >>= 8;
			}
			with_bits_t<bits_v<T>> y = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.y);
			for(uint8_t i = 0; i < (bits_v<T> / 8); ++i) {
				vs.mem[(bits_v<T> / 8) + i] = (y & 0xFF);
				y >>= 8;
			}
			with_bits_t<bits_v<T>> z = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.z);
			for(uint8_t i = 0; i < (bits_v<T> / 8); ++i) {
				vs.mem[(bits_v<T> / 4) + i] = (z & 0xFF);
				z >>= 8;
			}
			return vs;
		}

		template<vec_c<4> V>
		ValueStorage From(const V& vec) {
			using T = vec_subtype_t<V>;
			ValueStorage vs {.materialized = true, .mem = std::vector<unsigned char>(bits_v<T> / 2), .inStream = 0};
			with_bits_t<bits_v<T>> x = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.x);
			for(uint8_t i = 0; i < (bits_v<T> / 8); ++i) {
				vs.mem[i] = (x & 0xFF);
				x >>= 8;
			}
			with_bits_t<bits_v<T>> y = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.y);
			for(uint8_t i = 0; i < (bits_v<T> / 8); ++i) {
				vs.mem[(bits_v<T> / 8) + i] = (y & 0xFF);
				y >>= 8;
			}
			with_bits_t<bits_v<T>> z = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.z);
			for(uint8_t i = 0; i < (bits_v<T> / 8); ++i) {
				vs.mem[(bits_v<T> / 4) + i] = (z & 0xFF);
				z >>= 8;
			}
			with_bits_t<bits_v<T>> w = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.w);
			for(uint8_t i = 0; i < (bits_v<T> / 8); ++i) {
				vs.mem[(bits_v<T> / 8) * 3 + i] = (w & 0xFF);
				w >>= 8;
			}
			return vs;
		}

		template<mat M>
		ValueStorage From(const M& mat) {
			constexpr uint8_t W = mat_width_v<M>;
			constexpr uint8_t H = mat_height_v<M>;
			using T = mat_subtype_t<M>;
			ValueStorage vs {.materialized = true, .mem = std::vector<unsigned char>(bits_v<T> * W * H / 8), .inStream = 0};
			for(uint8_t x = 0; x < W; ++x) {
				for(uint8_t y = 0; y < H; ++y) {
					with_bits_t<bits_v<T>> val = std::bit_cast<with_bits_t<bits_v<T>>, T>(mat[x][y]);
					for(uint8_t i = 0; i < (bits_v<T> / 8); ++i) {
						vs.mem[(bits_v<T> / 8) * x * H + y + i] = (val & 0xFF);
						val >>= 8;
					}
				}
			}
			return vs;
		}

		template<typename T>
		T To(const ValueStorage&) = delete;

		template<number T>
		T To(const ValueStorage& storage) {
			with_bits_t<bits_v<T>> work = 0;
			for(uint8_t i = 0; i < storage.mem.size(); ++i) work |= (with_bits_t<bits_v<T>>(static_cast<uint8_t>(storage.mem[i]) & 0xFF) << (i * 8));
			return std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
		}

		template<>
		bool To(const ValueStorage& storage) {
			return storage.mem[0] == static_cast<unsigned char>(1);
		}

		template<>
		std::string To<std::string>(const ValueStorage& storage);

		template<byte_range T>
		T To(const ValueStorage& storage) {
			T t;
			if constexpr(resizable_range<T>) {
				std::ranges::copy(storage.mem.begin(), storage.mem.end(), std::back_inserter(t));
			} else {
				if(storage.mem.size() > std::ranges::size(t)) throw std::runtime_error("Storage too small to hold data!");
				std::ranges::copy(storage.mem.begin(), storage.mem.end(), t.begin());
			}
			return t;
		}

		template<vec_c<2> V>
		V To(const ValueStorage& storage) {
			using T = vec_subtype_t<V>;
			V vec {.x = 0, .y = 0};
			with_bits_t<bits_v<T>> work = 0;
			uint8_t component = 0;
			for(uint8_t i = 0; i <= storage.mem.size(); ++i) {
				//Update component data
				if(i != 0 && i % (bits_v<T> / 8) == 0) {
					switch(++component) {
						case 1:
							vec.x = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							break;
						case 2:
							vec.y = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							return vec;
					}
					work = 0;
				}

				//Push latest byte
				if constexpr(bits_v<T> > 8) {
					work |= (with_bits_t<bits_v<T>>(static_cast<uint8_t>(storage.mem[i]) & 0xFF) << (i * 8));
				} else {
					work = (static_cast<uint8_t>(storage.mem[i]) & 0xFF);
				}
			}
			return vec;
		}

		template<vec_c<3> V>
		V To(const ValueStorage& storage) {
			using T = vec_subtype_t<V>;
			V vec {.x = 0, .y = 0, .z = 0};
			with_bits_t<bits_v<T>> work = 0;
			uint8_t component = 0;
			for(uint8_t i = 0; i <= storage.mem.size(); ++i) {
				//Update component data
				if(i != 0 && i % (bits_v<T> / 8) == 0) {
					switch(++component) {
						case 1:
							vec.x = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							break;
						case 2:
							vec.y = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							break;
						case 3:
							vec.z = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							return vec;
					}
					work = 0;
				}

				//Push latest byte
				if constexpr(bits_v<T> > 8) {
					work |= (with_bits_t<bits_v<T>>(static_cast<uint8_t>(storage.mem[i]) & 0xFF) << (i * 8));
				} else {
					work = (static_cast<uint8_t>(storage.mem[i]) & 0xFF);
				}
			}
			return vec;
		}

		template<vec_c<4> V>
		V To(const ValueStorage& storage) {
			using T = vec_subtype_t<V>;
			V vec {.x = 0, .y = 0, .z = 0, .w = 0};
			with_bits_t<bits_v<T>> work = 0;
			uint8_t component = 0;
			for(uint8_t i = 0; i <= storage.mem.size(); ++i) {
				//Update component data
				if(i != 0 && i % (bits_v<T> / 8) == 0) {
					switch(++component) {
						case 1:
							vec.x = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							break;
						case 2:
							vec.y = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							break;
						case 3:
							vec.z = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							break;
						case 4:
							vec.w = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							return vec;
					}
					work = 0;
				}

				//Push latest byte
				if constexpr(bits_v<T> > 8) {
					work |= (with_bits_t<bits_v<T>>(static_cast<uint8_t>(storage.mem[i]) & 0xFF) << (i * 8));
				} else {
					work = (static_cast<uint8_t>(storage.mem[i]) & 0xFF);
				}
			}
			return vec;
		}

		template<mat M>
		M To(const ValueStorage& storage) {
			constexpr uint8_t W = mat_width_v<M>;
			constexpr uint8_t H = mat_height_v<M>;
			using T = mat_subtype_t<M>;
			M mat;
			for(uint8_t x = 0; x < W; ++x) {
				for(uint8_t y = 0; y < H; ++y) {
					with_bits_t<bits_v<T>> val = 0;
					unsigned int bytes = (bits_v<T> / 8);
					for(uint8_t i = 0; i < bytes; ++i) {
						if constexpr(bits_v<T> > 8) {
							val |= (with_bits_t<bits_v<T>>(static_cast<uint8_t>(storage.mem[bytes * (x + 1) * (y + 1) + i]) & 0xFF) << (i * 8));
						} else {
							val = (storage.mem[bytes * (x + 1) * (y + 1) + i] & static_cast<unsigned char>(0xFF));
						}
					}
					mat[x][y] = val;
				}
			}
			return mat;
		}

		template<typename T>
		void VerifyTypeCompatibility(const ValueEntry&) {}
		template<typename T>
			requires(!is_vec_v<T>) && (!is_mat_v<T>)
		void VerifyTypeCompatibility(const ValueEntry& ve);
		template<vec T>
		void VerifyTypeCompatibility(const ValueEntry& ve);
		template<mat T>
		void VerifyTypeCompatibility(const ValueEntry& ve);

		const ValueEntry& _ValInfoInternal(uint64_t id);
		const ScopeEntry& _ScopeInfoInternal(uint64_t id);
		const ValueStorage& _QueryInternal(uint64_t id);
		void _SetInternal(uint64_t id, const ValueStorage& val);
		std::any _QueryObjInternal(const std::string& path, std::type_index type);
		void _SetObjInternal(const std::string& path, const std::any& obj, std::type_index type);
		void _RegisterConverterInternal(const std::string& typeID, const StructuredTypeLayout& layout, const std::type_index& type, const typename decltype(converters)::mapped_type& cvt, std::function<void(const std::string&, bool)> createWrap);

		class DocPayloadProvider;
		friend class DocPayloadProvider;
	};

	///@cond
	template<typename T>
	concept SingleVal = requires(Document& d, const T& t, const Document::ValueStorage& s) {
		{ d.From<std::remove_cvref_t<T>>(t) };
		{ d.To<std::remove_cvref_t<T>>(s) };
	};

	template<typename T>
	T Document::QueryValue(const std::string& path) {
		if(!CheckUTF8(path)) throw std::runtime_error("Invalid UTF-8 supplied as path data to query is not allowed!");

		//List shenanigans
		if constexpr(std::ranges::range<T> && !std::is_same_v<T, std::string>) {
			using S = std::ranges::range_value_t<T>;

			//Byte ranges could be for a byte buffer or a list of UInt8, so we disambiguate by calling QueryValueInfo, which will throw an exception in this case if it's a list
			if constexpr(is_byte_range_v<T>) {
				bool maybeList = [this, &path]() {
					try {
						QueryValueInfo(path);
						return false;
					} catch(...) {
						return true;
					}
				}();
				if(!maybeList) goto qv_reg_handle;//Ahhhh, scary goto
			}

			//Check that we're dealing with a list here
			const ScopeEntry& scope = QueryScopeInfo(path);
			if(!scope.list) throw std::runtime_error("Type incompatibility: List checked against non-matching candidate type!");

			//Setup output container
			T result;
			std::size_t listSz = (scope.subvalues.size() + scope.subscopes.size());
			if constexpr(resizable_range<T>)
				result.resize(listSz);
			else if(std::ranges::size(result) < listSz)
				throw std::runtime_error("Type incompatibility: List requires a candidate storage with sufficient space!");

			//Write each item
			if constexpr(indexable_range<T>) {
				for(uint32_t i = 0; i < scope.subvalues.size(); ++i) {
					S val = QueryValue<S>(std::format("{}[{}]", path, i));
					result[i] = val;
				}
				for(uint32_t i = 0; i < scope.subscopes.size(); ++i) {
					S val = QueryValue<S>(std::format("{}[{}]", path, i));
					result[i] = val;
				}
			} else {
				auto it = std::ranges::begin(result);
				for(uint32_t i = 0; i < scope.subvalues.size(); ++i, ++it) {
					*it = QueryValue<S>(std::format("{}[{}]", path, i));
				}
				for(uint32_t i = 0; i < scope.subscopes.size(); ++i) {
					S val = QueryValue<S>(std::format("{}[{}]", path, i));
					result[i] = val;
				}
			}

			//Return list
			return result;
		}

	//Regular handling
	qv_reg_handle:
		if constexpr(SingleVal<T>) {
			const ValueEntry& ve = QueryValueInfo(path);
			VerifyTypeCompatibility<T>(ve);
			return To<T>(_QueryInternal(GenIndexID(path)));
		} else {
			if(converters.contains(typeid(T))) {
				if(structuredObjTypes.at(_ScopeInfoInternal(GenIndexID(path)).typeID) != typeid(T)) throw std::runtime_error("Structured object type mismatch!");
				return std::any_cast<T>(_QueryObjInternal(path, typeid(T)));
			}
			throw std::runtime_error("No valid type registered!");
		}
	}

	template<typename T>
	void Document::SetValue(const std::string& path, const T& value) {
		if(!CheckUTF8(path)) throw std::runtime_error("Invalid UTF-8 supplied as path data to set is not allowed!");
		if(!HasValue(path)) throw std::runtime_error("Cannot set value of nonexistent field!");

		//List shenanigans
		if constexpr(std::ranges::range<T> && !std::is_same_v<T, std::string>) {
			using S = std::ranges::range_value_t<T>;

			//Byte ranges could be for a byte buffer or a list of UInt8, so we disambiguate by calling QueryValueInfo, which will throw an exception in this case if it's a list
			if constexpr(is_byte_range_v<T>) {
				bool maybeList = [this, &path]() {
					try {
						QueryValueInfo(path);
						return false;
					} catch(...) {
						return true;
					}
				}();
				if(!maybeList) goto sv_reg_handle;//Ahhhh, scary goto
			}

			//Check that we're dealing with a list here
			const ScopeEntry& scope = QueryScopeInfo(path);
			if(!scope.list) throw std::runtime_error("Type incompatibility: List checked against non-matching candidate type!");

			//Set individual elements
			auto it = std::ranges::begin(value);
			for(uint32_t i = 0; it != std::ranges::end(value); ++i, ++it) {
				SetOrCreateValue<S>(std::format("{}[{}]", path, i), *it);
			}
			return;
		}

		//Regular handling
	sv_reg_handle:
		if constexpr(SingleVal<T>) {
			uint64_t id = GenIndexID(path);
			VerifyTypeCompatibility<T>(_ValInfoInternal(id));
			_SetInternal(id, From<T>(value));
		} else {
			if(converters.contains(typeid(T))) {
				if(uint64_t id = GenIndexID(path); HasValue(path) && structuredObjTypes.at(_ScopeInfoInternal(id).typeID) != typeid(T)) throw std::runtime_error("Existing structured object type mismatch!");
				_SetObjInternal(path, std::make_any<T>(value), typeid(T));
			} else
				throw std::runtime_error("No valid type registered!");
		}
	}

	template<byte_range T>
	void Document::CreateValue(const std::string& path, bool list) {
		//Base path checks
		if(!CheckUTF8(path)) throw std::runtime_error("Invalid UTF-8 supplied as path data to set is not allowed!");
		if(HasValue(path)) throw std::runtime_error("Cannot create field that already exists!");

		//Scope checks
		uint64_t id = GenIndexID(path);
		uint64_t parentID = 0;
		std::string fieldName = "";
		if(path.ends_with("]")) {
			if(path[0] == '[') throw std::runtime_error("Cannot index root scope!");
			auto openBracket = path.find_last_of('[');
			std::string parentPath = path.substr(0, openBracket);
			if(!QueryScopeInfo(parentPath).list) throw std::runtime_error("Cannot index non-list!");
			parentID = GenIndexID(parentPath);
			fieldName = path.substr(openBracket + 1, path.size() - 1);
		} else {
			auto lastDot = path.find_last_of('.');
			parentID = (lastDot == std::string::npos) ? index->root.id : GenIndexID(path.substr(0, lastDot));
			fieldName = (parentID == index->root.id) ? path : path.substr(lastDot + 1);
		}
		const auto indexWalk = [parentID](ScopeEntry& entry) -> std::optional<std::reference_wrapper<ScopeEntry>> {
			auto impl = [parentID](ScopeEntry& entry, auto& implRef) mutable -> std::optional<std::reference_wrapper<ScopeEntry>> {
				for(ScopeEntry& scope : entry.subscopes) {
					if(scope.id == parentID) return std::make_optional(std::reference_wrapper<ScopeEntry>(scope));
					if(auto result = implRef(scope, implRef); result.has_value()) return result;
				}
				return std::nullopt;
			};
			return impl(entry, impl);
		};
		auto maybeScope = (parentID == index->root.id) ? index->root : indexWalk(index->root);
		if(!maybeScope.has_value()) throw std::runtime_error("Cannot create field with a nonexistent parent scope!");
		ScopeEntry& parentScope = maybeScope->get();
		if(!parentScope.list && !parentScope.typeID.empty()) {
			StructuredTypeLayout& stl = index->types.at(parentScope.typeID);
			[&stl, &fieldName]() {
				for(const StructuredTypeLayout::Field& field : stl.fields) {
					if(field.name.compare(fieldName) == 0) return;
				}
				throw std::runtime_error("Cannot create field in structured object that is not listed in the type layout!");
			}();
		}

		//Entry addition
		if(list) {
			ScopeEntry scope = {};
			scope.list = true;
			scope.name = fieldName;
			scope.id = id;
			scope.streamBeginPosition = 0;
			scope.listElementType = TypeTag::UInt8;
			parentScope.subscopes.push_back(scope);
		} else {
			//Setup index entry
			ValueEntry value = {};
			value.name = fieldName;
			value.id = id;
			value.streamBeginPosition = 0;
			value.type = TypeTag::ByteBuffer;
			value.size = 0;
			parentScope.subvalues.push_back(value);

			//Setup storage
			ValueStorage vs = {};
			vs.materialized = true;
			vs.mem = std::vector<uint8_t>();
			storage[id] = vs;
		}
	}

	template<typename T>
		requires(!is_byte_range_v<T>)
	void Document::CreateValue(const std::string& path) {
		//Base path checks
		if(!CheckUTF8(path)) throw std::runtime_error("Invalid UTF-8 supplied as path data to set is not allowed!");
		if(HasValue(path)) throw std::runtime_error("Cannot create field that already exists!");

		//Scope checks
		uint64_t id = GenIndexID(path);
		uint64_t parentID = 0;
		std::string fieldName = "";
		if(path.ends_with("]")) {
			if(path[0] == '[') throw std::runtime_error("Cannot index root scope!");
			auto openBracket = path.find_last_of('[');
			std::string parentPath = path.substr(0, openBracket);
			if(!QueryScopeInfo(parentPath).list) throw std::runtime_error("Cannot index non-list!");
			parentID = GenIndexID(parentPath);
			fieldName = path.substr(openBracket + 1, path.size() - openBracket - 2);
		} else {
			auto lastDot = path.find_last_of('.');
			parentID = (lastDot == std::string::npos) ? index->root.id : GenIndexID(path.substr(0, lastDot));
			fieldName = (parentID == index->root.id) ? path : path.substr(lastDot + 1);
		}
		const auto indexWalk = [parentID](ScopeEntry& entry) -> std::optional<std::reference_wrapper<ScopeEntry>> {
			auto impl = [parentID](ScopeEntry& entry, auto& implRef) mutable -> std::optional<std::reference_wrapper<ScopeEntry>> {
				for(ScopeEntry& scope : entry.subscopes) {
					if(scope.id == parentID) return std::make_optional(std::reference_wrapper<ScopeEntry>(scope));
					if(auto result = implRef(scope, implRef); result.has_value()) return result;
				}
				return std::nullopt;
			};
			return impl(entry, impl);
		};
		auto maybeScope = (parentID == index->root.id) ? index->root : indexWalk(index->root);
		if(!maybeScope.has_value()) throw std::runtime_error("Cannot create field with a nonexistent parent scope!");
		ScopeEntry& parentScope = maybeScope->get();
		if(!parentScope.list && !parentScope.typeID.empty()) {
			StructuredTypeLayout& stl = index->types.at(parentScope.typeID);
			[&stl, &fieldName]() {
				for(const StructuredTypeLayout::Field& field : stl.fields) {
					if(field.name.compare(fieldName) == 0) return;
				}
				throw std::runtime_error("Cannot create field in structured object that is not listed in the type layout!");
			}();
		}

		//Make entry
		if constexpr(std::ranges::range<T> && !std::is_same_v<T, std::string>) {
			//Basic setup
			ScopeEntry scope = {};
			scope.list = true;
			scope.name = fieldName;
			scope.id = id;
			scope.streamBeginPosition = 0;

			//Type-specific data
			using S = std::ranges::range_value_t<T>;
			if constexpr(std::ranges::range<S> && !std::is_same_v<S, std::string>) {
				if(is_byte_range_v<S>)
					scope.listElementType = TypeTag::ByteBuffer;
				else
					throw std::runtime_error("Cannot create a list of lists!");
			} else if constexpr(std::is_same_v<S, float>) {
				scope.listElementType = TypeTag::Float32;
			} else if constexpr(std::is_same_v<S, double>) {
				scope.listElementType = TypeTag::Float64;
			} else if constexpr(is_number_v<S>) {
				scope.listElementType = static_cast<TypeTag>(std::log2(bits_v<S> / 8) + 0x1A + (std::is_signed_v<S> ? 0 : 0x10));
			} else if constexpr(std::is_same_v<S, std::string>) {
				scope.listElementType = TypeTag::String;
			} else if constexpr(std::is_same_v<S, bool>) {
				scope.listElementType = TypeTag::Boolean;
			} else if constexpr(vec<S>) {
				scope.listElementType = TypeTag::Vector;
				scope.listMathData = {};
				scope.listMathData.height = 1;
				scope.listMathData.width = vec_count_v<S>;
				using U = vec_subtype_t<S>;
				if constexpr(std::is_same_v<U, float>) {
					scope.listMathData.type = TypeTag::Float32;
				} else if constexpr(std::is_same_v<U, double>) {
					scope.listMathData.type = TypeTag::Float64;
				} else if constexpr(is_number_v<U>) {
					scope.listMathData.type = static_cast<TypeTag>(std::log2(bits_v<U> / 8) + 0x1A + (std::is_signed_v<U> ? 0 : 0x10));
				}
			} else if constexpr(mat<S>) {
				scope.listElementType = TypeTag::Matrix;
				scope.listMathData = {};
				scope.listMathData.width = mat_width_v<S>;
				scope.listMathData.height = mat_height_v<S>;
				using U = mat_subtype_t<S>;
				if constexpr(std::is_same_v<U, float>) {
					scope.listMathData.type = TypeTag::Float32;
				} else if constexpr(std::is_same_v<U, double>) {
					scope.listMathData.type = TypeTag::Float64;
				} else if constexpr(is_number_v<U>) {
					scope.listMathData.type = static_cast<TypeTag>(std::log2(bits_v<U> / 8) + 0x1A + (std::is_signed_v<U> ? 0 : 0x10));
				}
			} else if constexpr(std::is_same_v<S, UnstructuredObjTag>) {
				scope.listElementType = TypeTag::UnstructuredObj;
				scope.typeID = "";
			} else if(converters.contains(typeid(S))) {
				scope.listElementType = TypeTag::StructuredObj;
				scope.typeID = std::find_if(structuredObjTypes.begin(), structuredObjTypes.end(), [](const auto& pair) { return pair.second == typeid(S); })->first;
			} else
				throw std::runtime_error("Invalid type for field creation!");

			//Add to scope
			parentScope.subscopes.push_back(scope);
		} else if constexpr(std::is_same_v<T, UnstructuredObjTag>) {
			ScopeEntry scope = {};
			scope.list = false;
			scope.name = fieldName;
			scope.id = id;
			scope.streamBeginPosition = 0;
			scope.listElementType = TypeTag::UnstructuredObj;
			scope.typeID = "";
			if(parentScope.list && parentScope.listElementType != TypeTag::UnstructuredObj) throw std::runtime_error("Cannot store value in list with type that does not match that of the list!");
			parentScope.subscopes.push_back(scope);
		} else if constexpr(SingleVal<T>) {
			//Basic setup
			ValueEntry value = {};
			value.name = fieldName;
			value.id = id;
			value.streamBeginPosition = 0;
			value.size = 0;

			//Type-specific data
			if constexpr(std::is_same_v<T, float>) {
				value.type = TypeTag::Float32;
			} else if constexpr(std::is_same_v<T, double>) {
				value.type = TypeTag::Float64;
			} else if constexpr(is_number_v<T>) {
				value.type = static_cast<TypeTag>(std::log2(bits_v<T> / 8) + 0x1A + (std::is_signed_v<T> ? 0 : 0x10));
			} else if constexpr(std::is_same_v<T, std::string>) {
				value.type = TypeTag::String;
			} else if constexpr(std::is_same_v<T, bool>) {
				value.type = TypeTag::Boolean;
			} else if constexpr(vec<T>) {
				value.type = TypeTag::Vector;
				value.height = 1;
				value.width = vec_count_v<T>;
				using U = vec_subtype_t<T>;
				if constexpr(std::is_same_v<U, float>) {
					value.elementType = TypeTag::Float32;
				} else if constexpr(std::is_same_v<U, double>) {
					value.elementType = TypeTag::Float64;
				} else if constexpr(is_number_v<U>) {
					value.elementType = static_cast<TypeTag>(std::log2(bits_v<U> / 8) + 0x1A + (std::is_signed_v<U> ? 0 : 0x10));
				}
			} else if constexpr(mat<T>) {
				value.type = TypeTag::Matrix;
				value.width = mat_width_v<T>;
				value.height = mat_height_v<T>;
				using U = mat_subtype_t<T>;
				if constexpr(std::is_same_v<U, float>) {
					value.elementType = TypeTag::Float32;
				} else if constexpr(std::is_same_v<U, double>) {
					value.elementType = TypeTag::Float64;
				} else if constexpr(is_number_v<U>) {
					value.elementType = static_cast<TypeTag>(std::log2(bits_v<U> / 8) + 0x1A + (std::is_signed_v<U> ? 0 : 0x10));
				}
			}

			//Check for list type match
			if(parentScope.list && parentScope.listElementType != value.type) throw std::runtime_error("Cannot store value in list with type that does not match that of the list!");

			//Add to scope
			parentScope.subvalues.push_back(value);

			//Setup storage
			ValueStorage vs = {};
			vs.materialized = true;
			vs.mem = std::vector<uint8_t>();
			storage[id] = vs;
		} else if(converters.contains(typeid(T))) {
			//Setup scope entry
			ScopeEntry scope = {};
			scope.list = false;
			scope.name = fieldName;
			scope.id = id;
			scope.streamBeginPosition = 0;
			scope.listElementType = TypeTag::StructuredObj;
			scope.typeID = std::find_if(structuredObjTypes.begin(), structuredObjTypes.end(), [](const auto& pair) { return pair.second == typeid(T); })->first;
			if(parentScope.list && (parentScope.listElementType != TypeTag::StructuredObj || (parentScope.listElementType == TypeTag::StructuredObj && parentScope.typeID.compare(scope.typeID) != 0))) throw std::runtime_error("Cannot store value in list with type that does not match that of the list!");
			parentScope.subscopes.push_back(scope);

			//Configure fields
			const StructuredTypeLayout& stl = index->types[scope.typeID];
			for(const StructuredTypeLayout::Field& field : stl.fields) {
				switch(field.type) {
					case TypeTag::String:
						CreateValue<std::string>(path + "." + field.name);
						break;
					case TypeTag::ByteBuffer:
						CreateValue<std::vector<uint8_t>>(path + "." + field.name, false);
						break;
					case TypeTag::Boolean:
						CreateValue<bool>(path + "." + field.name);
						break;
					case TypeTag::Float32:
						CreateValue<float>(path + "." + field.name);
						break;
					case TypeTag::Float64:
						CreateValue<double>(path + "." + field.name);
						break;
					case TypeTag::SInt8:
						CreateValue<int8_t>(path + "." + field.name);
						break;
					case TypeTag::SInt16:
						CreateValue<int16_t>(path + "." + field.name);
						break;
					case TypeTag::SInt32:
						CreateValue<int32_t>(path + "." + field.name);
						break;
					case TypeTag::SInt64:
						CreateValue<int64_t>(path + "." + field.name);
						break;
					case TypeTag::UInt8:
						CreateValue<uint8_t>(path + "." + field.name);
						break;
					case TypeTag::UInt16:
						CreateValue<uint16_t>(path + "." + field.name);
						break;
					case TypeTag::UInt32:
						CreateValue<uint32_t>(path + "." + field.name);
						break;
					case TypeTag::UInt64:
						CreateValue<uint64_t>(path + "." + field.name);
						break;
					case TypeTag::List:
						if(field.elementType == TypeTag::StructuredObj) {
							const std::type_index& type = structuredObjTypes.at(field.typeID);
							createValWraps.at(type)(path + "." + field.name, false);
							break;
						} else {
							switch(field.elementType) {
								case TypeTag::String:
									CreateValue<std::vector<std::string>>(path + "." + field.name);
									break;
								case TypeTag::ByteBuffer:
									CreateValue<std::vector<std::vector<uint8_t>>>(path + "." + field.name);
									break;
								case TypeTag::Boolean:
									CreateValue<std::vector<bool>>(path + "." + field.name);
									break;
								case TypeTag::Float32:
									CreateValue<std::vector<float>>(path + "." + field.name);
									break;
								case TypeTag::Float64:
									CreateValue<std::vector<double>>(path + "." + field.name);
									break;
								case TypeTag::SInt8:
									CreateValue<std::vector<int8_t>>(path + "." + field.name);
									break;
								case TypeTag::SInt16:
									CreateValue<std::vector<int16_t>>(path + "." + field.name);
									break;
								case TypeTag::SInt32:
									CreateValue<std::vector<int32_t>>(path + "." + field.name);
									break;
								case TypeTag::SInt64:
									CreateValue<std::vector<int64_t>>(path + "." + field.name);
									break;
								case TypeTag::UInt8:
									CreateValue<std::vector<uint8_t>>(path + "." + field.name, false);
									break;
								case TypeTag::UInt16:
									CreateValue<std::vector<uint16_t>>(path + "." + field.name);
									break;
								case TypeTag::UInt32:
									CreateValue<std::vector<uint32_t>>(path + "." + field.name);
									break;
								case TypeTag::UInt64:
									CreateValue<std::vector<uint64_t>>(path + "." + field.name);
									break;
								case TypeTag::UnstructuredObj:
									CreateValue<std::vector<UnstructuredObjTag>>(path + "." + field.name);
									break;
								case TypeTag::Vector:
								case TypeTag::Matrix: {
									//This makes our lives way easier and improves performance by avoiding nested switch; final bit layout TTTTTTTT WWWWHHHH (T = type, W = width, H = height)
									//What do you mean it's hard to understand? /j
									uint16_t determiner = (static_cast<uint8_t>(field.elementType) << 8) | ((field.width - 1) << 4) | (field.type == TypeTag::Matrix ? (field.height - 1) : 0);
									switch(determiner) {
										case 0x0E10:
											CreateValue<std::vector<Vector<float, 2>>>(path + "." + field.name);
											break;
										case 0x0E11:
											CreateValue<std::vector<Matrix<float, 2, 2>>>(path + "." + field.name);
											break;
										case 0x0E12:
											CreateValue<std::vector<Matrix<float, 2, 3>>>(path + "." + field.name);
											break;
										case 0x0E13:
											CreateValue<std::vector<Matrix<float, 2, 4>>>(path + "." + field.name);
											break;
										case 0x0E20:
											CreateValue<std::vector<Vector<float, 3>>>(path + "." + field.name);
											break;
										case 0x0E21:
											CreateValue<std::vector<Matrix<float, 3, 2>>>(path + "." + field.name);
											break;
										case 0x0E22:
											CreateValue<std::vector<Matrix<float, 3, 3>>>(path + "." + field.name);
											break;
										case 0x0E23:
											CreateValue<std::vector<Matrix<float, 3, 4>>>(path + "." + field.name);
											break;
										case 0x0E30:
											CreateValue<std::vector<Vector<float, 4>>>(path + "." + field.name);
											break;
										case 0x0E31:
											CreateValue<std::vector<Matrix<float, 4, 2>>>(path + "." + field.name);
											break;
										case 0x0E32:
											CreateValue<std::vector<Matrix<float, 4, 3>>>(path + "." + field.name);
											break;
										case 0x0E33:
											CreateValue<std::vector<Matrix<float, 4, 4>>>(path + "." + field.name);
											break;
										case 0x0F10:
											CreateValue<std::vector<Vector<double, 2>>>(path + "." + field.name);
											break;
										case 0x0F11:
											CreateValue<std::vector<Matrix<double, 2, 2>>>(path + "." + field.name);
											break;
										case 0x0F12:
											CreateValue<std::vector<Matrix<double, 2, 3>>>(path + "." + field.name);
											break;
										case 0x0F13:
											CreateValue<std::vector<Matrix<double, 2, 4>>>(path + "." + field.name);
											break;
										case 0x0F20:
											CreateValue<std::vector<Vector<double, 3>>>(path + "." + field.name);
											break;
										case 0x0F21:
											CreateValue<std::vector<Matrix<double, 3, 2>>>(path + "." + field.name);
											break;
										case 0x0F22:
											CreateValue<std::vector<Matrix<double, 3, 3>>>(path + "." + field.name);
											break;
										case 0x0F23:
											CreateValue<std::vector<Matrix<double, 3, 4>>>(path + "." + field.name);
											break;
										case 0x0F30:
											CreateValue<std::vector<Vector<double, 4>>>(path + "." + field.name);
											break;
										case 0x0F31:
											CreateValue<std::vector<Matrix<double, 4, 2>>>(path + "." + field.name);
											break;
										case 0x0F32:
											CreateValue<std::vector<Matrix<double, 4, 3>>>(path + "." + field.name);
											break;
										case 0x0F33:
											CreateValue<std::vector<Matrix<double, 4, 4>>>(path + "." + field.name);
											break;
										case 0x1A10:
											CreateValue<std::vector<Vector<int8_t, 2>>>(path + "." + field.name);
											break;
										case 0x1A11:
											CreateValue<std::vector<Matrix<int8_t, 2, 2>>>(path + "." + field.name);
											break;
										case 0x1A12:
											CreateValue<std::vector<Matrix<int8_t, 2, 3>>>(path + "." + field.name);
											break;
										case 0x1A13:
											CreateValue<std::vector<Matrix<int8_t, 2, 4>>>(path + "." + field.name);
											break;
										case 0x1A20:
											CreateValue<std::vector<Vector<int8_t, 3>>>(path + "." + field.name);
											break;
										case 0x1A21:
											CreateValue<std::vector<Matrix<int8_t, 3, 2>>>(path + "." + field.name);
											break;
										case 0x1A22:
											CreateValue<std::vector<Matrix<int8_t, 3, 3>>>(path + "." + field.name);
											break;
										case 0x1A23:
											CreateValue<std::vector<Matrix<int8_t, 3, 4>>>(path + "." + field.name);
											break;
										case 0x1A30:
											CreateValue<std::vector<Vector<int8_t, 4>>>(path + "." + field.name);
											break;
										case 0x1A31:
											CreateValue<std::vector<Matrix<int8_t, 4, 2>>>(path + "." + field.name);
											break;
										case 0x1A32:
											CreateValue<std::vector<Matrix<int8_t, 4, 3>>>(path + "." + field.name);
											break;
										case 0x1A33:
											CreateValue<std::vector<Matrix<int8_t, 4, 4>>>(path + "." + field.name);
											break;
										case 0x1B10:
											CreateValue<std::vector<Vector<int16_t, 2>>>(path + "." + field.name);
											break;
										case 0x1B11:
											CreateValue<std::vector<Matrix<int16_t, 2, 2>>>(path + "." + field.name);
											break;
										case 0x1B12:
											CreateValue<std::vector<Matrix<int16_t, 2, 3>>>(path + "." + field.name);
											break;
										case 0x1B13:
											CreateValue<std::vector<Matrix<int16_t, 2, 4>>>(path + "." + field.name);
											break;
										case 0x1B20:
											CreateValue<std::vector<Vector<int16_t, 3>>>(path + "." + field.name);
											break;
										case 0x1B21:
											CreateValue<std::vector<Matrix<int16_t, 3, 2>>>(path + "." + field.name);
											break;
										case 0x1B22:
											CreateValue<std::vector<Matrix<int16_t, 3, 3>>>(path + "." + field.name);
											break;
										case 0x1B23:
											CreateValue<std::vector<Matrix<int16_t, 3, 4>>>(path + "." + field.name);
											break;
										case 0x1B30:
											CreateValue<std::vector<Vector<int16_t, 4>>>(path + "." + field.name);
											break;
										case 0x1B31:
											CreateValue<std::vector<Matrix<int16_t, 4, 2>>>(path + "." + field.name);
											break;
										case 0x1B32:
											CreateValue<std::vector<Matrix<int16_t, 4, 3>>>(path + "." + field.name);
											break;
										case 0x1B33:
											CreateValue<std::vector<Matrix<int16_t, 4, 4>>>(path + "." + field.name);
											break;
										case 0x1C10:
											CreateValue<std::vector<Vector<int32_t, 2>>>(path + "." + field.name);
											break;
										case 0x1C11:
											CreateValue<std::vector<Matrix<int32_t, 2, 2>>>(path + "." + field.name);
											break;
										case 0x1C12:
											CreateValue<std::vector<Matrix<int32_t, 2, 3>>>(path + "." + field.name);
											break;
										case 0x1C13:
											CreateValue<std::vector<Matrix<int32_t, 2, 4>>>(path + "." + field.name);
											break;
										case 0x1C20:
											CreateValue<std::vector<Vector<int32_t, 3>>>(path + "." + field.name);
											break;
										case 0x1C21:
											CreateValue<std::vector<Matrix<int32_t, 3, 2>>>(path + "." + field.name);
											break;
										case 0x1C22:
											CreateValue<std::vector<Matrix<int32_t, 3, 3>>>(path + "." + field.name);
											break;
										case 0x1C23:
											CreateValue<std::vector<Matrix<int32_t, 3, 4>>>(path + "." + field.name);
											break;
										case 0x1C30:
											CreateValue<std::vector<Vector<int32_t, 4>>>(path + "." + field.name);
											break;
										case 0x1C31:
											CreateValue<std::vector<Matrix<int32_t, 4, 2>>>(path + "." + field.name);
											break;
										case 0x1C32:
											CreateValue<std::vector<Matrix<int32_t, 4, 3>>>(path + "." + field.name);
											break;
										case 0x1C33:
											CreateValue<std::vector<Matrix<int32_t, 4, 4>>>(path + "." + field.name);
											break;
										case 0x1D10:
											CreateValue<std::vector<Vector<int64_t, 2>>>(path + "." + field.name);
											break;
										case 0x1D11:
											CreateValue<std::vector<Matrix<int64_t, 2, 2>>>(path + "." + field.name);
											break;
										case 0x1D12:
											CreateValue<std::vector<Matrix<int64_t, 2, 3>>>(path + "." + field.name);
											break;
										case 0x1D13:
											CreateValue<std::vector<Matrix<int64_t, 2, 4>>>(path + "." + field.name);
											break;
										case 0x1D20:
											CreateValue<std::vector<Vector<int64_t, 3>>>(path + "." + field.name);
											break;
										case 0x1D21:
											CreateValue<std::vector<Matrix<int64_t, 3, 2>>>(path + "." + field.name);
											break;
										case 0x1D22:
											CreateValue<std::vector<Matrix<int64_t, 3, 3>>>(path + "." + field.name);
											break;
										case 0x1D23:
											CreateValue<std::vector<Matrix<int64_t, 3, 4>>>(path + "." + field.name);
											break;
										case 0x1D30:
											CreateValue<std::vector<Vector<int64_t, 4>>>(path + "." + field.name);
											break;
										case 0x1D31:
											CreateValue<std::vector<Matrix<int64_t, 4, 2>>>(path + "." + field.name);
											break;
										case 0x1D32:
											CreateValue<std::vector<Matrix<int64_t, 4, 3>>>(path + "." + field.name);
											break;
										case 0x1D33:
											CreateValue<std::vector<Matrix<int64_t, 4, 4>>>(path + "." + field.name);
											break;
										case 0x2A10:
											CreateValue<std::vector<Vector<uint8_t, 2>>>(path + "." + field.name);
											break;
										case 0x2A11:
											CreateValue<std::vector<Matrix<uint8_t, 2, 2>>>(path + "." + field.name);
											break;
										case 0x2A12:
											CreateValue<std::vector<Matrix<uint8_t, 2, 3>>>(path + "." + field.name);
											break;
										case 0x2A13:
											CreateValue<std::vector<Matrix<uint8_t, 2, 4>>>(path + "." + field.name);
											break;
										case 0x2A20:
											CreateValue<std::vector<Vector<uint8_t, 3>>>(path + "." + field.name);
											break;
										case 0x2A21:
											CreateValue<std::vector<Matrix<uint8_t, 3, 2>>>(path + "." + field.name);
											break;
										case 0x2A22:
											CreateValue<std::vector<Matrix<uint8_t, 3, 3>>>(path + "." + field.name);
											break;
										case 0x2A23:
											CreateValue<std::vector<Matrix<uint8_t, 3, 4>>>(path + "." + field.name);
											break;
										case 0x2A30:
											CreateValue<std::vector<Vector<uint8_t, 4>>>(path + "." + field.name);
											break;
										case 0x2A31:
											CreateValue<std::vector<Matrix<uint8_t, 4, 2>>>(path + "." + field.name);
											break;
										case 0x2A32:
											CreateValue<std::vector<Matrix<uint8_t, 4, 3>>>(path + "." + field.name);
											break;
										case 0x2A33:
											CreateValue<std::vector<Matrix<uint8_t, 4, 4>>>(path + "." + field.name);
											break;
										case 0x2B10:
											CreateValue<std::vector<Vector<uint16_t, 2>>>(path + "." + field.name);
											break;
										case 0x2B11:
											CreateValue<std::vector<Matrix<uint16_t, 2, 2>>>(path + "." + field.name);
											break;
										case 0x2B12:
											CreateValue<std::vector<Matrix<uint16_t, 2, 3>>>(path + "." + field.name);
											break;
										case 0x2B13:
											CreateValue<std::vector<Matrix<uint16_t, 2, 4>>>(path + "." + field.name);
											break;
										case 0x2B20:
											CreateValue<std::vector<Vector<uint16_t, 3>>>(path + "." + field.name);
											break;
										case 0x2B21:
											CreateValue<std::vector<Matrix<uint16_t, 3, 2>>>(path + "." + field.name);
											break;
										case 0x2B22:
											CreateValue<std::vector<Matrix<uint16_t, 3, 3>>>(path + "." + field.name);
											break;
										case 0x2B23:
											CreateValue<std::vector<Matrix<uint16_t, 3, 4>>>(path + "." + field.name);
											break;
										case 0x2B30:
											CreateValue<std::vector<Vector<uint16_t, 4>>>(path + "." + field.name);
											break;
										case 0x2B31:
											CreateValue<std::vector<Matrix<uint16_t, 4, 2>>>(path + "." + field.name);
											break;
										case 0x2B32:
											CreateValue<std::vector<Matrix<uint16_t, 4, 3>>>(path + "." + field.name);
											break;
										case 0x2B33:
											CreateValue<std::vector<Matrix<uint16_t, 4, 4>>>(path + "." + field.name);
											break;
										case 0x2C10:
											CreateValue<std::vector<Vector<uint32_t, 2>>>(path + "." + field.name);
											break;
										case 0x2C11:
											CreateValue<std::vector<Matrix<uint32_t, 2, 2>>>(path + "." + field.name);
											break;
										case 0x2C12:
											CreateValue<std::vector<Matrix<uint32_t, 2, 3>>>(path + "." + field.name);
											break;
										case 0x2C13:
											CreateValue<std::vector<Matrix<uint32_t, 2, 4>>>(path + "." + field.name);
											break;
										case 0x2C20:
											CreateValue<std::vector<Vector<uint32_t, 3>>>(path + "." + field.name);
											break;
										case 0x2C21:
											CreateValue<std::vector<Matrix<uint32_t, 3, 2>>>(path + "." + field.name);
											break;
										case 0x2C22:
											CreateValue<std::vector<Matrix<uint32_t, 3, 3>>>(path + "." + field.name);
											break;
										case 0x2C23:
											CreateValue<std::vector<Matrix<uint32_t, 3, 4>>>(path + "." + field.name);
											break;
										case 0x2C30:
											CreateValue<std::vector<Vector<uint32_t, 4>>>(path + "." + field.name);
											break;
										case 0x2C31:
											CreateValue<std::vector<Matrix<uint32_t, 4, 2>>>(path + "." + field.name);
											break;
										case 0x2C32:
											CreateValue<std::vector<Matrix<uint32_t, 4, 3>>>(path + "." + field.name);
											break;
										case 0x2C33:
											CreateValue<std::vector<Matrix<uint32_t, 4, 4>>>(path + "." + field.name);
											break;
										case 0x2D10:
											CreateValue<std::vector<Vector<uint64_t, 2>>>(path + "." + field.name);
											break;
										case 0x2D11:
											CreateValue<std::vector<Matrix<uint64_t, 2, 2>>>(path + "." + field.name);
											break;
										case 0x2D12:
											CreateValue<std::vector<Matrix<uint64_t, 2, 3>>>(path + "." + field.name);
											break;
										case 0x2D13:
											CreateValue<std::vector<Matrix<uint64_t, 2, 4>>>(path + "." + field.name);
											break;
										case 0x2D20:
											CreateValue<std::vector<Vector<uint64_t, 3>>>(path + "." + field.name);
											break;
										case 0x2D21:
											CreateValue<std::vector<Matrix<uint64_t, 3, 2>>>(path + "." + field.name);
											break;
										case 0x2D22:
											CreateValue<std::vector<Matrix<uint64_t, 3, 3>>>(path + "." + field.name);
											break;
										case 0x2D23:
											CreateValue<std::vector<Matrix<uint64_t, 3, 4>>>(path + "." + field.name);
											break;
										case 0x2D30:
											CreateValue<std::vector<Vector<uint64_t, 4>>>(path + "." + field.name);
											break;
										case 0x2D31:
											CreateValue<std::vector<Matrix<uint64_t, 4, 2>>>(path + "." + field.name);
											break;
										case 0x2D32:
											CreateValue<std::vector<Matrix<uint64_t, 4, 3>>>(path + "." + field.name);
											break;
										case 0x2D33:
											CreateValue<std::vector<Matrix<uint64_t, 4, 4>>>(path + "." + field.name);
											break;
									}
									break;
								}
								default: break;
							}
							break;
						}
					case TypeTag::UnstructuredObj:
						CreateValue<UnstructuredObjTag>(path + "." + field.name);
						break;
					case TypeTag::StructuredObj: {
						const std::type_index& type = structuredObjTypes.at(field.typeID);
						createValWraps.at(type)(path + "." + field.name, false);
						break;
					}
					case TypeTag::Vector:
					case TypeTag::Matrix: {
						//This makes our lives way easier and improves performance by avoiding nested switch; final bit layout TTTTTTTT WWWWHHHH (T = type, W = width, H = height)
						//What do you mean it's hard to understand? /j
						uint16_t determiner = (static_cast<uint8_t>(field.elementType) << 8) | ((field.width - 1) << 4) | (field.type == TypeTag::Matrix ? (field.height - 1) : 0);
						switch(determiner) {
							case 0x0E10:
								CreateValue<Vector<float, 2>>(path + "." + field.name);
								break;
							case 0x0E11:
								CreateValue<Matrix<float, 2, 2>>(path + "." + field.name);
								break;
							case 0x0E12:
								CreateValue<Matrix<float, 2, 3>>(path + "." + field.name);
								break;
							case 0x0E13:
								CreateValue<Matrix<float, 2, 4>>(path + "." + field.name);
								break;
							case 0x0E20:
								CreateValue<Vector<float, 3>>(path + "." + field.name);
								break;
							case 0x0E21:
								CreateValue<Matrix<float, 3, 2>>(path + "." + field.name);
								break;
							case 0x0E22:
								CreateValue<Matrix<float, 3, 3>>(path + "." + field.name);
								break;
							case 0x0E23:
								CreateValue<Matrix<float, 3, 4>>(path + "." + field.name);
								break;
							case 0x0E30:
								CreateValue<Vector<float, 4>>(path + "." + field.name);
								break;
							case 0x0E31:
								CreateValue<Matrix<float, 4, 2>>(path + "." + field.name);
								break;
							case 0x0E32:
								CreateValue<Matrix<float, 4, 3>>(path + "." + field.name);
								break;
							case 0x0E33:
								CreateValue<Matrix<float, 4, 4>>(path + "." + field.name);
								break;
							case 0x0F10:
								CreateValue<Vector<double, 2>>(path + "." + field.name);
								break;
							case 0x0F11:
								CreateValue<Matrix<double, 2, 2>>(path + "." + field.name);
								break;
							case 0x0F12:
								CreateValue<Matrix<double, 2, 3>>(path + "." + field.name);
								break;
							case 0x0F13:
								CreateValue<Matrix<double, 2, 4>>(path + "." + field.name);
								break;
							case 0x0F20:
								CreateValue<Vector<double, 3>>(path + "." + field.name);
								break;
							case 0x0F21:
								CreateValue<Matrix<double, 3, 2>>(path + "." + field.name);
								break;
							case 0x0F22:
								CreateValue<Matrix<double, 3, 3>>(path + "." + field.name);
								break;
							case 0x0F23:
								CreateValue<Matrix<double, 3, 4>>(path + "." + field.name);
								break;
							case 0x0F30:
								CreateValue<Vector<double, 4>>(path + "." + field.name);
								break;
							case 0x0F31:
								CreateValue<Matrix<double, 4, 2>>(path + "." + field.name);
								break;
							case 0x0F32:
								CreateValue<Matrix<double, 4, 3>>(path + "." + field.name);
								break;
							case 0x0F33:
								CreateValue<Matrix<double, 4, 4>>(path + "." + field.name);
								break;
							case 0x1A10:
								CreateValue<Vector<int8_t, 2>>(path + "." + field.name);
								break;
							case 0x1A11:
								CreateValue<Matrix<int8_t, 2, 2>>(path + "." + field.name);
								break;
							case 0x1A12:
								CreateValue<Matrix<int8_t, 2, 3>>(path + "." + field.name);
								break;
							case 0x1A13:
								CreateValue<Matrix<int8_t, 2, 4>>(path + "." + field.name);
								break;
							case 0x1A20:
								CreateValue<Vector<int8_t, 3>>(path + "." + field.name);
								break;
							case 0x1A21:
								CreateValue<Matrix<int8_t, 3, 2>>(path + "." + field.name);
								break;
							case 0x1A22:
								CreateValue<Matrix<int8_t, 3, 3>>(path + "." + field.name);
								break;
							case 0x1A23:
								CreateValue<Matrix<int8_t, 3, 4>>(path + "." + field.name);
								break;
							case 0x1A30:
								CreateValue<Vector<int8_t, 4>>(path + "." + field.name);
								break;
							case 0x1A31:
								CreateValue<Matrix<int8_t, 4, 2>>(path + "." + field.name);
								break;
							case 0x1A32:
								CreateValue<Matrix<int8_t, 4, 3>>(path + "." + field.name);
								break;
							case 0x1A33:
								CreateValue<Matrix<int8_t, 4, 4>>(path + "." + field.name);
								break;
							case 0x1B10:
								CreateValue<Vector<int16_t, 2>>(path + "." + field.name);
								break;
							case 0x1B11:
								CreateValue<Matrix<int16_t, 2, 2>>(path + "." + field.name);
								break;
							case 0x1B12:
								CreateValue<Matrix<int16_t, 2, 3>>(path + "." + field.name);
								break;
							case 0x1B13:
								CreateValue<Matrix<int16_t, 2, 4>>(path + "." + field.name);
								break;
							case 0x1B20:
								CreateValue<Vector<int16_t, 3>>(path + "." + field.name);
								break;
							case 0x1B21:
								CreateValue<Matrix<int16_t, 3, 2>>(path + "." + field.name);
								break;
							case 0x1B22:
								CreateValue<Matrix<int16_t, 3, 3>>(path + "." + field.name);
								break;
							case 0x1B23:
								CreateValue<Matrix<int16_t, 3, 4>>(path + "." + field.name);
								break;
							case 0x1B30:
								CreateValue<Vector<int16_t, 4>>(path + "." + field.name);
								break;
							case 0x1B31:
								CreateValue<Matrix<int16_t, 4, 2>>(path + "." + field.name);
								break;
							case 0x1B32:
								CreateValue<Matrix<int16_t, 4, 3>>(path + "." + field.name);
								break;
							case 0x1B33:
								CreateValue<Matrix<int16_t, 4, 4>>(path + "." + field.name);
								break;
							case 0x1C10:
								CreateValue<Vector<int32_t, 2>>(path + "." + field.name);
								break;
							case 0x1C11:
								CreateValue<Matrix<int32_t, 2, 2>>(path + "." + field.name);
								break;
							case 0x1C12:
								CreateValue<Matrix<int32_t, 2, 3>>(path + "." + field.name);
								break;
							case 0x1C13:
								CreateValue<Matrix<int32_t, 2, 4>>(path + "." + field.name);
								break;
							case 0x1C20:
								CreateValue<Vector<int32_t, 3>>(path + "." + field.name);
								break;
							case 0x1C21:
								CreateValue<Matrix<int32_t, 3, 2>>(path + "." + field.name);
								break;
							case 0x1C22:
								CreateValue<Matrix<int32_t, 3, 3>>(path + "." + field.name);
								break;
							case 0x1C23:
								CreateValue<Matrix<int32_t, 3, 4>>(path + "." + field.name);
								break;
							case 0x1C30:
								CreateValue<Vector<int32_t, 4>>(path + "." + field.name);
								break;
							case 0x1C31:
								CreateValue<Matrix<int32_t, 4, 2>>(path + "." + field.name);
								break;
							case 0x1C32:
								CreateValue<Matrix<int32_t, 4, 3>>(path + "." + field.name);
								break;
							case 0x1C33:
								CreateValue<Matrix<int32_t, 4, 4>>(path + "." + field.name);
								break;
							case 0x1D10:
								CreateValue<Vector<int64_t, 2>>(path + "." + field.name);
								break;
							case 0x1D11:
								CreateValue<Matrix<int64_t, 2, 2>>(path + "." + field.name);
								break;
							case 0x1D12:
								CreateValue<Matrix<int64_t, 2, 3>>(path + "." + field.name);
								break;
							case 0x1D13:
								CreateValue<Matrix<int64_t, 2, 4>>(path + "." + field.name);
								break;
							case 0x1D20:
								CreateValue<Vector<int64_t, 3>>(path + "." + field.name);
								break;
							case 0x1D21:
								CreateValue<Matrix<int64_t, 3, 2>>(path + "." + field.name);
								break;
							case 0x1D22:
								CreateValue<Matrix<int64_t, 3, 3>>(path + "." + field.name);
								break;
							case 0x1D23:
								CreateValue<Matrix<int64_t, 3, 4>>(path + "." + field.name);
								break;
							case 0x1D30:
								CreateValue<Vector<int64_t, 4>>(path + "." + field.name);
								break;
							case 0x1D31:
								CreateValue<Matrix<int64_t, 4, 2>>(path + "." + field.name);
								break;
							case 0x1D32:
								CreateValue<Matrix<int64_t, 4, 3>>(path + "." + field.name);
								break;
							case 0x1D33:
								CreateValue<Matrix<int64_t, 4, 4>>(path + "." + field.name);
								break;
							case 0x2A10:
								CreateValue<Vector<uint8_t, 2>>(path + "." + field.name);
								break;
							case 0x2A11:
								CreateValue<Matrix<uint8_t, 2, 2>>(path + "." + field.name);
								break;
							case 0x2A12:
								CreateValue<Matrix<uint8_t, 2, 3>>(path + "." + field.name);
								break;
							case 0x2A13:
								CreateValue<Matrix<uint8_t, 2, 4>>(path + "." + field.name);
								break;
							case 0x2A20:
								CreateValue<Vector<uint8_t, 3>>(path + "." + field.name);
								break;
							case 0x2A21:
								CreateValue<Matrix<uint8_t, 3, 2>>(path + "." + field.name);
								break;
							case 0x2A22:
								CreateValue<Matrix<uint8_t, 3, 3>>(path + "." + field.name);
								break;
							case 0x2A23:
								CreateValue<Matrix<uint8_t, 3, 4>>(path + "." + field.name);
								break;
							case 0x2A30:
								CreateValue<Vector<uint8_t, 4>>(path + "." + field.name);
								break;
							case 0x2A31:
								CreateValue<Matrix<uint8_t, 4, 2>>(path + "." + field.name);
								break;
							case 0x2A32:
								CreateValue<Matrix<uint8_t, 4, 3>>(path + "." + field.name);
								break;
							case 0x2A33:
								CreateValue<Matrix<uint8_t, 4, 4>>(path + "." + field.name);
								break;
							case 0x2B10:
								CreateValue<Vector<uint16_t, 2>>(path + "." + field.name);
								break;
							case 0x2B11:
								CreateValue<Matrix<uint16_t, 2, 2>>(path + "." + field.name);
								break;
							case 0x2B12:
								CreateValue<Matrix<uint16_t, 2, 3>>(path + "." + field.name);
								break;
							case 0x2B13:
								CreateValue<Matrix<uint16_t, 2, 4>>(path + "." + field.name);
								break;
							case 0x2B20:
								CreateValue<Vector<uint16_t, 3>>(path + "." + field.name);
								break;
							case 0x2B21:
								CreateValue<Matrix<uint16_t, 3, 2>>(path + "." + field.name);
								break;
							case 0x2B22:
								CreateValue<Matrix<uint16_t, 3, 3>>(path + "." + field.name);
								break;
							case 0x2B23:
								CreateValue<Matrix<uint16_t, 3, 4>>(path + "." + field.name);
								break;
							case 0x2B30:
								CreateValue<Vector<uint16_t, 4>>(path + "." + field.name);
								break;
							case 0x2B31:
								CreateValue<Matrix<uint16_t, 4, 2>>(path + "." + field.name);
								break;
							case 0x2B32:
								CreateValue<Matrix<uint16_t, 4, 3>>(path + "." + field.name);
								break;
							case 0x2B33:
								CreateValue<Matrix<uint16_t, 4, 4>>(path + "." + field.name);
								break;
							case 0x2C10:
								CreateValue<Vector<uint32_t, 2>>(path + "." + field.name);
								break;
							case 0x2C11:
								CreateValue<Matrix<uint32_t, 2, 2>>(path + "." + field.name);
								break;
							case 0x2C12:
								CreateValue<Matrix<uint32_t, 2, 3>>(path + "." + field.name);
								break;
							case 0x2C13:
								CreateValue<Matrix<uint32_t, 2, 4>>(path + "." + field.name);
								break;
							case 0x2C20:
								CreateValue<Vector<uint32_t, 3>>(path + "." + field.name);
								break;
							case 0x2C21:
								CreateValue<Matrix<uint32_t, 3, 2>>(path + "." + field.name);
								break;
							case 0x2C22:
								CreateValue<Matrix<uint32_t, 3, 3>>(path + "." + field.name);
								break;
							case 0x2C23:
								CreateValue<Matrix<uint32_t, 3, 4>>(path + "." + field.name);
								break;
							case 0x2C30:
								CreateValue<Vector<uint32_t, 4>>(path + "." + field.name);
								break;
							case 0x2C31:
								CreateValue<Matrix<uint32_t, 4, 2>>(path + "." + field.name);
								break;
							case 0x2C32:
								CreateValue<Matrix<uint32_t, 4, 3>>(path + "." + field.name);
								break;
							case 0x2C33:
								CreateValue<Matrix<uint32_t, 4, 4>>(path + "." + field.name);
								break;
							case 0x2D10:
								CreateValue<Vector<uint64_t, 2>>(path + "." + field.name);
								break;
							case 0x2D11:
								CreateValue<Matrix<uint64_t, 2, 2>>(path + "." + field.name);
								break;
							case 0x2D12:
								CreateValue<Matrix<uint64_t, 2, 3>>(path + "." + field.name);
								break;
							case 0x2D13:
								CreateValue<Matrix<uint64_t, 2, 4>>(path + "." + field.name);
								break;
							case 0x2D20:
								CreateValue<Vector<uint64_t, 3>>(path + "." + field.name);
								break;
							case 0x2D21:
								CreateValue<Matrix<uint64_t, 3, 2>>(path + "." + field.name);
								break;
							case 0x2D22:
								CreateValue<Matrix<uint64_t, 3, 3>>(path + "." + field.name);
								break;
							case 0x2D23:
								CreateValue<Matrix<uint64_t, 3, 4>>(path + "." + field.name);
								break;
							case 0x2D30:
								CreateValue<Vector<uint64_t, 4>>(path + "." + field.name);
								break;
							case 0x2D31:
								CreateValue<Matrix<uint64_t, 4, 2>>(path + "." + field.name);
								break;
							case 0x2D32:
								CreateValue<Matrix<uint64_t, 4, 3>>(path + "." + field.name);
								break;
							case 0x2D33:
								CreateValue<Matrix<uint64_t, 4, 4>>(path + "." + field.name);
								break;
						}
						break;
					}
					default: break;
				}
			}
		} else
			throw std::runtime_error("Invalid type for field creation!");
	}

	template<typename T>
		requires(!is_vec_v<T>) && (!is_mat_v<T>)
	void Document::VerifyTypeCompatibility(const ValueEntry& ve) {
		switch(ve.type) {
			case TypeTag::String:
				if constexpr(!std::is_same_v<T, std::string>) throw std::runtime_error("Type incompatibility: String checked against non-matching candidate type!");
				break;
			case TypeTag::ByteBuffer:
				if constexpr(!is_byte_range_v<T>)
					throw std::runtime_error("Type incompatibility: ByteBuffer checked against non-matching candidate type!");
				else if constexpr(!resizable_range<T>) {
					if(std::ranges::size(T {}) < ve.size) throw std::runtime_error("Type incompatibility: ByteBuffer requires a candidate storage with sufficient space!");
				}
				break;
			case TypeTag::Boolean:
				if constexpr(!std::is_same_v<T, bool>) throw std::runtime_error("Type incompatibility: Boolean checked against non-matching candidate type!");
				break;
			case TypeTag::Float32:
				if constexpr(!std::is_same_v<T, float>) throw std::runtime_error("Type incompatibility: Float32 checked against non-matching candidate type!");
				break;
			case TypeTag::Float64:
				if constexpr(!std::is_same_v<T, double>) throw std::runtime_error("Type incompatibility: Float64 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt8:
				if constexpr(!std::is_same_v<T, int8_t>) throw std::runtime_error("Type incompatibility: SInt8 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt16:
				if constexpr(!std::is_same_v<T, int16_t>) throw std::runtime_error("Type incompatibility: SInt16 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt32:
				if constexpr(!std::is_same_v<T, int32_t>) throw std::runtime_error("Type incompatibility: SInt32 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt64:
				if constexpr(!std::is_same_v<T, int64_t>) throw std::runtime_error("Type incompatibility: SInt64 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt8:
				if constexpr(!std::is_same_v<T, uint8_t>) throw std::runtime_error("Type incompatibility: UInt8 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt16:
				if constexpr(!std::is_same_v<T, uint16_t>) throw std::runtime_error("Type incompatibility: UInt16 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt32:
				if constexpr(!std::is_same_v<T, uint32_t>) throw std::runtime_error("Type incompatibility: UInt32 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt64:
				if constexpr(!std::is_same_v<T, uint64_t>) throw std::runtime_error("Type incompatibility: UInt64 checked against non-matching candidate type!");
				break;
			case TypeTag::Vector:
				throw std::runtime_error("Type incompatibility: Vector checked against non-matching candidate type!");
			case TypeTag::Matrix:
				throw std::runtime_error("Type incompatibility: Vector checked against non-matching candidate type!");
				break;
			default: break;
		}
	}

	template<vec T>
	void Document::VerifyTypeCompatibility(const ValueEntry& ve) {
		if(ve.type != TypeTag::Vector) throw std::runtime_error("Type incompatibility: Vector checked against non-matching candidate type!");
		if(vec_count_v<T> != ve.width) throw std::runtime_error("Type incompatibility: Vector requires a candidate storage with inappropriate component count!");
		switch(ve.elementType) {
			case TypeTag::Float32:
				if constexpr(!std::is_same_v<vec_subtype_t<T>, float>) throw std::runtime_error("Type incompatibility: Vector of Float32 checked against non-matching candidate type!");
				break;
			case TypeTag::Float64:
				if constexpr(!std::is_same_v<vec_subtype_t<T>, double>) throw std::runtime_error("Type incompatibility: Vector of Float64 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt8:
				if constexpr(!std::is_same_v<vec_subtype_t<T>, int8_t>) throw std::runtime_error("Type incompatibility: Vector of SInt8 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt16:
				if constexpr(!std::is_same_v<vec_subtype_t<T>, int16_t>) throw std::runtime_error("Type incompatibility: Vector of SInt16 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt32:
				if constexpr(!std::is_same_v<vec_subtype_t<T>, int32_t>) throw std::runtime_error("Type incompatibility: Vector of SInt32 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt64:
				if constexpr(!std::is_same_v<vec_subtype_t<T>, int64_t>) throw std::runtime_error("Type incompatibility: Vector of SInt64 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt8:
				if constexpr(!std::is_same_v<vec_subtype_t<T>, uint8_t>) throw std::runtime_error("Type incompatibility: Vector of UInt8 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt16:
				if constexpr(!std::is_same_v<vec_subtype_t<T>, uint16_t>) throw std::runtime_error("Type incompatibility: Vector of UInt16 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt32:
				if constexpr(!std::is_same_v<vec_subtype_t<T>, uint32_t>) throw std::runtime_error("Type incompatibility: Vector of UInt32 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt64:
				if constexpr(!std::is_same_v<vec_subtype_t<T>, uint64_t>) throw std::runtime_error("Type incompatibility: Vector of UInt64 checked against non-matching candidate type!");
				break;
			default: break;
		}
	}

	template<mat T>
	void Document::VerifyTypeCompatibility(const ValueEntry& ve) {
		if(ve.type != TypeTag::Matrix) throw std::runtime_error("Type incompatibility: Matrix checked against non-matching candidate type!");
		if(mat_width_v<T> != ve.width || mat_height_v<T> != ve.height) throw std::runtime_error("Type incompatibility: Matrix requires a candidate storage with inappropriate dimensions!");
		switch(ve.elementType) {
			case TypeTag::Float32:
				if constexpr(!std::is_same_v<mat_subtype_t<T>, float>) throw std::runtime_error("Type incompatibility: Matrix of Float32 checked against non-matching candidate type!");
				break;
			case TypeTag::Float64:
				if constexpr(!std::is_same_v<mat_subtype_t<T>, double>) throw std::runtime_error("Type incompatibility: Matrix of Float64 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt8:
				if constexpr(!std::is_same_v<mat_subtype_t<T>, int8_t>) throw std::runtime_error("Type incompatibility: Matrix of SInt8 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt16:
				if constexpr(!std::is_same_v<mat_subtype_t<T>, int16_t>) throw std::runtime_error("Type incompatibility: Matrix of SInt16 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt32:
				if constexpr(!std::is_same_v<mat_subtype_t<T>, int32_t>) throw std::runtime_error("Type incompatibility: Matrix of SInt32 checked against non-matching candidate type!");
				break;
			case TypeTag::SInt64:
				if constexpr(!std::is_same_v<mat_subtype_t<T>, int64_t>) throw std::runtime_error("Type incompatibility: Matrix of SInt64 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt8:
				if constexpr(!std::is_same_v<mat_subtype_t<T>, uint8_t>) throw std::runtime_error("Type incompatibility: Matrix of UInt8 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt16:
				if constexpr(!std::is_same_v<mat_subtype_t<T>, uint16_t>) throw std::runtime_error("Type incompatibility: Matrix of UInt16 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt32:
				if constexpr(!std::is_same_v<mat_subtype_t<T>, uint32_t>) throw std::runtime_error("Type incompatibility: Matrix of UInt32 checked against non-matching candidate type!");
				break;
			case TypeTag::UInt64:
				if constexpr(!std::is_same_v<mat_subtype_t<T>, uint64_t>) throw std::runtime_error("Type incompatibility: Matrix of UInt64 checked against non-matching candidate type!");
				break;
			default: break;
		}
	}

	template<typename T>
		requires(!is_byte_range_v<T>)
	void Document::SetOrCreateValue(const std::string& path, const T& value) {
		if(!HasValue(path)) {
			CreateValue<T>(path);
		}
		SetValue<T>(path, value);
	}

	template<byte_range T>
	void Document::SetOrCreateValue(const std::string& path, bool list, const T& value) {
		if(!HasValue(path)) {
			CreateValue<T>(path, list);
		}
		SetValue<T>(path, value);
	}
	///@endcond
}