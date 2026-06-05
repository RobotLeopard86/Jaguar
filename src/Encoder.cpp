#include "libjaguar/Encoder.hpp"
#include "libjaguar/Index.hpp"
#include "libjaguar/MathTypes.hpp"
#include "libjaguar/StructuredTypeLayout.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/ValueHeader.hpp"

#include <set>
#include <sstream>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace libjaguar {
	Encoder::Encoder(Writer&& writer) : writer(std::move(writer)), writerValid(true) {}

	Encoder::Encoder(Encoder&& other) : writer(std::move(other.writer)), writerValid(true) {
		other.writerValid = false;
	}

	Encoder& Encoder::operator=(Encoder&& other) {
		if(this != &other) {
			writer = std::move(other.writer);
			writerValid = true;
			other.writerValid = false;
		}
		return *this;
	}

	Writer&& Encoder::ReleaseWriter() && {
		if(!writerValid) throw std::runtime_error("Encoder has no valid writer!");
		return std::move(writer);
	}

	void Encoder::_WriteNum(TypeTag type, uint64_t asBits) {
		switch(type) {
			case TypeTag::Float32:
				writer.WriteInteger<uint32_t>(static_cast<uint32_t>(asBits));
				break;
			case TypeTag::Float64:
				writer.WriteInteger<uint64_t>(asBits);
				break;
			case TypeTag::SInt8:
			case TypeTag::SInt16:
			case TypeTag::SInt32:
			case TypeTag::SInt64: {
				//This looks weird but it's just converting the type tag to the approriate number of bytes for the type
				uint8_t bytes = std::pow(2, (static_cast<uint8_t>(type) & 0xF) - 0xA);

				//Sign extend the value to ensure negative values are handled properly
				uint64_t mask = (bytes == 8) ? ~0ull : ((1ull << (bytes * 8)) - 1);
				uint64_t cvt = asBits & mask;
				uint64_t signBitExtended = 1ull << ((bytes * 8) - 1);
				if(cvt & signBitExtended) cvt |= ~mask;
				int64_t trueValue = static_cast<int64_t>(cvt);

				if(std::abs(trueValue) > (std::pow(2, bytes * 8) / 2 - 1) && trueValue > std::pow(2, bytes * 8) / -2) throw std::runtime_error("Provider returned value too large for the requested type!");
				switch(bytes) {
					case 1:
						writer.WriteInteger<uint8_t>(asBits);
						break;
					case 2:
						writer.WriteInteger<uint16_t>(asBits);
						break;
					case 4:
						writer.WriteInteger<uint32_t>(asBits);
						break;
					case 8:
						writer.WriteInteger<uint64_t>(asBits);
						break;
				}
				break;
			}
			case TypeTag::UInt8:
			case TypeTag::UInt16:
			case TypeTag::UInt32:
			case TypeTag::UInt64: {
				//This looks weird but it's just converting the type tag to the approriate number of bytes for the type
				uint8_t bytes = std::pow(2, (static_cast<uint8_t>(type) & 0xF) - 0xA);
				if(asBits > (std::pow(2, bytes * 8) - 1)) throw std::runtime_error("Provider returned value too large for the requested type!");
				switch(bytes) {
					case 1:
						writer.WriteInteger<int8_t>(asBits);
						break;
					case 2:
						writer.WriteInteger<int16_t>(asBits);
						break;
					case 4:
						writer.WriteInteger<int32_t>(asBits);
						break;
					case 8:
						writer.WriteInteger<int64_t>(asBits);
						break;
				}
				break;
			}
			default: break;
		}
	}

	void Encoder::_WriteValue(const ValueEntry& entry, PayloadProvider* provider, bool forList) {
		//Check type
		if((static_cast<uint8_t>(entry.type) & 0xF0) >> 4 == 0x3) throw std::runtime_error("Cannot write a scope type as a value!");

		//Build header
		ValueHeader header = {};
		header.name = entry.name;
		header.type = entry.type;
		if(static_cast<uint8_t>(entry.type) <= 0xC) {
			//Buffer objects
			if(entry.size >= std::pow(2, 24)) throw std::runtime_error("String is too long!");
			header.size = entry.size;
		} else if(entry.type == TypeTag::Vector || entry.type == TypeTag::Matrix) {
			if(uint8_t asByte = static_cast<uint8_t>(entry.elementType); asByte < 0x0E || asByte > 0x2D) throw std::runtime_error("Invalid element type for vector/matrix!");
			header.elementType = entry.elementType;
			if(entry.width < 2 || entry.width > 4) throw std::runtime_error("Invalid vector/matrix width!");
			header.width = entry.width;
			if(entry.type == TypeTag::Matrix) {
				if(entry.height < 2 || entry.height > 4) throw std::runtime_error("Invalid matrix height!");
				header.height = entry.height;
			}
		}

		//Write the header
		if(!forList)
			writer.WriteHeader(header);
		else if(static_cast<uint8_t>(entry.type) <= 0xC || entry.type == TypeTag::UnstructuredObj)
			writer.WriteHeader(header, true);

		//Start writing the data
		constexpr std::size_t chunkSize = 64 * 1024;//64 KiB (one KiB is 1024 bytes)
		switch(header.type) {
			case TypeTag::String: {
				if(provider->UseStream2StreamTransfer(entry.id)) {
					std::istream* stream = provider->StringViaStream(entry.id);
					writer.WriteStringFromStream(stream, header.size);
					delete stream;
				} else {
					std::size_t current = 0;
					std::ostringstream intermediate;
					while(current < header.size) {
						//Clear intermediate buffer
						intermediate.str("");

						//Read into intermediate
						std::size_t len = std::min(header.size - current, chunkSize);
						provider->String(entry.id, intermediate, len, current);
						if(intermediate.str().size() != len) throw std::runtime_error("Incorrect amount of data provided by payload provider for string chunk!");

						//Write data slice
						writer.WriteString(intermediate.str());

						//Increment progress counter
						current += len;
					}
				}
				break;
			}
			case TypeTag::ByteBuffer: {
				if(provider->UseStream2StreamTransfer(entry.id)) {
					std::istream* stream = provider->BufferViaStream(entry.id);
					writer.WriteBufferFromStream(stream, header.size);
					delete stream;
				} else {
					std::size_t current = 0;
					std::stringstream intermediate;
					while(current < header.size) {
						//Clear intermediate buffer
						intermediate.str("");

						//Read into intermediate
						std::size_t len = std::min(header.size - current, chunkSize);
						provider->Buffer(entry.id, intermediate, len, current);
						if(intermediate.str().size() != len) throw std::runtime_error("Incorrect amount of data provided by payload provider for string chunk!");

						//Write data slice
						writer.WriteBufferFromStream(&intermediate, len);

						//Increment progress counter
						current += len;
					}
				}
				break;
			}
			case TypeTag::Boolean:
				writer.WriteBool(provider->Boolean(entry.id));
				break;
			case TypeTag::Float32:
				_WriteNum(TypeTag::Float32, std::bit_cast<uint32_t, float>(provider->Float32(entry.id)));
				break;
			case TypeTag::Float64:
				_WriteNum(TypeTag::Float64, std::bit_cast<uint64_t, double>(provider->Float64(entry.id)));
				break;
			case TypeTag::SInt8:
			case TypeTag::SInt16:
			case TypeTag::SInt32:
			case TypeTag::SInt64:
			case TypeTag::UInt8:
			case TypeTag::UInt16:
			case TypeTag::UInt32:
			case TypeTag::UInt64:
				_WriteNum(header.type, provider->Integer(entry.id, std::pow(2, (static_cast<uint8_t>(header.type) & 0xF) - 0xA) * 8, static_cast<uint8_t>(header.type) < 0x20));
				break;
			case TypeTag::Vector: {
				std::vector<uint64_t> nums;
				switch(header.width) {
					case 2:
						switch(header.elementType) {
							case TypeTag::Float32: {
								Vector<float, 2> vec = provider->Float32Vec2(entry.id);
								nums.push_back(std::bit_cast<uint32_t, float>(vec.x));
								nums.push_back(std::bit_cast<uint32_t, float>(vec.y));
								break;
							}
							case TypeTag::Float64: {
								Vector<double, 2> vec = provider->Float64Vec2(entry.id);
								nums.push_back(std::bit_cast<uint64_t, double>(vec.x));
								nums.push_back(std::bit_cast<uint64_t, double>(vec.y));
								break;
							}
							case TypeTag::SInt8:
							case TypeTag::SInt16:
							case TypeTag::SInt32:
							case TypeTag::SInt64:
							case TypeTag::UInt8:
							case TypeTag::UInt16:
							case TypeTag::UInt32:
							case TypeTag::UInt64: {
								Vector<uint64_t, 2> vec = provider->IntegerVec2(entry.id, std::pow(2, (static_cast<uint8_t>(header.elementType) & 0xF) - 0xA) * 8, static_cast<uint8_t>(header.elementType) < 0x20);
								nums.push_back(vec.x);
								nums.push_back(vec.y);
								break;
							}
							default: break;
						}
						break;
					case 3:
						switch(header.elementType) {
							case TypeTag::Float32: {
								Vector<float, 3> vec = provider->Float32Vec3(entry.id);
								nums.push_back(std::bit_cast<uint32_t, float>(vec.x));
								nums.push_back(std::bit_cast<uint32_t, float>(vec.y));
								nums.push_back(std::bit_cast<uint32_t, float>(vec.z));
								break;
							}
							case TypeTag::Float64: {
								Vector<double, 3> vec = provider->Float64Vec3(entry.id);
								nums.push_back(std::bit_cast<uint64_t, double>(vec.x));
								nums.push_back(std::bit_cast<uint64_t, double>(vec.y));
								nums.push_back(std::bit_cast<uint64_t, double>(vec.z));
								break;
							}
							case TypeTag::SInt8:
							case TypeTag::SInt16:
							case TypeTag::SInt32:
							case TypeTag::SInt64:
							case TypeTag::UInt8:
							case TypeTag::UInt16:
							case TypeTag::UInt32:
							case TypeTag::UInt64: {
								Vector<uint64_t, 3> vec = provider->IntegerVec3(entry.id, std::pow(2, (static_cast<uint8_t>(header.elementType) & 0xF) - 0xA) * 8, static_cast<uint8_t>(header.elementType) < 0x20);
								nums.push_back(vec.x);
								nums.push_back(vec.y);
								nums.push_back(vec.z);
								break;
							}
							default: break;
						}
						break;
					case 4:
						switch(header.elementType) {
							case TypeTag::Float32: {
								Vector<float, 4> vec = provider->Float32Vec4(entry.id);
								nums.push_back(std::bit_cast<uint32_t, float>(vec.x));
								nums.push_back(std::bit_cast<uint32_t, float>(vec.y));
								nums.push_back(std::bit_cast<uint32_t, float>(vec.z));
								nums.push_back(std::bit_cast<uint32_t, float>(vec.w));
								break;
							}
							case TypeTag::Float64: {
								Vector<double, 4> vec = provider->Float64Vec4(entry.id);
								nums.push_back(std::bit_cast<uint64_t, double>(vec.x));
								nums.push_back(std::bit_cast<uint64_t, double>(vec.y));
								nums.push_back(std::bit_cast<uint64_t, double>(vec.z));
								nums.push_back(std::bit_cast<uint64_t, double>(vec.w));
								break;
							}
							case TypeTag::SInt8:
							case TypeTag::SInt16:
							case TypeTag::SInt32:
							case TypeTag::SInt64:
							case TypeTag::UInt8:
							case TypeTag::UInt16:
							case TypeTag::UInt32:
							case TypeTag::UInt64: {
								Vector<uint64_t, 4> vec = provider->IntegerVec4(entry.id, std::pow(2, (static_cast<uint8_t>(header.elementType) & 0xF) - 0xA) * 8, static_cast<uint8_t>(header.elementType) < 0x20);
								nums.push_back(vec.x);
								nums.push_back(vec.y);
								nums.push_back(vec.z);
								nums.push_back(vec.w);
								break;
							}
							default: break;
						}
						break;
				}
				for(uint8_t i = 0; i < nums.size(); ++i) {
					_WriteNum(header.elementType, nums[i]);
				}
				break;
			}
			case TypeTag::Matrix: {
				for(uint8_t x = 0; x < header.width; ++x) {
					for(uint8_t y = 0; y < header.height; ++y) {
						uint64_t val = [&]() -> uint64_t {
							switch(header.elementType) {
								case TypeTag::Float32:
									return (uint64_t)std::bit_cast<uint32_t, float>(provider->Float32Mat(entry.id, x, y));
								case TypeTag::Float64:
									return std::bit_cast<uint64_t, double>(provider->Float64Mat(entry.id, x, y));
								case TypeTag::SInt8:
								case TypeTag::SInt16:
								case TypeTag::SInt32:
								case TypeTag::SInt64:
								case TypeTag::UInt8:
								case TypeTag::UInt16:
								case TypeTag::UInt32:
								case TypeTag::UInt64:
									return provider->IntegerMat(entry.id, x, y, std::pow(2, (static_cast<uint8_t>(header.elementType) & 0xF) - 0xA) * 8, static_cast<uint8_t>(header.elementType) < 0x20);
								default: return 0;
							}
						}();
						_WriteNum(header.elementType, val);
					}
				}
				break;
			}
			default: break;
		}
	}

	void Encoder::_WriteObj(const Index& index, const ScopeEntry& entry, PayloadProvider* provider, bool forList) {
		//Write appropriate header for structured or unstructured types
		if(entry.typeID.empty()) {
			//Create and write header
			ValueHeader header = {};
			header.name = entry.name;
			header.type = TypeTag::UnstructuredObj;
			header.fieldCount = entry.subvalues.size() + entry.subscopes.size();
			writer.WriteHeader(header, forList);
		} else {
			//Check type
			if(!index.types.contains(entry.typeID)) throw std::runtime_error("Cannot encode structured object subscope using undeclared type!");

			//Create and write header
			if(!forList) {
				ValueHeader header = {};
				header.name = entry.name;
				header.type = TypeTag::StructuredObj;
				header.typeID = entry.typeID;
				writer.WriteHeader(header);
			}
		}

		//Write the subscope
		_WriteScope(index, entry, provider);
	}

	void Encoder::_WriteScope(const Index& index, const ScopeEntry& entry, PayloadProvider* provider) {
		//We use this to track what field names are used to avoid duplicates
		std::unordered_map<std::string, bool> fieldTracker;

		//Structured object preparation
		StructuredTypeLayout layout = {};
		if(!entry.typeID.empty()) {
			layout = index.types.at(entry.typeID);
			for(const StructuredTypeLayout::Field& field : layout.fields) {
				fieldTracker.insert_or_assign(field.name, false);
			}
		}

		//Write all values
		for(const ValueEntry& val : entry.subvalues) {
			//Check for duplication
			if(fieldTracker.contains(val.name) && fieldTracker[val.name])
				throw std::runtime_error("Detected duplicate field in scope!");
			else
				fieldTracker[val.name] = true;

			//Write value
			_WriteValue(val, provider);
		}

		//Write all scopes
		for(const ScopeEntry& scope : entry.subscopes) {
			//Check for duplication
			if(fieldTracker.contains(scope.name) && fieldTracker[scope.name])
				throw std::runtime_error("Detected duplicate field in scope!");
			else
				fieldTracker[scope.name] = true;

			//Check that we're not nesting too deep
			if(++nest > 64) throw std::runtime_error("Nesting too deep!");

			//Behavior varies if list or not
			if(scope.list) {
				//Create header object
				ValueHeader header = {};
				header.name = scope.name;
				header.type = TypeTag::List;
				header.elementType = scope.listElementType;
				if(header.elementType == TypeTag::List) throw std::runtime_error("Lists may not directly contain other lists!");
				if(header.elementType == TypeTag::StructuredObjTypeDecl) throw std::runtime_error("Lists may not contain type declarations!");
				if(header.elementType == TypeTag::ScopeBoundary) throw std::runtime_error("Lists of scope boundaries cannot exist!");
				header.size = (IsValue(header.elementType) ? scope.subvalues.size() : 0) + (!IsValue(header.elementType) ? scope.subscopes.size() : 0);
				if(header.elementType == TypeTag::StructuredObj) {
					if(!index.types.contains(scope.typeID)) throw std::runtime_error("Cannot encode list of structured objects using undeclared type!");
					header.typeID = scope.typeID;
				} else if(header.elementType == TypeTag::Vector || header.elementType == TypeTag::Matrix) {
					if(uint8_t asByte = static_cast<uint8_t>(entry.listMathData.type); asByte < 0x0E || asByte > 0x2D) throw std::runtime_error("Cannot encode vectors/matrices with invalid element type!");
					header.nestedElementType = entry.listMathData.type;
					if(entry.listMathData.width < 2 || entry.listMathData.width > 4) throw std::runtime_error("Cannot encode vectors/matrices with invalid width!");
					header.width = entry.listMathData.width;
					if(entry.listElementType == TypeTag::Matrix) {
						if(entry.listMathData.height < 2 || entry.listMathData.height > 4) throw std::runtime_error("Cannot encode matrices with invalid height!");
						header.height = entry.listMathData.height;
					}
				}

				//Write it
				writer.WriteHeader(header);

				//Write each element
				if(IsValue(header.elementType)) {
					for(const ValueEntry& val : scope.subvalues) {
						_WriteValue(val, provider, true);
					}
				} else {
					for(const ScopeEntry& subscope : scope.subscopes) {
						_WriteObj(index, subscope, provider, true);
					}
				}

				//Write scope boundary
				writer->put(static_cast<uint8_t>(TypeTag::ScopeBoundary));
			} else {
				_WriteObj(index, scope, provider);
			}

			//All's well; roll back nesting counter
			--nest;
		}

		//Ensure that all fields were written
		for(const auto& [_, written] : fieldTracker) {
			if(!written) throw std::runtime_error("While encoding, left scope without writing all required fields!");
		}

		//If not root, write boundary
		if(entry.id != index.root.id) writer->put(static_cast<uint8_t>(TypeTag::ScopeBoundary));
	}

	//Implement DFS algorithm for type ordering
	enum class VisitState {
		Unvisited,
		Visiting,
		Visited
	};

	void DFS(const std::string& node, const std::unordered_map<std::string, std::vector<std::string>>& graph, std::unordered_map<std::string, VisitState>& state, std::vector<std::string>& result) {
		//Pre-screening
		switch(state[node]) {
			//Cycle detection
			case VisitState::Visiting: throw std::runtime_error("Dependency cycle detected in type list!");

			//Known types are good
			case VisitState::Visited: return;

			//This is a new type
			case VisitState::Unvisited:
				state[node] = VisitState::Visiting;
				break;
		}

		//Process dependencies
		for(const auto& dep : graph.at(node)) {
			DFS(dep, graph, state, result);
		}

		//We're done visiting (and thus dependencies are handled)
		state[node] = VisitState::Visited;
		result.push_back(node);
	}

	void Encoder::_Write(const Index& index, PayloadProvider* provider) {
		//We have to sort types to ensure dependencies are handled in the right order since types can depend on each other
		//So the first step is to find the dependencies
		std::unordered_map<std::string, std::vector<std::string>> dependencies;
		for(const auto& [id, layout] : index.types) {
			//Validate the layout first
			if(!ValidateTypeLayout(layout)) throw std::runtime_error("Invalid type layout passed to encoder!");

			//Find dependencies
			std::set<std::string> deps;
			for(const StructuredTypeLayout::Field& field : layout.fields) {
				if(field.type == TypeTag::StructuredObj || (field.type == TypeTag::List && field.elementType == TypeTag::StructuredObj)) {
					deps.insert(field.typeID);
				}
			}

			//Add to dependency graph
			std::vector<std::string> depsVec(deps.begin(), deps.end());
			dependencies[id] = depsVec;
		}

		//Now we can run the sort (DFS algorithm)
		std::unordered_map<std::string, VisitState> state;
		std::vector<std::string> sorted;
		for(const auto& [id, _] : dependencies) {
			state[id] = VisitState::Unvisited;
		}
		for(const auto& [id, _] : dependencies) {
			//Skip already known types (DFS would return but this saves us creating a stack frame)
			if(state[id] == VisitState::Unvisited) {
				DFS(id, dependencies, state, sorted);
			}
		}

		//Write types (now sorted!)
		for(const std::string& id : sorted) {
			//Get the layout
			const StructuredTypeLayout& layout = index.types.at(id);

			//Create and write type decl header
			ValueHeader header = {};
			header.name = id;
			header.type = TypeTag::StructuredObjTypeDecl;
			header.fieldCount = layout.fields.size();
			writer.WriteHeader(header);

			//Start writing fields
			for(const StructuredTypeLayout::Field& field : layout.fields) {
				//We can just use default header behavior most of the time except for a few edge-cases
				if(static_cast<uint8_t>(field.type) <= 0xC || field.type == TypeTag::List || field.type == TypeTag::UnstructuredObj) {
					//Write element TypeTag
					writer->put(static_cast<uint8_t>(field.type));

					//Write element name
					writer.WriteInteger(static_cast<uint8_t>(field.name.size()));
					writer.WriteString(field.name);

					//Keep going with lists specifically
					if(field.type == TypeTag::List) {
						//Element type
						writer->put(static_cast<uint8_t>(header.elementType));

						//Type-specific data
						if(field.elementType == TypeTag::StructuredObj) {
							writer.WriteInteger(static_cast<uint8_t>(field.typeID.size()));
							writer.WriteString(field.typeID);
						} else if(field.elementType == TypeTag::Vector || field.elementType == TypeTag::Matrix) {
							writer->put(static_cast<uint8_t>(field.nestedElementType));
							writer.WriteInteger(static_cast<uint8_t>(field.width));
							if(field.elementType == TypeTag::Matrix) writer.WriteInteger(static_cast<uint8_t>(field.height));
						}
					}
				} else {
					//Make and write header
					ValueHeader fieldHead = {};
					fieldHead.name = field.name;
					fieldHead.type = field.type;
					if(field.type == TypeTag::StructuredObj) {
						fieldHead.typeID = field.typeID;
					} else {
						//Vectors & matrices
						fieldHead.elementType = field.elementType;
						fieldHead.width = field.width;
						fieldHead.height = field.height;
					}
					writer.WriteHeader(fieldHead);
				}
			}


			//Write scope boundary
			writer->put(static_cast<uint8_t>(TypeTag::ScopeBoundary));
		}

		//Root scope
		nest = 0;
		_WriteScope(index, index.root, provider);
	}
}