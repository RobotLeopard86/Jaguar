#include "libjaguar/Decoder.hpp"
#include "Utilities.hpp"
#include "libjaguar/Index.hpp"
#include "libjaguar/StructuredTypeLayout.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/ValueHeader.hpp"

#include <exception>
#include <stdexcept>

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
		entry.id = GenIndexID(scopePath + (scopePath.empty() ? "" : ".") + entry.name);

		//Vector/matrix handling
		if(header.type == TypeTag::Vector || header.type == TypeTag::Matrix) {
			if(uint8_t asByte = static_cast<uint8_t>(header.elementType); asByte < 0x0E || asByte > 0x2D) throw std::runtime_error("Encountered vector/matrix with invalid element type!");
			entry.elementType = header.elementType;

			if(header.width < 2 || header.width > 4) throw std::runtime_error("Encountered vector/matrix with invalid width!");
			entry.width = header.width;
			if(header.type == TypeTag::Matrix) {
				if(header.height < 2 || header.height > 4) throw std::runtime_error("Encountered matrix with invalid height!");
				entry.height = header.height;
			}
		}

		//Buffer objects and size checks
		if(static_cast<uint8_t>(header.type) <= 0xC) entry.size = header.size;
		if(header.type == TypeTag::String && header.size >= std::pow(2, 24)) throw std::runtime_error("Encountered a string that is too long (> 24-bit integer limit!)");

		return entry;
	}

	void Decoder::_ParseScopeInternal(ScopeEntry& scope, ScopeExpectations expectations, const std::string& scopePath) {
		//Continuously read the next header
		while(true) {
			//Get next header
			ValueHeader header = reader.ReadHeader();
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
				std::string newScopePath = scopePath + (scopePath.empty() ? "" : ".") + entry.name;
				entry.id = GenIndexID(newScopePath);
				if(nest > 1 && header.type == TypeTag::StructuredObjTypeDecl) throw std::runtime_error("Type declarations may only appear in the root scope!");

				//Handle different scope types
				if(entry.list) {
					//Element type
					entry.listElementType = header.elementType;
					if(entry.listElementType == TypeTag::List) throw std::runtime_error("Lists may not directly contain other lists!");
					if(entry.listElementType == TypeTag::StructuredObjTypeDecl) throw std::runtime_error("Lists may not contain type declarations!");

					//Validate element type parameters
					if(entry.listElementType == TypeTag::StructuredObj && !index->types.contains(entry.typeID))
						throw std::runtime_error("List of structured objects uses a type that has not yet been defined!");
					else if(entry.listElementType == TypeTag::Vector || entry.listElementType == TypeTag::Matrix) {
						entry.listMathData = {.type = header.nestedElementType, .width = header.width, .height = header.height};
						if(uint8_t asByte = static_cast<uint8_t>(entry.listMathData.type); asByte < 0x0E || asByte > 0x2D) throw std::runtime_error("Encountered list of vectors/matrices with invalid element type!");
						if(entry.listMathData.width < 2 || entry.listMathData.width > 4) throw std::runtime_error("Encountered list of vectors/matrices with invalid width!");
						if(entry.listElementType == TypeTag::Matrix && (entry.listMathData.height < 2 || entry.listMathData.height > 4)) throw std::runtime_error("Encountered list of matrices with invalid height!");
					}
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
		index.emplace();
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