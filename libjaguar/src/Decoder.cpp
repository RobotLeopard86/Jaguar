#include "libjaguar/Decoder.hpp"
#include "Utilities.hpp"
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

	void Decoder::_ParseScopeInternal(ScopeEntry& scope, unsigned int expectedFieldCount) {
		//Continuously read the next header
		while(true) {
			//Get next header
			ValueHeader header = reader.ReadHeader();
			std::size_t encounteredFields = scope.subscopes.size() + scope.subvalues.size();

			//If we see a scope boundary, check position
			if(header.type == TypeTag::ScopeBoundary) {
				//Is this root (expected field count is UINT16_MAX + 1, since that's above the allowed number of object fields)
				if(expectedFieldCount > UINT16_MAX) throw std::runtime_error("Unexpected scope boundary in root scope!");

				//Have we seen the expected number of values yet?
				//Return if so because the scope is done
				if(encounteredFields == expectedFieldCount) return;

				//If we're less, this is simply a case of early scope termination
				//We still do an if-check to throw the appropriate exception in case we passed the expected field count without a boundary
				else if(encounteredFields < expectedFieldCount)
					throw std::runtime_error("Early scope boundary detected!");
				else
					//This really shouldn't happen because we try to anticipate excess fields early
					throw std::runtime_error("Late scope boundary detected!");
			}

			//Check expected field count to make sure we're not over
			if(encounteredFields > expectedFieldCount) throw std::runtime_error("Excess number of fields detected in scope!");
		}
	}

	void Decoder::Parse() {
		if(!readerValid) throw std::runtime_error("Decoder has no valid reader!");
		if(index.has_value()) throw std::runtime_error("Stream has already been parsed!");

		//Configure root node
		index->root.name = "";
		index->root.id = GenIndexID("");
		index->root.streamBeginPosition = 0;
		index->root.typeID = "";

		//Start decoding the root scope
		try {
			_ParseScopeInternal(index->root, UINT16_MAX + 1);
		} catch(...) {
			//Intercept exception to set fail flag and then rethrow
			failFlag = true;
			std::rethrow_exception(std::current_exception());
		}
	}
}