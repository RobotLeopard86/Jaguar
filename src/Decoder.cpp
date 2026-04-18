#include "libjaguar/Decoder.hpp"
#include "libjaguar/Index.hpp"
#include "libjaguar/StructuredTypeLayout.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/ValueHeader.hpp"

#include <exception>
#include <set>
#include <stdexcept>
#include <cmath>
#include <unordered_map>
#include <sstream>

#define TYPE_OR_LIST_OF(cmp, tp) cmp.type == TypeTag::tp || (cmp.type == TypeTag::List && cmp.elementType == TypeTag::tp)
#define DECODE_ERROR(msg)                                                                              \
	{                                                                                                  \
		std::stringstream ss;                                                                          \
		ss << "[Decode Error] At offset 0x" << std::hex << reader->tellg() << std::dec << ": " << msg; \
		throw std::runtime_error(ss.str());                                                            \
	}

namespace libjaguar {
	Decoder::Decoder(Reader&& reader) : reader(std::move(reader)), readerValid(true), failFlag(false) {}

	Decoder::Decoder(Decoder&& other) : reader(std::move(other.reader)), readerValid(true), failFlag(false) {
		other.readerValid = false;
	}

	Decoder& Decoder::operator=(Decoder&& other) {
		if(this != &other) {
			reader = std::move(other.reader);
			readerValid = true;
			failFlag = false;
			other.readerValid = false;
		}
		return *this;
	}

	Reader&& Decoder::ReleaseReader() && {
		if(!readerValid) throw std::runtime_error("Decoder has no valid reader!");
		return std::move(reader);
	}

	ValueEntry Decoder::_ParseValueInternal(const ValueHeader& header, const std::string& scopePath) {
		//Entry setup
		ValueEntry entry = {};
		entry.type = header.type;
		entry.name = header.name;
		entry.streamBeginPosition = reader->tellg();
		if(!scopePath.empty()) {
			if(scopePath.ends_with("["))
				entry.id = GenIndexID(scopePath + header.name + "]");
			else
				entry.id = GenIndexID(scopePath + "." + header.name);
		} else {
			entry.id = GenIndexID(entry.name);
		}

		//Vector/matrix handling
		if(header.type == TypeTag::Vector || header.type == TypeTag::Matrix) {
			if(uint8_t asByte = static_cast<uint8_t>(header.elementType); asByte < 0x0E || asByte > 0x2D) DECODE_ERROR("Encountered vector/matrix with invalid element type!");
			entry.elementType = header.elementType;

			if(header.width < 2 || header.width > 4) DECODE_ERROR("Encountered vector/matrix with invalid width!");
			entry.width = header.width;
			if(header.type == TypeTag::Matrix) {
				if(header.height < 2 || header.height > 4) DECODE_ERROR("Encountered matrix with invalid height!");
				entry.height = header.height;
			} else {
				entry.height = 1;
			}
		}

		//Buffer objects and size checks
		if(static_cast<uint8_t>(header.type) <= 0xC) entry.size = header.size;
		if(header.type == TypeTag::String && entry.size >= std::pow(2, 24)) DECODE_ERROR("Encountered a string that is too long (> 24-bit integer limit!)");

		//Calculate size of value body to skip
		std::size_t skipAmountBytes = 0;
		if(header.type != TypeTag::ScopeBoundary) {
			MathTypeDescriptor mtd = {};
			if((static_cast<uint8_t>(header.type) >> 4) == 0x4) {
				mtd.width = entry.width;
				mtd.height = entry.height;
				mtd.type = entry.elementType;
			}
			skipAmountBytes = CalcValueSize(header.type, mtd, static_cast<uint8_t>(header.type) <= 0xC ? header.size : 0);
		}
		reader->ignore(skipAmountBytes);

		return entry;
	}

