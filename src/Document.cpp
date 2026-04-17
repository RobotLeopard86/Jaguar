#include "libjaguar/Document.hpp"
#include "libjaguar/Decoder.hpp"
#include "libjaguar/Encoder.hpp"
#include "libjaguar/Index.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/Writer.hpp"
#include "Utilities.hpp"

#include <cstring>
#include <ostream>
#include <stdexcept>

#define INDEX_READ_CHECK \
	if(!index.has_value() && !_Verify()) throw std::runtime_error("Could not validate document stream state required for read operation!")

namespace libjaguar {
	Document::Document(Document&& other)
	  : reader(std::move(other.reader)), streamState(std::exchange(other.streamState, StreamState::Unavailable)), index(std::move(other.index)), structuredObjTypes(std::move(other.structuredObjTypes)), converters(std::move(other.converters)), storage(std::move(other.storage)) {}

	Document& Document::operator=(Document&& other) {
		if(this != &other) {
			reader = std::move(other.reader);
			streamState = std::exchange(other.streamState, StreamState::Unavailable);
			index = std::move(other.index);
			structuredObjTypes = std::move(other.structuredObjTypes);
			converters = std::move(other.converters);
			storage = std::move(other.storage);
		}
		return *this;
	}

	bool Document::_Verify() {
		if(streamState == StreamState::Available) {
			//Fix that up (shouldn't need to be checked but safety measure)
			if(!reader.has_value()) {
				streamState = StreamState::Unavailable;
				return false;
			}

			//Setup index if needed
			if(!index.has_value()) {
				//Save initial stream position
				std::streampos beg = reader.value()->tellg();

				//Create decoder and parse
				Decoder dec(std::move(reader.value()));
				dec.Parse();
				index = dec.GetIndex();

				//Get the reader back so RAII can safely destroy the decoder
				reader = std::move(dec).ReleaseReader();

				//Reset to initial position
				reader.value()->clear();
				reader.value()->seekg(beg);

				//Walk index to create initial storage map
				const auto indexWalk = [this](const ScopeEntry& entry) -> void {
					auto impl = [this](const ScopeEntry& entry, auto& implRef) mutable -> void {
						for(const ValueEntry& val : entry.subvalues) {
							ValueStorage vs = {};
							vs.materialized = false;
							vs.inStream = val.streamBeginPosition;
							vs.mem = {};
							storage[val.id] = vs;
						}
						for(const ScopeEntry& scope : entry.subscopes) implRef(scope, implRef);
					};
					impl(entry, impl);
				};
				indexWalk(index->root);
			}

			return true;
		} else
			return false;
	}

	template<>
	Document::ValueStorage Document::From<std::string>(const std::string& val) {
		ValueStorage vs = {.materialized = true, .mem = std::vector<std::byte> {val.size()}, .inStream = 0};
		std::memcpy(vs.mem.data(), val.data(), val.size());
		return vs;
	}

	template<>
	std::string Document::To<std::string>(const ValueStorage& storage) {
		std::string out;
		out.resize(storage.mem.size());
		std::memcpy(out.data(), storage.mem.data(), out.size());
		return out;
	}

	class Document::DocPayloadProvider : public PayloadProvider {
	  public:
		DocPayloadProvider(Document* doc)
		  : doc(doc) {}

