#pragma once

#include "DllHelper.hpp"
#include "MathTypes.hpp"
#include "Reader.hpp"
#include "Index.hpp"
#include "Traits.hpp"
#include "TypeTags.hpp"

#include <algorithm>
#include <any>
#include <bit>
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
	bool CheckUTF8(const std::string& string);
	///@endcond

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
		void RegisterStructuredObjConverter(const std::string& typeID, std::function<T(const ScopeEntry&, ObjReader&)> dec, std::function<void(const T&, ObjWriter&)> enc) {
			if(!CheckUTF8(typeID)) throw std::runtime_error("Cannot register a structured object type with an invalid UTF-8 type ID!");
			if(converters.contains(typeid(T))) throw std::runtime_error("A converter has already been registered for this type!");
			structuredObjTypes.insert_or_assign(typeID, typeid(T));
			auto dec2 = [dec](const ScopeEntry& s, ObjReader& o) -> std::any { return std::make_any<T>(std::move(dec(s, o))); };
			auto enc2 = [enc](const std::any& t, ObjWriter& o) -> void { enc(std::any_cast<T>(t), o); };
			auto cvt = std::make_pair<std::function<std::any(const ScopeEntry&, ObjReader&)>, std::function<void(const std::any&, ObjWriter&)>>(dec2, enc2);
			converters.insert_or_assign(typeid(T), std::move(cvt));
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

		/**
		 * @brief Check if the document contains a field at the given path
		 *
		 * @param path The full path of the field whose existence to check
		 *
		 * @return If the field exists
		 */
		bool HasValue(const std::string& path) {
			return _Has(GenIndexID(path));
		}

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
			bool materialized;		   //Whether or not the data has actually been loaded into memory
			std::vector<std::byte> mem;//In-memory storage
			std::streampos inStream;   //Location in stream
		};
		///@endcond

	  private:
		//Reading data
		std::optional<Reader> reader;
		enum class StreamState {
			NoStream,	///No stream is being used
			Unavailable,///A stream was being used but is now gone due to a move or other invalidating operation
			Available	///A stream is being used and is good to go
		} streamState;

		//Storage
		std::optional<Index> index;
		std::unordered_map<std::string, std::type_index> structuredObjTypes;
		std::unordered_map<std::type_index, std::pair<std::function<std::any(const ScopeEntry&, ObjReader&)>, std::function<void(const std::any&, ObjWriter&)>>> converters;
		std::unordered_map<uint64_t, ValueStorage> storage;

		bool _Verify();

		friend struct ObjReader;
		friend struct ObjWriter;

		template<typename T>
		ValueStorage From(const T&) = delete;

		template<number T>
		ValueStorage From(const T& num) {
			with_bits_t<bits_v<T>> work = std::bit_cast<with_bits_t<bits_v<T>>, T>(num);
			std::vector<std::byte> mem(bits_v<T>);
			for(uint8_t i = 0; i < bits_v<T>; ++i) {
				mem[i] = std::byte(work & 0xFF);
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
		ValueStorage From<std::string>(const std::string& val);

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
			for(uint8_t i = 0; i < storage.mem.size(); ++i) work |= (with_bits_t<bits_v<T>>(static_cast<uint8_t>(storage.mem[i]) & 0xFF) << (i * 8));
			return std::bit_cast<T, with_bits_t<bits_v<T>>>(work);
		}

		template<>
		bool To(const ValueStorage& storage) {
			return storage.mem[0] == std::byte(1);
		}

		template<>
		std::string To<std::string>(const ValueStorage& storage);

		template<byte_range T>
		T To(const ValueStorage& storage) {
			T t;
			std::ranges::copy(storage.mem.begin(), storage.mem.end(), std::back_inserter(t));
			return t;
		}

		template<vec_c<2> V>
		V To(const ValueStorage& storage) {
			using T = vec_subtype_t<V>;
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
				if constexpr(bits_v<T> > 8) {
					work |= (with_bits_t<bits_v<T>>(static_cast<uint8_t>(storage.mem[i]) & 0xFF) << (i * 8));
				} else {
					work = (static_cast<uint8_t>(storage.mem[i]) & 0xFF);
				}
			}
			return vec;
		}

		template<number T, uint8_t W, uint8_t H>
		Matrix<T, W, H> To(const ValueStorage& storage) {
			Matrix<T, W, H> mat;
			for(uint8_t x = 0; x < W; ++x) {
				for(uint8_t y = 0; y < H; ++y) {
					with_bits_t<bits_v<T>> val = 0;
					unsigned int bytes = (bits_v<T> / 8);
					for(uint8_t i = 0; i < bytes; ++i) {
						if constexpr(bits_v<T> > 8) {
							val |= (with_bits_t<bits_v<T>>(static_cast<uint8_t>(storage.mem[bytes * (x + 1) * (y + 1) + i]) & 0xFF) << (i * 8));
						} else {
							val = (storage.mem[bytes * (x + 1) * (y + 1) + i] & std::byte(0xFF));
						}
					}
					mat[x][y] = val;
				}
			}
			return mat;
		}

		template<typename T>
		void VerifyTypeCompatibility(const ValueEntry& ve);

		template<typename T>
			requires(!is_vec_v<T>) && (!is_mat_v<T>)
		void VerifyTypeCompatibility(const ValueEntry& ve) {
			switch(ve.type) {
				case TypeTag::String:
					if constexpr(!std::is_same_v<T, std::string>) throw std::runtime_error("Type incompatibility: String checked against non-matching candidate type!");
					break;
				case TypeTag::ByteBuffer:
					if constexpr(!is_byte_range_v<T>) throw std::runtime_error("Type incompatibility: ByteBuffer checked against non-matching candidate type!");
					if constexpr(std::ranges::sized_range<T>) {
						if(std::extent_v<T> < ve.size) throw std::runtime_error("Type incompatibility: ByteBuffer requires a candidate storage with sufficient space!");
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
		void VerifyTypeCompatibility(const ValueEntry& ve) {
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
		void VerifyTypeCompatibility(const ValueEntry& ve) {
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

		const ValueEntry& _ValInfoInternal(uint64_t id);
		const ScopeEntry& _ScopeInfoInternal(uint64_t id);
		const ValueStorage& _QueryInternal(uint64_t id);
		void _SetInternal(uint64_t id, const ValueStorage& val);
		std::any _QueryObjInternal(const std::string& path, std::type_index type);
		void _SetObjInternal(const std::string& path, const std::any& obj, std::type_index type);
		bool _Has(uint64_t id);

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
		if(!CheckUTF8(path)) throw std::runtime_error("Invalid UTF-8 supplied as path data to query is not allowed!");
		if constexpr(SingleVal<T>) {
			uint64_t id = GenIndexID(path);
			if(_Has(id)) {
				//We have to type-check so we don't overwrite
				VerifyTypeCompatibility<T>(_ValInfoInternal(id));
			} else {
				//TODO: Configure index entry
			}
			_SetInternal(id, From<T>(value));
		} else {
			if(converters.contains(typeid(T))) {
				if(uint64_t id = GenIndexID(path); _Has(id) && structuredObjTypes.at(_ScopeInfoInternal(id).typeID) != typeid(T)) throw std::runtime_error("Existing structured object type mismatch!");
				_SetObjInternal(path, std::make_any(value), typeid(T));
			} else
				throw std::runtime_error("No valid type registered!");
		}
	}
	///@endcond
}