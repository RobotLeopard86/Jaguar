#include "libjaguar/Document.hpp"
#include "libjaguar/Decoder.hpp"
#include "libjaguar/Encoder.hpp"
#include "libjaguar/Index.hpp"
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

		//TODO: implement methods
		void String(uint64_t id, std::ostream& out, std::size_t chunkSize, std::size_t offset) override {}
		void Buffer(uint64_t id, std::ostream& out, std::size_t chunkSize, std::size_t offset) override {}
		bool Boolean(uint64_t id) override {
			return true;
		}
		int64_t SignedInt(uint64_t id, uint8_t bits) override {
			return 37;
		}
		uint64_t UnsignedInt(uint64_t id, uint8_t bits) override {
			return 37;
		}
		float Float32(uint64_t id) override {
			return std::numbers::pi_v<float>;
		}
		double Float64(uint64_t id) override {
			return std::numbers::pi_v<double>;
		}
		int64_t SignedIntVec(uint64_t id, VecComponent component, uint8_t bits) override {
			return 41;
		}
		uint64_t UnsignedIntVec(uint64_t id, VecComponent component, uint8_t bits) override {
			return 41;
		}
		float Float32Vec(uint64_t id, VecComponent component) override {
			return std::numbers::phi_v<float>;
		}
		double Float64Vec(uint64_t id, VecComponent component) override {
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