		void String(uint64_t id, std::ostream& out, std::size_t chunkSize, std::size_t offset) override {
			const ValueEntry& ve = doc->_ValInfoInternal(id);
			if(ve.type != TypeTag::String) throw std::runtime_error("Requested a string for a value that is not one!");
			std::string chunk = doc->To<std::string>(doc->_QueryInternal(id)).substr(offset, chunkSize);
			out.write(chunk.data(), chunkSize);
		}
		void Buffer(uint64_t id, std::ostream& out, std::size_t chunkSize, std::size_t offset) override {
			const ValueEntry& ve = doc->_ValInfoInternal(id);
			if(ve.type != TypeTag::ByteBuffer) throw std::runtime_error("Requested a byte buffer for a value that is not one!");
			const ValueStorage& vs = doc->_QueryInternal(id);
			if((int64_t(vs.mem.size()) - offset - chunkSize) < 0) throw std::runtime_error("Bad byte amount request!");
			out.write(reinterpret_cast<const char*>(vs.mem.data()), chunkSize);
		}
		bool Boolean(uint64_t id) override {
			if(doc->_ValInfoInternal(id).type != TypeTag::Boolean) throw std::runtime_error("Requested a boolean for a value that is not one!");
			return doc->To<bool>(doc->_QueryInternal(id));
		}
		uint64_t Integer(uint64_t id, uint8_t bits, bool isSigned) override {
			const ValueEntry& ve = doc->_ValInfoInternal(id);
			switch(bits) {
				case 8:
					if(isSigned && ve.type != TypeTag::SInt8)
						throw std::runtime_error("Requested an SInt8 for a value that is not one!");
					else if(!isSigned && ve.type != TypeTag::UInt8)
						throw std::runtime_error("Requested a UInt8 for a value that is not one!");
					break;
				case 16:
					if(isSigned && ve.type != TypeTag::SInt16)
						throw std::runtime_error("Requested an SInt16 for a value that is not one!");
					else if(!isSigned && ve.type != TypeTag::UInt16)
						throw std::runtime_error("Requested a UInt16 for a value that is not one!");
					break;
				case 32:
					if(isSigned && ve.type != TypeTag::SInt32)
						throw std::runtime_error("Requested an SInt32 for a value that is not one!");
					else if(!isSigned && ve.type != TypeTag::UInt32)
						throw std::runtime_error("Requested a UInt32 for a value that is not one!");
					break;
				case 64:
					if(isSigned && ve.type != TypeTag::SInt64)
						throw std::runtime_error("Requested an SInt64 for a value that is not one!");
					else if(!isSigned && ve.type != TypeTag::UInt64)
						throw std::runtime_error("Requested a UInt64 for a value that is not one!");
					break;
				default: break;
			}
			return doc->To<uint64_t>(doc->_QueryInternal(id));
		}
		float Float32(uint64_t id) override {
			if(doc->_ValInfoInternal(id).type != TypeTag::Float32) throw std::runtime_error("Requested a Float32 for a value that is not one!");
			return doc->To<float>(doc->_QueryInternal(id));
		}
		double Float64(uint64_t id) override {
			if(doc->_ValInfoInternal(id).type != TypeTag::Float64) throw std::runtime_error("Requested a Float64 for a value that is not one!");
			return doc->To<double>(doc->_QueryInternal(id));
		}
		Vector<uint64_t, 2> IntegerVec2(uint64_t id, uint8_t bits, bool isSigned) override {
			const ValueEntry& ve = doc->_ValInfoInternal(id);
			if(ve.type != TypeTag::Vector || ve.width != 2) throw std::runtime_error("Requested a 2-component vector for a value that is not one!");
			switch(bits) {
				case 8:
					if(isSigned && ve.elementType != TypeTag::SInt8)
						throw std::runtime_error("Requested a vector with element type SInt8 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt8)
						throw std::runtime_error("Requested a vector with element type UInt8 for a value that is not one!");
					{
						Vector<uint8_t, 2> vec = doc->To<Vector<uint8_t, 2>>(doc->_QueryInternal(id));
						return Vector<uint64_t, 2> {.x = vec.x, .y = vec.y};
					}
				case 16:
					if(isSigned && ve.elementType != TypeTag::SInt16)
						throw std::runtime_error("Requested a vector with element type SInt16 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt16)
						throw std::runtime_error("Requested a vector with element type UInt16 for a value that is not one!");
					{
						Vector<uint16_t, 2> vec = doc->To<Vector<uint16_t, 2>>(doc->_QueryInternal(id));
						return Vector<uint64_t, 2> {.x = vec.x, .y = vec.y};
					}
				case 32:
					if(isSigned && ve.elementType != TypeTag::SInt32)
						throw std::runtime_error("Requested a vector with element type SInt32 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt32)
						throw std::runtime_error("Requested a vector with element type UInt32 for a value that is not one!");
					{
						Vector<uint32_t, 2> vec = doc->To<Vector<uint32_t, 2>>(doc->_QueryInternal(id));
						return Vector<uint64_t, 2> {.x = vec.x, .y = vec.y};
					}
				case 64:
					if(isSigned && ve.elementType != TypeTag::SInt64)
						throw std::runtime_error("Requested a vector with element type SInt64 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt64)
						throw std::runtime_error("Requested a vector with element type UInt64 for a value that is not one!");
					return doc->To<Vector<uint64_t, 2>>(doc->_QueryInternal(id));
				default: return {};
			}
		}
		Vector<uint64_t, 3> IntegerVec3(uint64_t id, uint8_t bits, bool isSigned) override {
			const ValueEntry& ve = doc->_ValInfoInternal(id);
			if(ve.type != TypeTag::Vector || ve.width != 3) throw std::runtime_error("Requested a 3-component vector for a value that is not one!");
			switch(bits) {
				case 8:
					if(isSigned && ve.elementType != TypeTag::SInt8)
						throw std::runtime_error("Requested a vector with element type SInt8 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt8)
						throw std::runtime_error("Requested a vector with element type UInt8 for a value that is not one!");
					{
						Vector<uint8_t, 3> vec = doc->To<Vector<uint8_t, 3>>(doc->_QueryInternal(id));
						return Vector<uint64_t, 3> {.x = vec.x, .y = vec.y, .z = vec.z};
					}
				case 16:
					if(isSigned && ve.elementType != TypeTag::SInt16)
						throw std::runtime_error("Requested a vector with element type SInt16 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt16)
						throw std::runtime_error("Requested a vector with element type UInt16 for a value that is not one!");
					{
						Vector<uint16_t, 3> vec = doc->To<Vector<uint16_t, 3>>(doc->_QueryInternal(id));
						return Vector<uint64_t, 3> {.x = vec.x, .y = vec.y, .z = vec.z};
					}
				case 32:
					if(isSigned && ve.elementType != TypeTag::SInt32)
						throw std::runtime_error("Requested a vector with element type SInt32 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt32)
						throw std::runtime_error("Requested a vector with element type UInt32 for a value that is not one!");
					{
						Vector<uint32_t, 3> vec = doc->To<Vector<uint32_t, 3>>(doc->_QueryInternal(id));
						return Vector<uint64_t, 3> {.x = vec.x, .y = vec.y, .z = vec.z};
					}
				case 64:
					if(isSigned && ve.elementType != TypeTag::SInt64)
						throw std::runtime_error("Requested a vector with element type SInt64 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt64)
						throw std::runtime_error("Requested a vector with element type UInt64 for a value that is not one!");
					return doc->To<Vector<uint64_t, 3>>(doc->_QueryInternal(id));
				default: return {};
			}
		}
		Vector<uint64_t, 4> IntegerVec4(uint64_t id, uint8_t bits, bool isSigned) override {
			const ValueEntry& ve = doc->_ValInfoInternal(id);
			if(ve.type != TypeTag::Vector || ve.width != 4) throw std::runtime_error("Requested a 4-component vector for a value that is not one!");
			switch(bits) {
				case 8:
					if(isSigned && ve.elementType != TypeTag::SInt8)
						throw std::runtime_error("Requested a vector with element type SInt8 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt8)
						throw std::runtime_error("Requested a vector with element type UInt8 for a value that is not one!");
					{
						Vector<uint8_t, 4> vec = doc->To<Vector<uint8_t, 4>>(doc->_QueryInternal(id));
						return Vector<uint64_t, 4> {.x = vec.x, .y = vec.y, .z = vec.z, .w = vec.w};
					}
				case 16:
					if(isSigned && ve.elementType != TypeTag::SInt16)
						throw std::runtime_error("Requested a vector with element type SInt16 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt16)
						throw std::runtime_error("Requested a vector with element type UInt16 for a value that is not one!");
					{
						Vector<uint16_t, 4> vec = doc->To<Vector<uint16_t, 4>>(doc->_QueryInternal(id));
						return Vector<uint64_t, 4> {.x = vec.x, .y = vec.y, .z = vec.z, .w = vec.w};
					}
				case 32:
					if(isSigned && ve.elementType != TypeTag::SInt32)
						throw std::runtime_error("Requested a vector with element type SInt32 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt32)
						throw std::runtime_error("Requested a vector with element type UInt32 for a value that is not one!");
					{
						Vector<uint32_t, 4> vec = doc->To<Vector<uint32_t, 4>>(doc->_QueryInternal(id));
						return Vector<uint64_t, 4> {.x = vec.x, .y = vec.y, .z = vec.z, .w = vec.w};
					}
				case 64:
					if(isSigned && ve.elementType != TypeTag::SInt64)
						throw std::runtime_error("Requested a vector with element type SInt64 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt64)
						throw std::runtime_error("Requested a vector with element type UInt64 for a value that is not one!");
					return doc->To<Vector<uint64_t, 4>>(doc->_QueryInternal(id));
				default: return {};
			}
		}
		Vector<float, 2> Float32Vec2(uint64_t id) override {
			if(auto ve = doc->_ValInfoInternal(id); ve.type != TypeTag::Vector || ve.width != 2 || ve.elementType != TypeTag::Float32) throw std::runtime_error("Requested a 2-component vector with element type Float64 for a value that is not one!");
			return doc->To<Vector<float, 2>>(doc->_QueryInternal(id));
		}
		Vector<float, 3> Float32Vec3(uint64_t id) override {
			if(auto ve = doc->_ValInfoInternal(id); ve.type != TypeTag::Vector || ve.width != 3 || ve.elementType != TypeTag::Float32) throw std::runtime_error("Requested a 2-component vector with element type Float64 for a value that is not one!");
			return doc->To<Vector<float, 3>>(doc->_QueryInternal(id));
		}
		Vector<float, 4> Float32Vec4(uint64_t id) override {
			if(auto ve = doc->_ValInfoInternal(id); ve.type != TypeTag::Vector || ve.width != 4 || ve.elementType != TypeTag::Float32) throw std::runtime_error("Requested a 2-component vector with element type Float64 for a value that is not one!");
			return doc->To<Vector<float, 4>>(doc->_QueryInternal(id));
		}
		Vector<double, 2> Float64Vec2(uint64_t id) override {
			if(auto ve = doc->_ValInfoInternal(id); ve.type != TypeTag::Vector || ve.width != 2 || ve.elementType != TypeTag::Float64) throw std::runtime_error("Requested a 2-component vector with element type Float64 for a value that is not one!");
			return doc->To<Vector<double, 2>>(doc->_QueryInternal(id));
		}
		Vector<double, 3> Float64Vec3(uint64_t id) override {
			if(auto ve = doc->_ValInfoInternal(id); ve.type != TypeTag::Vector || ve.width != 3 || ve.elementType != TypeTag::Float64) throw std::runtime_error("Requested a 2-component vector with element type Float64 for a value that is not one!");
			return doc->To<Vector<double, 3>>(doc->_QueryInternal(id));
		}
		Vector<double, 4> Float64Vec4(uint64_t id) override {
			if(auto ve = doc->_ValInfoInternal(id); ve.type != TypeTag::Vector || ve.width != 4 || ve.elementType != TypeTag::Float64) throw std::runtime_error("Requested a 2-component vector with element type Float64 for a value that is not one!");
			return doc->To<Vector<double, 4>>(doc->_QueryInternal(id));
		}
		uint64_t IntegerMat(uint64_t id, uint8_t x, uint8_t y, uint8_t bits, bool isSigned) override {
			const ValueEntry& ve = doc->_ValInfoInternal(id);
			if(ve.type != TypeTag::Matrix || ve.width != x || ve.height != y) throw std::runtime_error("Requested a matrix with the incorrect dimensions for the value!");
			switch(bits) {
				case 8:
					if(isSigned && ve.elementType != TypeTag::SInt8)
						throw std::runtime_error("Requested a matrix with element type SInt8 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt8)
						throw std::runtime_error("Requested a matrix with element type UInt8 for a value that is not one!");
					break;
				case 16:
					if(isSigned && ve.elementType != TypeTag::SInt16)
						throw std::runtime_error("Requested a matrix with element type SInt16 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt16)
						throw std::runtime_error("Requested a matrix with element type UInt16 for a value that is not one!");
					break;
				case 32:
					if(isSigned && ve.elementType != TypeTag::SInt32)
						throw std::runtime_error("Requested a matrix with element type SInt32 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt32)
						throw std::runtime_error("Requested a matrix with element type UInt32 for a value that is not one!");
					break;
				case 64:
					if(isSigned && ve.elementType != TypeTag::SInt64)
						throw std::runtime_error("Requested a matrix with element type SInt64 for a value that is not one!");
					else if(!isSigned && ve.elementType != TypeTag::UInt64)
						throw std::runtime_error("Requested a matrix with element type UInt64 for a value that is not one!");
					break;
				default: break;
			}

			const ValueStorage& storage = doc->_QueryInternal(id);
			uint64_t out = 0;
			for(uint8_t i = ((bits / 8) * (x + 1) * (y + 1)); i < (bits / 8); ++i) {
				out <<= 8;
				out &= uint64_t(storage.mem[i] & std::byte(0xFF));
			}
			return out;
		}
		float Float32Mat(uint64_t id, uint8_t x, uint8_t y) override {
			if(auto ve = doc->_ValInfoInternal(id); ve.type != TypeTag::Matrix || ve.width != x || ve.height != y || ve.elementType != TypeTag::Float32) throw std::runtime_error("Requested a matrix with the incorrect dimensions or element type for the value!");
			const ValueStorage& storage = doc->_QueryInternal(id);
			uint32_t out = 0;
			for(uint8_t i = (4 * (x + 1) * (y + 1)); i < 4; ++i) {
				out <<= 8;
				out &= uint32_t(storage.mem[i] & std::byte(0xFF));
			}
			return std::bit_cast<float, uint32_t>(out);
		}
		double Float64Mat(uint64_t id, uint8_t x, uint8_t y) override {
			if(auto ve = doc->_ValInfoInternal(id); ve.type != TypeTag::Matrix || ve.width != x || ve.height != y || ve.elementType != TypeTag::Float64) throw std::runtime_error("Requested a matrix with the incorrect dimensions or element type for the value!");
			const ValueStorage& storage = doc->_QueryInternal(id);
			uint64_t out = 0;
			for(uint8_t i = (8 * (x + 1) * (y + 1)); i < 8; ++i) {
				out <<= 8;
				out &= uint64_t(storage.mem[i] & std::byte(0xFF));
			}
			return std::bit_cast<double, uint64_t>(out);
		}

	  private:
		Document* doc;
	};

	void Document::ExportTo(std::ostream& out) {
		INDEX_READ_CHECK;

		//Create encoder
		std::unique_ptr<std::ostream> stream = std::make_unique<std::ostream>(out.rdbuf());
		Writer w(std::move(stream));
		Encoder enc(std::move(w));

		//Create payload provider
		DocPayloadProvider provider(this);

		//Encode document
		enc.Write(index.value(), provider);

		//Hand back the streambuf
		w = std::move(enc).ReleaseWriter();
		w->flush();
		out.rdbuf(w->rdbuf());
	}

	void Document::MaterializeAll() {
		INDEX_READ_CHECK;
		Materialize(index->root.id);
	}

	void Document::Materialize(uint64_t id) {
		INDEX_READ_CHECK;

		//Check if this is an end value (very easy to materialize) (all values have preloaded entries in the storage map)
		if(storage.contains(id)) {
			//If the object is already materialized, nothing happens
			if(ValueStorage& vs = storage[id]; !vs.materialized) {
				//Seek to start of value body
				reader.value()->seekg(vs.inStream, std::ios::beg);

				//Grab type info to calculate value size
				const ValueEntry& typeInfo = _ValInfoInternal(id);
				MathTypeDescriptor mtd = {};
				if((static_cast<uint8_t>(typeInfo.type) >> 4) == 0x4) {
					mtd.width = typeInfo.width;
					mtd.height = typeInfo.height;
					mtd.type = typeInfo.elementType;
				}
				uint32_t valSize = CalcValueSize(typeInfo.type, mtd, static_cast<uint8_t>(typeInfo.type) <= 0xC ? typeInfo.size : 0);

				//Fetch data into storage memory
				//We can use it directly because ValueStorage is laid out the same as in the stream
				vs.mem.resize(valSize);
				reader.value()->read(reinterpret_cast<char*>(vs.mem.data()), valSize);
				if(reader.value()->eof()) throw std::runtime_error("Unexpected EOF in stream while materializing value!");
				if(!reader.value()->good()) throw std::runtime_error("Unexpected stream IO error while materializing value!");
				vs.materialized = true;
			}
			return;
		}

		//Otherwise we gotta grab the item's type info to materialize it
		const ScopeEntry& scope = _ScopeInfoInternal(id);

		//Now we can go through each subscope and subvalue to materialize them
		const auto materializeScope = [this](const ScopeEntry& entry) -> void {
			auto impl = [this](const ScopeEntry& entry, auto& implRef) mutable -> void {
				for(const ValueEntry& val : entry.subvalues) Materialize(val.id);
				for(const ScopeEntry& subscope : entry.subscopes) implRef(subscope, implRef);
			};
			return impl(entry, impl);
		};
		materializeScope(scope);
	}

	const ValueEntry& Document::_ValInfoInternal(uint64_t id) {
		//Verify stream state if needed
		INDEX_READ_CHECK;

		//Now we follow the path down
		const auto indexWalk = [id](ScopeEntry& entry) -> std::optional<std::reference_wrapper<ValueEntry>> {
			auto impl = [id](ScopeEntry& entry, auto& implRef) mutable -> std::optional<std::reference_wrapper<ValueEntry>> {
				for(ValueEntry& value : entry.subvalues) {
					if(value.id == id) return std::make_optional(std::reference_wrapper<ValueEntry>(value));
				}
				for(ScopeEntry& scope : entry.subscopes) {
					auto result = implRef(scope, implRef);
					if(result.has_value()) return result;
				}
				return std::nullopt;
			};
			return impl(entry, impl);
		};
		auto maybeVal = indexWalk(index->root);
		if(!maybeVal.has_value()) throw std::runtime_error("No value with the provided ID exists!");
		const ValueEntry& value = maybeVal->get();
		return value;
	}

	const ScopeEntry& Document::_ScopeInfoInternal(uint64_t id) {
		//Verify stream state if needed
		INDEX_READ_CHECK;

		//Now we follow the path down
		const auto indexWalk = [id](ScopeEntry& entry) -> std::optional<std::reference_wrapper<ScopeEntry>> {
			auto impl = [id](ScopeEntry& entry, auto& implRef) mutable -> std::optional<std::reference_wrapper<ScopeEntry>> {
				if(entry.id == id) return std::make_optional(std::reference_wrapper<ScopeEntry>(entry));
				for(ScopeEntry& scope : entry.subscopes) {
					auto result = implRef(scope, implRef);
					if(result.has_value()) return result;
				}
				return std::nullopt;
			};
			return impl(entry, impl);
		};
		auto maybeScope = indexWalk(index->root);
		if(!maybeScope.has_value()) throw std::runtime_error("No scope with the provided ID exists!");
		const ScopeEntry& scope = maybeScope->get();
		return scope;
	}

	const ValueEntry& Document::QueryValueInfo(const std::string& path) {
		if(!CheckUTF8(path)) throw std::runtime_error("Path supplied to document that is invalid UTF-8 data!");
		return _ValInfoInternal(GenIndexID(path));
	}

	const ScopeEntry& Document::QueryScopeInfo(const std::string& path) {
		if(!CheckUTF8(path)) throw std::runtime_error("Path supplied to document that is invalid UTF-8 data!");
		return _ScopeInfoInternal(GenIndexID(path));
	}

	bool Document::_Has(uint64_t id) {
		INDEX_READ_CHECK;
		return storage.contains(id);
	}

	const Document::ValueStorage& Document::_QueryInternal(uint64_t id) {
		//Verify stream state if needed
		INDEX_READ_CHECK;

		//Check storage state
		if(!storage.contains(id)) throw std::runtime_error("Cannot query value of nonexistent or scope field!");
		if(!storage[id].materialized) Materialize(id);

		//Send back the value
		return storage[id];
	}

	void Document::_SetInternal(uint64_t id, const ValueStorage& val) {
		//So... we just set it now
		storage[id] = val;
	}

	std::any Document::_QueryObjInternal(const std::string& path, std::type_index type) {
		//Create reader object and get it
		ObjReader rd;
		rd.basePath = path;
		rd.doc = this;

		//Execute conversion
		return converters[type].first(QueryScopeInfo(path), rd);
	}

	void Document::_SetObjInternal(const std::string& path, const std::any& obj, std::type_index type) {
		//Set all values
		ObjWriter ow;
		ow.basePath = path;
		ow.doc = this;
		converters[type].second(obj, ow);
	}

	void Document::DeleteValue(const std::string& path) {
		//Verify stream state if needed
		INDEX_READ_CHECK;

		if(!CheckUTF8(path)) throw std::runtime_error("Path supplied to document that is invalid UTF-8 data!");
		uint64_t id = GenIndexID(path);

		//Make sure this isn't a list item
		if(path.ends_with("]")) throw std::runtime_error("Cannot delete individual list items; must delete whole list!");

		//Find parent
		const auto indexWalk = [id](ScopeEntry& entry) -> std::optional<std::reference_wrapper<ScopeEntry>> {
			auto impl = [id](ScopeEntry& entry, auto& implRef) mutable -> std::optional<std::reference_wrapper<ScopeEntry>> {
				for(ValueEntry& value : entry.subvalues) {
					if(value.id == id) return std::make_optional(std::reference_wrapper<ScopeEntry>(entry));
				}
				for(ScopeEntry& scope : entry.subscopes) {
					if(scope.id == id) return std::make_optional(std::reference_wrapper<ScopeEntry>(entry));
					if(auto result = implRef(scope, implRef); result.has_value()) return result;
				}
				return std::nullopt;
			};
			return impl(entry, impl);
		};
		auto maybeScope = indexWalk(index->root);
		if(!maybeScope.has_value()) throw std::runtime_error("Couldn't find parent scope!");
		ScopeEntry& parentScope = maybeScope->get();
		if(!parentScope.typeID.empty()) throw std::runtime_error("Cannot delete member of structured object; must delete whole object!");

		//Check if this is a value (it'll be in storage if so)
		if(storage.contains(id)) {
			//It's a value; wipe it from storage and delete its entry
			storage.erase(id);
			for(auto it = parentScope.subvalues.begin(); it != parentScope.subvalues.end(); ++it) {
				if(it->id == id) parentScope.subvalues.erase(it);
			}
		} else {
			//It's a scope, so we need to erase all the children
			const auto purgeScope = [this](ScopeEntry& entry) -> void {
				auto impl = [this](ScopeEntry& entry, auto& implRef) mutable -> void {
					for(ValueEntry& val : entry.subvalues) {
						storage.erase(val.id);
					}
					entry.subvalues.clear();
					for(ScopeEntry& subscope : entry.subscopes) implRef(subscope, implRef);
					entry.subscopes.clear();
				};
				return impl(entry, impl);
			};
			auto scopeIt = [&parentScope, id]() -> decltype(parentScope.subscopes)::iterator {
				for(auto it = parentScope.subscopes.begin(); it != parentScope.subscopes.end(); ++it) {
					if(it->id == id) return it;
				}
				throw std::runtime_error("UNREACHABLE CODE!!!! HOW DID YOU GET HERE!!!!!");
			}();
			purgeScope(*scopeIt);
			parentScope.subscopes.erase(scopeIt);
		}
	}
}