	void Decoder::_ParseScopeInternal(ScopeEntry& scope, ScopeExpectations expectations, const std::string& scopePath) {
		//We use this to track what field names are used to avoid duplicates
		std::unordered_map<std::string, bool> fieldTracker;

		//Structured object preparation
		StructuredTypeLayout layout = {};
		if(expectations.type == TypeTag::StructuredObj) {
			layout = index->types[expectations.typeID];
			for(const StructuredTypeLayout::Field& field : layout.fields) {
				fieldTracker.insert_or_assign(field.name, false);
			}
		}

		//Continuously read the next header
		while(true) {
			//Get next header
			ValueHeader header = {};
			try {
				header = reader.ReadHeader();
			} catch(...) {
				if(reader->eof()) {
					if(!expectations.rootFlag) DECODE_ERROR("Unexpected EOF in nested scope!");
					return;
				}
				std::rethrow_exception(std::current_exception());
			}
			std::size_t encounteredFields = scope.subscopes.size() + scope.subvalues.size();

			//If we see a scope boundary, check position
			if(header.type == TypeTag::ScopeBoundary) {
				//Is this root
				if(expectations.rootFlag) DECODE_ERROR("Unexpected scope boundary in root scope!");

				//Have we seen the expected number of values yet?
				//Return if so because the scope is done
				if(encounteredFields == expectations.fieldCount) {
					//For structured objects, check that all fields were found first (other measures should mean this never needs to be checked, but just to be safe)
					if(expectations.type == TypeTag::StructuredObj) {
						for(const auto& [_, seen] : fieldTracker) {
							if(!seen) DECODE_ERROR("Exited structured object scope without all required fields present!");
						}
					}
					return;
				}

				//If we're less, this is simply a case of early scope termination
				//We still do an if-check to throw the appropriate exception in case we passed the expected field count without a boundary
				else if(encounteredFields < expectations.fieldCount)
					DECODE_ERROR("Early scope boundary detected!")
				else
					//This really shouldn't happen because we try to anticipate excess fields early
					DECODE_ERROR("Late scope boundary detected!")
			}

			//Check expected field count to make sure we're not over
			if(encounteredFields > expectations.fieldCount) DECODE_ERROR("Excess number of fields detected in scope!");

			//Check names to avoid duplication
			if(header.type != TypeTag::StructuredObjTypeDecl && fieldTracker.contains(header.name) && fieldTracker[header.name])
				DECODE_ERROR("Detected duplicate field in scope!")
			else
				fieldTracker[header.name] = true;

			//For structured objects: check that the type actually contains this value
			if(expectations.type == TypeTag::StructuredObj) {
				//Try to find a field matching this header
				bool ok = false;
				for(const StructuredTypeLayout::Field& field : layout.fields) {
					//If any check fails, go to next iteration
					if(field.name.compare(header.name) != 0) continue;
					if(field.type != header.type) continue;
					if(field.type == TypeTag::Vector || field.type == TypeTag::Matrix) {
						if(field.elementType != header.elementType) continue;
						if(field.width != header.width) continue;
						if(field.type == TypeTag::Matrix && field.height != header.height) continue;
					} else if(field.type == TypeTag::StructuredObj) {
						if(field.typeID != header.typeID) continue;
					} else if(field.type == TypeTag::List) {
						if(field.elementType == TypeTag::Vector || field.elementType == TypeTag::Matrix) {
							if(field.nestedElementType != header.nestedElementType) continue;
							if(field.width != header.width) continue;
							if(field.type == TypeTag::Matrix && field.height != header.height) continue;
						} else if(field.elementType == TypeTag::StructuredObj) {
							if(field.typeID != header.typeID) continue;
						}
					}

					//All checks passed, ok!
					ok = true;
					break;
				}
				if(!ok) DECODE_ERROR("While parsing structured object, encountered a field not present in the type declaration!");
			}

			//Handle what we found
			if(IsValue(header.type)) {
				//Parse the value
				ValueEntry entry = _ParseValueInternal(header, scopePath);

				//Add entry
				scope.subvalues.push_back(std::move(entry));
			} else if(header.type == TypeTag::StructuredObjTypeDecl) {
				//Check that there's not already a type with this name
				if(index->types.contains(header.name)) DECODE_ERROR("Encountered a type declaration with a name that already exists!");

				//Set up layout object
				StructuredTypeLayout type = {};
				type.fields.resize(header.fieldCount);
				std::set<std::string> fieldNames;

				//Start parsing fields
				for(StructuredTypeLayout::Field& field : type.fields) {
					//Get type tag
					uint8_t tagByte = reader.ReadInteger<uint8_t>();
					if(!ValidateTypeTag(tagByte)) DECODE_ERROR("Invalid TypeTag in structured object type declaration!");
					field.type = (TypeTag)tagByte;
					if(field.type == TypeTag::StructuredObjTypeDecl || field.type == TypeTag::ScopeBoundary) DECODE_ERROR("Type declarations may not contain scope boundaries or other declarations!");

					//Field name
					uint8_t nameLen = reader.ReadInteger<uint8_t>();
					if(nameLen == 0) DECODE_ERROR("Cannot declare a field with no name!");
					field.name = reader.ReadString(nameLen);
					if(fieldNames.contains(field.name)) DECODE_ERROR("Encountered duplicate field name in type declaration!");
					fieldNames.insert(field.name);

					//Type-specific functionality
					if(field.type == TypeTag::List) {
						//Get element type tag
						uint8_t elementTagByte = reader.ReadInteger<uint8_t>();
						if(!ValidateTypeTag(elementTagByte)) DECODE_ERROR("Invalid list element TypeTag in structured object type declaration!");
						field.elementType = (TypeTag)elementTagByte;
						if(field.elementType == TypeTag::List || field.elementType == TypeTag::StructuredObjTypeDecl || field.elementType == TypeTag::ScopeBoundary) DECODE_ERROR("Lists of scope boundaries, type declarations, or other lists are not allowed in type declarations!");
					}
					if(TYPE_OR_LIST_OF(field, StructuredObj)) {
						//Type ID
						uint8_t typeLen = reader.ReadInteger<uint8_t>();
						field.typeID = reader.ReadString(typeLen);
						if(!index->types.contains(field.typeID)) DECODE_ERROR("Structured object in type declaration uses a type that does not exist!");
					} else if(TYPE_OR_LIST_OF(field, Vector) || TYPE_OR_LIST_OF(field, Matrix)) {
						//Validation of element type
						uint8_t elementTagByte = reader.ReadInteger<uint8_t>();
						if(!ValidateTypeTag(elementTagByte)) DECODE_ERROR("Invalid math element TypeTag in structured object type declaration!");
						if(elementTagByte < 0x0E || elementTagByte > 0x2D) DECODE_ERROR("In type declaration: encountered vector/matrix with invalid element type!");
						if(field.type == TypeTag::List) {
							field.nestedElementType = (TypeTag)elementTagByte;
						} else {
							field.elementType = (TypeTag)elementTagByte;
						}

						//Check dimensions
						field.width = reader.ReadInteger<uint8_t>();
						if(field.width < 2 || field.width > 4) DECODE_ERROR("In type declaration: encountered vector/matrix with invalid width!");
						if(field.elementType == TypeTag::Matrix) {
							field.height = reader.ReadInteger<uint8_t>();
							if(field.height < 2 || field.height > 4) DECODE_ERROR("In type declaration: encountered matrix with invalid height!");
						}
					}
				}

				//Ensure we hit the scope boundary
				uint8_t tagByte = reader.ReadInteger<uint8_t>();
				if(!(ValidateTypeTag(tagByte) && (TypeTag)tagByte == TypeTag::ScopeBoundary)) DECODE_ERROR("Expected a scope boundary at the end of type declaration!");

				//Add type to registry
				index->types.insert_or_assign(header.name, std::move(type));
			} else {
				//Check that we're not nesting too deep
				if(++nest > 64) DECODE_ERROR("Nesting too deep!");

				//Prepare entry object
				ScopeEntry entry = {};
				entry.list = (header.type == TypeTag::List);
				entry.name = header.name;
				entry.streamBeginPosition = reader->tellg();
				entry.typeID = header.typeID;
				std::string newScopePath;
				if(!scopePath.empty()) {
					if(scopePath.ends_with("["))
						newScopePath = scopePath + entry.name + "]";
					else
						newScopePath = scopePath + "." + entry.name;
				} else {
					newScopePath = entry.name;
				}
				entry.id = GenIndexID(newScopePath);
				if(entry.list) newScopePath += "[";
				if(nest > 1 && header.type == TypeTag::StructuredObjTypeDecl) DECODE_ERROR("Type declarations may only appear in the root scope!");

				//Handle different scope types
				if(entry.list) {
					//Element type & validation
					entry.listElementType = header.elementType;
					if(entry.listElementType == TypeTag::List) DECODE_ERROR("Lists may not directly contain other lists!");
					if(entry.listElementType == TypeTag::StructuredObjTypeDecl) DECODE_ERROR("Lists may not contain type declarations!");
					if(entry.listElementType == TypeTag::ScopeBoundary) DECODE_ERROR("Lists of scope boundaries cannot exist!");

					//Validate element type parameters
					if(entry.listElementType == TypeTag::StructuredObj && !index->types.contains(entry.typeID))
						DECODE_ERROR("List of structured objects uses a type that has not yet been defined!")
					else if(entry.listElementType == TypeTag::Vector || entry.listElementType == TypeTag::Matrix) {
						entry.listMathData = {.width = header.width, .height = header.height, .type = header.nestedElementType};
						if(uint8_t asByte = static_cast<uint8_t>(entry.listMathData.type); asByte < 0x0E || asByte > 0x2D) DECODE_ERROR("Encountered list of vectors/matrices with invalid element type!");
						if(entry.listMathData.width < 2 || entry.listMathData.width > 4) DECODE_ERROR("Encountered list of vectors/matrices with invalid width!");
						if(entry.listElementType == TypeTag::Matrix && (entry.listMathData.height < 2 || entry.listMathData.height > 4)) DECODE_ERROR("Encountered list of matrices with invalid height!");
					}

					//Start parsing values
					for(unsigned int i = 0; i < header.size; ++i) {
						//Construct a fake value header for reading
						ValueHeader fakeHeader = {};
						fakeHeader.name = std::to_string(i);
						fakeHeader.type = entry.listElementType;
						if(static_cast<uint8_t>(entry.listElementType) <= 0xC) {
							//Buffer object (string/byte buffer)
							fakeHeader.size = reader.ReadInteger<uint32_t>();
							if(entry.listElementType == TypeTag::String && fakeHeader.size > std::pow(2, 24)) DECODE_ERROR("Encountered a string that is too long (> 24-bit integer limit!)");
						} else if(entry.listElementType == TypeTag::Vector || entry.listElementType == TypeTag::Matrix) {
							fakeHeader.width = entry.listMathData.width;
							fakeHeader.nestedElementType = entry.listMathData.type;
							if(entry.listElementType == TypeTag::Matrix) fakeHeader.height = entry.listMathData.height;
						} else if(entry.listElementType == TypeTag::StructuredObj) {
							fakeHeader.typeID = entry.typeID;
						} else if(entry.listElementType == TypeTag::UnstructuredObj) {
							fakeHeader.fieldCount = reader.ReadInteger<uint16_t>();
						}

						//Parse the value
						if(IsValue(entry.listElementType)) {
							ValueEntry parsed = _ParseValueInternal(fakeHeader, newScopePath);
							entry.subvalues.push_back(std::move(parsed));
						} else {
							//Setup scope entry
							ScopeEntry listScope = {};
							listScope.list = false;
							listScope.name = std::to_string(i);
							listScope.streamBeginPosition = reader->tellg();
							listScope.typeID = entry.typeID;
							std::string subscopePath;
							if(newScopePath.ends_with("["))
								subscopePath = newScopePath + listScope.name + "]";
							else
								subscopePath = newScopePath + "." + listScope.name;
							listScope.id = GenIndexID(subscopePath);

							//Setup expectations
							ScopeExpectations se = {};
							se.type = entry.listElementType;
							se.typeID = entry.typeID;
							se.fieldCount = (entry.listElementType == TypeTag::StructuredObj ? index->types[entry.typeID].fields.size() : fakeHeader.fieldCount);
							se.rootFlag = false;

							//Parse scope
							_ParseScopeInternal(listScope, se, newScopePath);

							//Add to list
							entry.subscopes.push_back(std::move(listScope));
						}
					}

					//Ensure we hit the scope boundary
					uint8_t tagByte = reader.ReadInteger<uint8_t>();
					if(!(ValidateTypeTag(tagByte) && (TypeTag)tagByte == TypeTag::ScopeBoundary)) DECODE_ERROR("Expected a scope boundary at the end of list!");
				} else {
					//If this is structured then the type must be declared
					if(header.type == TypeTag::StructuredObj && !index->types.contains(entry.typeID)) DECODE_ERROR("Structured object uses a type that has not yet been defined!");

					//Prepare expectations
					ScopeExpectations se = {.type = header.type,
						.fieldCount = (header.type == TypeTag::StructuredObj ? index->types[entry.typeID].fields.size() : header.fieldCount),
						.typeID = entry.typeID,
						.rootFlag = false};

					//Parse scope
					_ParseScopeInternal(entry, se, newScopePath);
				}

				//Roll back nest counter and add scope to list
				--nest;
				scope.subscopes.push_back(std::move(entry));
			}
		}
	}

	void Decoder::Parse() {
		if(!readerValid) throw std::runtime_error("Decoder has no valid reader!");
		if(index.has_value()) throw std::runtime_error("Stream has already been parsed!");

		//Configure root node
		index = std::make_optional<Index>();
		index->root.name = "";
		index->root.id = GenIndexID("");
		index->root.streamBeginPosition = 0;
		index->root.typeID = "";
		nest = 0;

		//Start decoding the root scope
		try {
			_ParseScopeInternal(index->root, ScopeExpectations {.type = TypeTag::UnstructuredObj, .fieldCount = SIZE_MAX, .typeID = "", .rootFlag = true}, "");
		} catch(...) {
			//Intercept exception to set fail flag and then rethrow
			failFlag = true;
			std::rethrow_exception(std::current_exception());
		}
	}
}