#include "Utilities.hpp"
#include "libjaguar/Index.hpp"
#include "libjaguar/StructuredTypeLayout.hpp"
#include "libjaguar/TypeTags.hpp"

#include <set>
#include <string>
#include <vector>

namespace libjaguar {
	bool CheckUTF8(const std::string& string) {
		//Keep track of expected continuation bytes (to prevent overlong encodings)
		uint8_t expectedContinuations = 0;

		//Check characters
		for(char character : string) {
			//Convert to byte
			uint8_t byte = static_cast<uint8_t>(character);

			//Check if this is leading byte/ASCII or a continuation byte?
			if(expectedContinuations == 0) {
				//ASCII: 0xxx'xxxx
				if((byte & 0b1000'0000) == 0b0000'0000) {
					continue;
				}

				//Start of 2-byte sequence: 110x'xxxx
				if((byte & 0b1110'0000) == 0b1100'0000) {
					expectedContinuations = 1;
					continue;
				}

				//Start of 3-byte sequence: 1110'xxxx
				if((byte & 0b1111'0000) == 0b1110'0000) {
					expectedContinuations = 2;
					continue;
				}

				//Start of 4-byte sequence: 1111'0xxx
				if((byte & 0b1111'1000) == 0b1111'0000) {
					expectedContinuations = 3;
					continue;
				}

				//Invalid leading byte
				return false;
			} else {
				//Continuation bytes must be 10xx'xxxx
				if((byte & 0b1100'0000) == 0b1000'0000) {
					--expectedContinuations;
					continue;
				} else {
					return false;
				}
			}
		}

		//All characters passed, string is valid as long as we don't have outstanding continuation bytes
		return expectedContinuations == 0;
	}

	uint64_t GenIndexID(std::string path) {
		//Initial seed
		uint64_t hash = 0xEE674237ull;

		//Split path components
		std::vector<std::string> components;
		components.push_back("");
		std::size_t idx = 0;
		for(char c : path) {
			if(c == '.') {
				++idx;
				components.push_back("");
				continue;
			}
			if(c == '[') {
				++idx;
				components.push_back("$$arr");
				continue;
			}
			if(c == ']') {
				continue;
			}
			components[idx] += c;
		}

		//Run hashing
		for(const std::string& component : components) {
			//Convert string to bytes
			uint64_t hc = 0;
			for(char c : component) hc = (hc * 257 + static_cast<unsigned char>(c));

			//Multiply hash component by 37 because why not
			hc *= 37;

			//Fold new component into hash
			hash *= (hc + 2);

			//Swap the upper and lower nibbles of all bytes
			{
				uint64_t nibbleSwapped = 0;
				for(uint8_t i = 0; i < 8; ++i) {
					//Get the byte out
					uint8_t byte = (hash >> (i * 8)) & 0xFF;

					//Swap it
					uint8_t swappedByte = ((byte & 0x0F) << 4) | ((byte & 0xF0) >> 4);

					//Put the swapped byte back in
					nibbleSwapped |= (static_cast<uint64_t>(swappedByte) << (i * 8));
				}
				hash = nibbleSwapped;
			}

			//Rotate hash left by one byte
			hash = (hash << 8) | (hash >> 56);
		}

		//Return end product
		return hash;
	}

	//TODO
	bool ValidateTypeLayout(const StructuredTypeLayout& layout) {
		//Tracking info
		std::set<std::string> seenFields;

		//Check all the fields; if everything passes than they all must be good
		for(const StructuredTypeLayout::Field& field : layout.fields) {
			//Field name
			if(seenFields.contains(field.name)) return false;
			if(!CheckUTF8(field.name)) return false;
			if(field.name.size() < 1 || field.name.size() > UINT8_MAX) return false;
			seenFields.insert(field.name);

			//Ensure this is an actual type
			if(field.type == TypeTag::StructuredObjTypeDecl || field.type == TypeTag::ScopeBoundary) return false;

			//No more checks needed if this is not a list, structured object or math type
			if(static_cast<uint8_t>(field.type) < 0x3A || field.type == TypeTag::UnstructuredObj) continue;

			//Type-specific checks
			switch(field.type) {
				case TypeTag::List:
					if(field.elementType == TypeTag::List || field.elementType == TypeTag::StructuredObjTypeDecl || field.elementType == TypeTag::ScopeBoundary) return false;
					if(field.elementType == TypeTag::StructuredObj && (field.typeID.empty() || !CheckUTF8(field.typeID) || field.typeID.size() > UINT8_MAX)) return false;
					if(field.elementType == TypeTag::Vector || field.elementType == TypeTag::Matrix) {
						if(field.width < 2 || field.width > 4) return false;
						if(uint8_t elementTagByte = static_cast<uint8_t>(field.nestedElementType); elementTagByte < 0x0E || elementTagByte > 0x2D) return false;
						if(field.elementType == TypeTag::Matrix && (field.height < 2 || field.height > 4)) return false;
					}
					break;
				case TypeTag::StructuredObj:
					if(field.typeID.empty()) return false;
					if(field.typeID.size() > UINT8_MAX) return false;
					if(!CheckUTF8(field.typeID)) return false;
					break;
				case TypeTag::Matrix:
					if(field.height < 2 || field.height > 4) return false;
				case TypeTag::Vector:
					if(field.width < 2 || field.width > 4) return false;
					if(uint8_t elementTagByte = static_cast<uint8_t>(field.elementType); elementTagByte < 0x0E || elementTagByte > 0x2D) return false;
					break;
				default: break;
			}
		}
		return true;
	}
}