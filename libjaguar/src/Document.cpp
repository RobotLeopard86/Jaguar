#include "libjaguar/Document.hpp"
#include "libjaguar/Decoder.hpp"
#include "libjaguar/Encoder.hpp"
#include "libjaguar/Index.hpp"
#include "libjaguar/MathTypes.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/Writer.hpp"
#include "Utilities.hpp"

#include <numbers>
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
	Document::ValueStorage Document::From(const std::string& val) {
		ValueStorage vs = {.materialized = true, .mem = std::vector<std::byte> {val.size()}, .inStream = 0};
		std::memcpy(vs.mem.data(), val.data(), val.size());
		return vs;
	}

	template<>
	std::string Document::To(const ValueStorage& storage) {
		std::string out;
		out.resize(storage.mem.size());
		std::memcpy(out.data(), storage.mem.data(), out.size());
		return out;
	}

	class Document::DocPayloadProvider : public PayloadProvider {
	  public:
		DocPayloadProvider(Document* doc)
		  : doc(doc) {}

		void String(uint64_t id, std::ostream& out, std::size_t chunkSize, std::size_t offset) override {}
		void Buffer(uint64_t id, std::ostream& out, std::size_t chunkSize, std::size_t offset) override {}
		bool Boolean(uint64_t id) override {
			if(doc->_ValInfoInternal(id).type != TypeTag::Boolean) throw std::runtime_error("Requested a boolean for a value that is not one!");
			return doc->To<bool>(doc->_QueryInternal(id));
		}
		int64_t SignedInt(uint64_t id, uint8_t bits) override {
			switch(bits) {
				case 8:
					if(doc->_ValInfoInternal(id).type != TypeTag::SInt8) throw std::runtime_error("Requested an SInt8 for a value that is not one!");
					break;
				case 16:
					if(doc->_ValInfoInternal(id).type != TypeTag::SInt16) throw std::runtime_error("Requested an SInt16 for a value that is not one!");
					break;
				case 32:
					if(doc->_ValInfoInternal(id).type != TypeTag::SInt32) throw std::runtime_error("Requested an SInt32 for a value that is not one!");
					break;
				case 64:
					if(doc->_ValInfoInternal(id).type != TypeTag::SInt64) throw std::runtime_error("Requested an SInt64 for a value that is not one!");
					break;
				default: break;
			}
			return doc->To<int64_t>(doc->_QueryInternal(id));
		}
		uint64_t UnsignedInt(uint64_t id, uint8_t bits) override {
			switch(bits) {
				case 8:
					if(doc->_ValInfoInternal(id).type != TypeTag::UInt8) throw std::runtime_error("Requested a UInt8 for a value that is not one!");
					break;
				case 16:
					if(doc->_ValInfoInternal(id).type != TypeTag::UInt16) throw std::runtime_error("Requested a UInt16 for a value that is not one!");
					break;
				case 32:
					if(doc->_ValInfoInternal(id).type != TypeTag::UInt32) throw std::runtime_error("Requested a UInt32 for a value that is not one!");
					break;
				case 64:
					if(doc->_ValInfoInternal(id).type != TypeTag::UInt64) throw std::runtime_error("Requested a UInt64 for a value that is not one!");
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
		int64_t SignedIntVec(uint64_t id, VecComponent component, uint8_t bits) override {
			const ValueEntry& ve = doc->_ValInfoInternal(id);
			if(ve.type != TypeTag::Vector) throw std::runtime_error("Requested a vector for a value that is not one!");
			switch(component) {
				case VecComponent::Z:
					if(ve.width < 3) throw std::runtime_error("Requested Z component of less than 3-component vector vector");
				case VecComponent::W:
					if(ve.width != 4) throw std::runtime_error("Requested W component of non-4-component vector");
					break;
				default: break;
			}
			switch(bits) {
				case 8:
					if(ve.elementType != TypeTag::SInt8) throw std::runtime_error("Requested a vector of SInt8 for a value that is not one!");
					break;
				case 16:
					if(ve.elementType != TypeTag::SInt16) throw std::runtime_error("Requested a vector of SInt16 for a value that is not one!");
					break;
				case 32:
					if(ve.elementType != TypeTag::SInt32) throw std::runtime_error("Requested a vector of SInt32 for a value that is not one!");
					break;
				case 64:
					if(ve.elementType != TypeTag::SInt64) throw std::runtime_error("Requested a vector of SInt64 for a value that is not one!");
					break;
				default: break;
			}
			return 41;
		}
		uint64_t UnsignedIntVec(uint64_t id, VecComponent component, uint8_t bits) override {
			const ValueEntry& ve = doc->_ValInfoInternal(id);
			if(ve.type != TypeTag::Vector) throw std::runtime_error("Requested a vector for a value that is not one!");
			switch(component) {
				case VecComponent::Z:
					if(ve.width < 3) throw std::runtime_error("Requested Z component of less than 3-component vector vector");
				case VecComponent::W:
					if(ve.width != 4) throw std::runtime_error("Requested W component of non-4-component vector");
					break;
				default: break;
			}
			switch(bits) {
				case 8:
					if(ve.elementType != TypeTag::UInt8) throw std::runtime_error("Requested a vector of UInt8 for a value that is not one!");
					break;
				case 16:
					if(ve.elementType != TypeTag::UInt16) throw std::runtime_error("Requested a vector of UInt16 for a value that is not one!");
					break;
				case 32:
					if(ve.elementType != TypeTag::UInt32) throw std::runtime_error("Requested a vector of UInt32 for a value that is not one!");
					break;
				case 64:
					if(ve.elementType != TypeTag::UInt64) throw std::runtime_error("Requested a vector of UInt64 for a value that is not one!");
					break;
				default: break;
			}
			return 41;
		}
		float Float32Vec(uint64_t id, VecComponent component) override {
			const ValueEntry& ve = doc->_ValInfoInternal(id);
			if(ve.type != TypeTag::Vector || ve.elementType != TypeTag::Float32) throw std::runtime_error("Requested a vector of Float32 for a value that is not one!");
			switch(component) {
				case VecComponent::Z:
					if(ve.width < 3) throw std::runtime_error("Requested Z component of less than 3-component vector vector");
				case VecComponent::W:
					if(ve.width != 4) throw std::runtime_error("Requested W component of non-4-component vector");
					break;
				default: break;
			}
			return std::numbers::phi_v<float>;
		}
		double Float64Vec(uint64_t id, VecComponent component) override {
			const ValueEntry& ve = doc->_ValInfoInternal(id);
			if(ve.type != TypeTag::Vector || ve.elementType != TypeTag::Float64) throw std::runtime_error("Requested a vector of Float64 for a value that is not one!");
			switch(component) {
				case VecComponent::Z:
					if(ve.width < 3) throw std::runtime_error("Requested Z component of less than 3-component vector vector");
				case VecComponent::W:
					if(ve.width != 4) throw std::runtime_error("Requested W component of non-4-component vector");
					break;
				default: break;
			}
			return std::numbers::phi_v<double>;
		}
		int64_t SignedIntMat(uint64_t id, uint8_t x, uint8_t y, uint8_t bits) override {
			return 19;
		}
		uint64_t UnsignedIntMat(uint64_t id, uint8_t x, uint8_t y, uint8_t bits) override {
			return 19;
		}
		float Float32Mat(uint64_t id, uint8_t x, uint8_t y) override {
			return std::numbers::e_v<float>;
		}
		double Float64Mat(uint64_t id, uint8_t x, uint8_t y) override {
			return std::numbers::e_v<double>;
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
			if(ValueStorage& vs = storage[id]; vs.materialized) {
				//Something happens (we nab the value)
				reader.value()->seekg(vs.inStream);
				{
					const ValueEntry& typeInfo = _ValInfoInternal(id);
					MathTypeDescriptor mtd = {};
					if((static_cast<uint8_t>(typeInfo.type) >> 4) == 0x4) {
						mtd.width = typeInfo.width;
						mtd.height = typeInfo.height;
						mtd.type = typeInfo.elementType;
					}
					vs.mem.resize(CalcValueSize(typeInfo.type, mtd, static_cast<uint8_t>(typeInfo.type) <= 0xC ? typeInfo.size : 0));
				}
				//TODO: Read and parse value
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
}