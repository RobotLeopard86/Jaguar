#include "libjaguar/Decoder.hpp"
#include "Utilities.hpp"
#include "libjaguar/Index.hpp"
#include "libjaguar/StructuredTypeLayout.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/ValueHeader.hpp"

#include <exception>
#include <stdexcept>
#include <cmath>

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
			if(uint8_t asByte = static_cast<uint8_t>(header.elementType); asByte < 0x0E || asByte > 0x2D) throw std::runtime_error("Encountered vector/matrix with invalid element type!");
			entry.elementType = header.elementType;

			if(header.width < 2 || header.width > 4) throw std::runtime_error("Encountered vector/matrix with invalid width!");
			entry.width = header.width;
			if(header.type == TypeTag::Matrix) {
				if(header.height < 2 || header.height > 4) throw std::runtime_error("Encountered matrix with invalid height!");
				entry.height = header.height;
			} else {
				entry.height = 1;
			}
		}

		//Buffer objects and size checks
		if(static_cast<uint8_t>(header.type) <= 0xC) entry.size = header.size;
		if(header.type == TypeTag::String && header.size >= std::pow(2, 24)) throw std::runtime_error("Encountered a string that is too long (> 24-bit integer limit!)");

		//Calculate size of value body to skip
		std::size_t skipAmountBytes = 0;
		switch(header.type) {
			case TypeTag::String:
			case TypeTag::ByteBuffer:
			case TypeTag::Substream:
				skipAmountBytes = header.size;
				break;
			case TypeTag::SInt8:
			case TypeTag::UInt8:
			case TypeTag::Boolean:
				skipAmountBytes = 1;
				break;
			case TypeTag::SInt32:
			case TypeTag::UInt32:
			case TypeTag::Float32:
				skipAmountBytes = 4;
				break;
			case TypeTag::SInt64:
			case TypeTag::UInt64:
			case TypeTag::Float64:
				skipAmountBytes = 8;
				break;
			case TypeTag::SInt16:
			case TypeTag::UInt16:
				skipAmountBytes = 2;
				break;
			case TypeTag::ScopeBoundary:
				skipAmountBytes = 0;
				break;
			case TypeTag::Vector:
			case TypeTag::Matrix: {
				uint8_t asByte = static_cast<uint8_t>(header.elementType);
				if(asByte >= 0xE) asByte -= 2;
				skipAmountBytes = ((asByte & 0xF) - 0x9) * entry.width * entry.height;
				break;
			}
			default: break;
		}
		reader->ignore(skipAmountBytes);

		return entry;
	}

	void Decoder::_ParseScopeInternal(ScopeEntry& scope, ScopeExpectations expectations, const std::string& scopePath) {
		//Continuously read the next header
		while(true) {
			//Get next header
			ValueHeader header = {};
			try {
				header = reader.ReadHeader();
			} catch(...) {
				if(reader->eof()) break;
				std::rethrow_exception(std::current_exception());
			}
			std::size_t encounteredFields = scope.subscopes.size() + scope.subvalues.size();

			//If we see a scope boundary, check position
			if(header.type == TypeTag::ScopeBoundary) {
				//Is this root
				if(expectations.rootFlag) throw std::runtime_error("Unexpected scope boundary in root scope!");

				//Have we seen the expected number of values yet?
				//Return if so because the scope is done
				if(encounteredFields == expectations.fieldCount) return;

				//If we're less, this is simply a case of early scope termination
				//We still do an if-check to throw the appropriate exception in case we passed the expected field count without a boundary
				else if(encounteredFields < expectations.fieldCount)
					throw std::runtime_error("Early scope boundary detected!");
				else
					//This really shouldn't happen because we try to anticipate excess fields early
					throw std::runtime_error("Late scope boundary detected!");
			}

			//Check expected field count to make sure we're not over
			if(encounteredFields > expectations.fieldCount) throw std::runtime_error("Excess number of fields detected in scope!");

			//Handle what we found
			if(IsValue(header.type)) {
				//Parse the value
				ValueEntry entry = _ParseValueInternal(header, scopePath);

				//Add entry
				scope.subvalues.push_back(std::move(entry));
			} else {
				//Check that we're not nesting too deep
				if(++nest > 64) throw std::runtime_error("Nesting too deep!");

				//Prepare entry object
				ScopeEntry entry = {};
				entry.list = (header.type == TypeTag::List);
				entry.name = header.name;
				entry.streamBeginPosition = reader->tellg();
				entry.typeID = header.typeID;
				std::string newScopePath;
				if(!scopePath.empty()) {
					if(scopePath.ends_with("["))
						newScopePath = scopePath + header.name + "]";
					else
						newScopePath = scopePath + "." + header.name;
				} else {
					newScopePath = entry.name;
				}
				entry.id = GenIndexID(newScopePath);
				if(nest > 1 && header.type == TypeTag::StructuredObjTypeDecl) throw std::runtime_error("Type declarations may only appear in the root scope!");

				//Handle different scope types
				if(entry.list) {
					//Element type & validation
					entry.listElementType = header.elementType;
					if(entry.listElementType == TypeTag::List) throw std::runtime_error("Lists may not directly contain other lists!");
					if(entry.listElementType == TypeTag::StructuredObjTypeDecl) throw std::runtime_error("Lists may not contain type declarations!");
					if(entry.listElementType == TypeTag::ScopeBoundary) throw std::runtime_error("Lists of scope boundaries cannot exist!");

					//Validate element type parameters
					if(entry.listElementType == TypeTag::StructuredObj && !index->types.contains(entry.typeID))
						throw std::runtime_error("List of structured objects uses a type that has not yet been defined!");
					else if(entry.listElementType == TypeTag::Vector || entry.listElementType == TypeTag::Matrix) {
						entry.listMathData = {.width = header.width, .height = header.height, .type = header.nestedElementType};
						if(uint8_t asByte = static_cast<uint8_t>(entry.listMathData.type); asByte < 0x0E || asByte > 0x2D) throw std::runtime_error("Encountered list of vectors/matrices with invalid element type!");
						if(entry.listMathData.width < 2 || entry.listMathData.width > 4) throw std::runtime_error("Encountered list of vectors/matrices with invalid width!");
						if(entry.listElementType == TypeTag::Matrix && (entry.listMathData.height < 2 || entry.listMathData.height > 4)) throw std::runtime_error("Encountered list of matrices with invalid height!");
					}

					//Start parsing values
					for(unsigned int i = 0; i < header.size; ++i) {
						//Construct a fake value header for reading
						ValueHeader fakeHeader = {};
						fakeHeader.name = std::to_string(i);
						fakeHeader.type = entry.listElementType;
						if(static_cast<uint8_t>(entry.listElementType) <= 0xC) {
							//Buffer object (string, byte buffer, substream)
							fakeHeader.size = reader.ReadInteger<uint32_t>();
							if(entry.listElementType == TypeTag::String && fakeHeader.size > std::pow(2, 24)) throw std::runtime_error("Encountered a string that is too long (> 24-bit integer limit!)");
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
					if(!(ValidateTypeTag(tagByte) && (TypeTag)tagByte != TypeTag::ScopeBoundary)) throw std::runtime_error("Expected a scope boundary at the end of list!");
				} else if(header.type == TypeTag::StructuredObjTypeDecl) {
				} else {
					//If this is structured then the type must be declared
					if(header.type == TypeTag::StructuredObj && !index->types.contains(entry.typeID)) throw std::runtime_error("Structured object uses a type that has not yet been defined!");

					//Prepare expectations
					ScopeExpectations se = {.type = header.type,
						.fieldCount = (header.type == TypeTag::StructuredObj ? index->types[entry.typeID].fields.size() : header.fieldCount),
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