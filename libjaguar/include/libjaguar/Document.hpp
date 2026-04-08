#pragma once

#include "DllHelper.hpp"
#include "MathTypes.hpp"
#include "Reader.hpp"
#include "Index.hpp"
#include "Traits.hpp"
#include "Utilities.hpp"

#include <algorithm>
#include <any>
#include <bit>
#include <functional>
#include <istream>
#include <iterator>
#include <ostream>
#include <optional>
#include <stdexcept>
#include <string>
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
		  : reader(), streamState(StreamState::NoStream), index(Index {}) {}

		/**
		 * @brief Create a document sourcing initial data from an input stream
		 *
		 * @param stream The stream to read from
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
		 * @brief A utility class for structured object converters to read values
		 */
		struct ObjReader {
		  public:
			/**
			 * @brief Get a value from the document
			 *
			 * @tparam T The object type to return the Jaguar data as; must match the type declared in the index
			 *
			 * @param field The name of the field to retrieve
			 *
			 * @return The current value stored in that field
			 *
			 * @throws std::runtime_error If no field exists in the document with the given ID and type
			 */
			template<typename T>
			T Get(const std::string& field) {
				return doc->QueryValue<T>(basePath + "." + field);
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
			 * @brief Set a value in the object scope
			 *
			 * @tparam T The object type from which to derive the Jaguar data; must match the type declared in the index
			 *
			 * @param field The name of the field to set
			 * @param value The value to store in the field
			 *
			 * @throws std::runtime_error If a field exists in the document with a type that does not match
			 */
			template<typename T>
			void Set(const std::string& field, const T& value) {
				doc->SetValue(basePath + "." + field, value);
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
		 * @param dec A function to convert from Jaguar representation to a T object
		 * @param enc A function to convert from a T object to Jaguar representation
		 *
		 * @throws If a converter has already been registered for T
		 */
		template<typename T>
			requires std::is_class_v<T>
		void RegisterStructuredObjConverter(const std::string& typeID, std::function<T(const ScopeEntry&, const ObjReader&)> dec, std::function<void(const T&, ObjWriter&)> enc) {
			if(!CheckUTF8(typeID)) throw std::runtime_error("Cannot register a structured object type with an invalid UTF-8 type ID!");
			if(converters.contains(typeid(T))) throw std::runtime_error("A converter has already been registered for this type!");
			structuredObjTypes[typeID] = typeid(T);
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
		std::unordered_map<std::string, std::type_index> structuredObjTypes;
		std::unordered_map<std::type_index, std::pair<std::function<std::any(const ScopeEntry&, const ObjReader&)>, std::function<void(const std::any&, ObjWriter&)>>> converters;
		std::unordered_map<uint64_t, ValueStorage> storage;

		bool _Verify();

		friend struct ObjReader;
		friend struct ObjWriter;

		template<typename T>
		ValueStorage From(const T&) = delete;

		template<number T>
		ValueStorage From(const T& num) {
			with_bits_t<bits_v<T>> work = std::bit_cast<with_bits_t<bits_v<T>>, T>(num);
			std::vector<std::byte> mem(0, bits_v<T>);
			for(uint8_t i = 0; i < bits_v<T>; ++i) {
				mem[i] = (work & 0xFF);
				work >>= 8;
			}
			return ValueStorage {.materialized = true, .mem = mem, .inStream = 0};
		}

		template<>
		ValueStorage From(const bool& val) {
			ValueStorage vs = {.materialized = true, .mem = std::vector<std::byte> {1}, .inStream = 0};
			vs.mem[0] = std::byte(val ? 1 : 0);
			return vs;
		}

		template<>
		ValueStorage From(const std::string& val);

		template<byte_range T>
		ValueStorage From(const T& range) {
			return ValueStorage {.materialized = true, .mem = std::vector<std::byte>(range.begin(), range.end()), .inStream = 0};
		}

		template<number T>
		ValueStorage From(const Vector<T, 2>& vec) {
			ValueStorage vs {.materialized = true, .mem = std::vector<std::byte>(bits_v<T> / 4), .inStream = 0};
			with_bits_t<bits_v<T>> x = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.x);
			for(uint8_t i = 0; i < bits_v<T>; ++i) {
				vs.mem[i] = (x & 0xFF);
				x >>= 8;
			}
			with_bits_t<bits_v<T>> y = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.y);
			for(uint8_t i = 0; i < bits_v<T>; ++i) {
				vs.mem[bits_v<T> + i] = (y & 0xFF);
				y >>= 8;
			}
			return vs;
		}

		template<number T>
		ValueStorage From(const Vector<T, 3>& vec) {
			ValueStorage vs {.materialized = true, .mem = std::vector<std::byte>(bits_v<T> / 8 * 3), .inStream = 0};
			with_bits_t<bits_v<T>> x = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.x);
			for(uint8_t i = 0; i < bits_v<T>; ++i) {
				vs.mem[i] = (x & 0xFF);
				x >>= 8;
			}
			with_bits_t<bits_v<T>> y = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.y);
			for(uint8_t i = 0; i < bits_v<T>; ++i) {
				vs.mem[bits_v<T> + i] = (y & 0xFF);
				y >>= 8;
			}
			with_bits_t<bits_v<T>> z = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.z);
			for(uint8_t i = 0; i < bits_v<T>; ++i) {
				vs.mem[bits_v<T> * 2 + i] = (z & 0xFF);
				z >>= 8;
			}
			return vs;
		}

		template<number T>
		ValueStorage From(const Vector<T, 4>& vec) {
			ValueStorage vs {.materialized = true, .mem = std::vector<std::byte>(bits_v<T> / 2), .inStream = 0};
			with_bits_t<bits_v<T>> x = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.x);
			for(uint8_t i = 0; i < bits_v<T>; ++i) {
				vs.mem[i] = (x & 0xFF);
				x >>= 8;
			}
			with_bits_t<bits_v<T>> y = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.y);
			for(uint8_t i = 0; i < bits_v<T>; ++i) {
				vs.mem[bits_v<T> + i] = (y & 0xFF);
				y >>= 8;
			}
			with_bits_t<bits_v<T>> z = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.z);
			for(uint8_t i = 0; i < bits_v<T>; ++i) {
				vs.mem[bits_v<T> * 2 + i] = (z & 0xFF);
				z >>= 8;
			}
			with_bits_t<bits_v<T>> w = std::bit_cast<with_bits_t<bits_v<T>>, T>(vec.w);
			for(uint8_t i = 0; i < bits_v<T>; ++i) {
				vs.mem[bits_v<T> * 3 + i] = (w & 0xFF);
				w >>= 8;
			}
			return vs;
		}

		template<number T, uint8_t W, uint8_t H>
		ValueStorage From(const Matrix<T, W, H>& mat) {
			ValueStorage vs {.materialized = true, .mem = std::vector<std::byte>(bits_v<T> / 2), .inStream = 0};
			for(uint8_t x = 0; x < W; ++x) {
				for(uint8_t y = 0; y < H; ++y) {
					with_bits_t<bits_v<T>> val = std::bit_cast<with_bits_t<bits_v<T>>, T>(mat[x][y]);
					for(uint8_t i = 0; i < bits_v<T>; ++i) {
						vs.mem[bits_v<T> * (x + 1) * (y + 1) + i] = (val & 0xFF);
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
			for(uint8_t i = 0; i < storage.mem.size(); ++i) {
				work <<= 8;
				work &= (static_cast<uint8_t>(storage.mem[i]) & 0xFF);
			}
			return std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
		}

		template<>
		bool To(const ValueStorage& storage) {
			return storage.mem[0] == std::byte(1);
		}

		template<>
		std::string To(const ValueStorage& storage);

		template<byte_range T>
		T To(const ValueStorage& storage) {
			T t;
			std::ranges::copy(storage.mem.begin(), storage.mem.end(), std::back_inserter(t));
			return t;
		}

		template<vec_c<2> V, typename T = vec_subtype_t<2, V>>
		V To(const ValueStorage& storage) {
			V vec {.x = 0, .y = 0};
			with_bits_t<bits_v<T>> work;
			for(uint8_t i = 0; i < storage.mem.size(); ++i) {
				//Update component data
				static uint8_t component = 0;
				if(i % (bits_v<T> / 8) == 0) {
					++component;
					work = 0;
					switch(component) {
						case 1:
							vec.x = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							break;
						case 2:
							vec.y = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							break;
					}
				}

				//Push latest byte
				work <<= 8;
				work &= (static_cast<uint8_t>(storage.mem[i]) & 0xFF);
			}
			return vec;
		}

		template<vec_c<3> V, typename T = vec_subtype_t<3, V>>
		V To(const ValueStorage& storage) {
			V vec {.x = 0, .y = 0, .z = 0};
			with_bits_t<bits_v<T>> work;
			for(uint8_t i = 0; i < storage.mem.size(); ++i) {
				//Update component data
				static uint8_t component = 0;
				if(i % (bits_v<T> / 8) == 0) {
					++component;
					work = 0;
					switch(component) {
						case 1:
							vec.x = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							break;
						case 2:
							vec.y = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							break;
						case 3:
							vec.z = std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
							break;
					}
				}

				//Push latest byte
				work <<= 8;
				work &= (static_cast<uint8_t>(storage.mem[i]) & 0xFF);
			}
			return vec;
		}

		template<vec_c<4> V, typename T = vec_subtype_t<4, V>>
		V To(const ValueStorage& storage) {
			V vec {.x = 0, .y = 0, .z = 0, .w = 0};
			with_bits_t<bits_v<T>> work;
			for(uint8_t i = 0; i < storage.mem.size(); ++i) {
				//Update component data
				static uint8_t component = 0;
				if(i % (bits_v<T> / 8) == 0) {
					++component;
					work = 0;
					switch(component) {
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
							break;
					}
				}

				//Push latest byte
				work <<= 8;
				work &= (static_cast<uint8_t>(storage.mem[i]) & 0xFF);
			}
			return vec;
		}

		template<number T, uint8_t W, uint8_t H>
		Matrix<T, W, H> To(const ValueStorage& storage) {
			Matrix<T, W, H> mat;
			for(uint8_t x = 0; x < W; ++x) {
				for(uint8_t y = 0; y < H; ++y) {
					with_bits_t<bits_v<T>> val = 0;
					for(uint8_t i = 0; i < bits_v<T>; ++i) {
						val <<= 8;
						val &= (storage.mem[bits_v<T> * (x + 1) * (y + 1) + i] & 0xFF);
					}
					mat[x][y] = val;
				}
			}
			return mat;
		}

		const ValueEntry& _ValInfoInternal(uint64_t id);
		const ScopeEntry& _ScopeInfoInternal(uint64_t id);
		const ValueStorage& _QueryInternal(uint64_t id);
		void _SetInternal(uint64_t id, const ValueStorage& val);
		std::any _QueryObjInternal(const std::string& path);
		void _SetObjInternal(const std::string& path, const std::any& obj);

		class DocPayloadProvider;
		friend class DocPayloadProvider;
	};

	///@cond
	template<typename T>
	concept SingleVal = requires(Document& d, const T& t) {
		d.From<std::template remove_cvref_t<T>>(t);
		d.To(t)->std::template remove_cvref_t<T>;
	};

	template<typename T>
	T Document::QueryValue(const std::string& path) {
		if(!CheckUTF8(path)) throw std::runtime_error("Invalid UTF-8 supplied as path data to query is not allowed!");
		if constexpr(SingleVal<T>) {
			return To<T>(_QueryInternal(GenIndexID(path)));
		} else {
			if(converters.contains(typeid(T))) {
				return std::any_cast<T>(_QueryObjInternal(path));
			}
			throw std::runtime_error("No valid type registered!");
		}
	}

	template<typename T>
	void Document::SetValue(const std::string& path, const T& value) {
		if(!CheckUTF8(path)) throw std::runtime_error("Invalid UTF-8 supplied as path data to query is not allowed!");
		if constexpr(SingleVal<T>) {
			_SetInternal(GenIndexID(path), From<T>(value));
		} else {
			if(converters.contains(typeid(T))) {
				_SetObjInternal(path, std::make_any(value));
			} else
				throw std::runtime_error("No valid type registered!");
		}
	}
	///@endcond
}