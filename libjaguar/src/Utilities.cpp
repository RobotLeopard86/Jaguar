#include "Utilities.hpp"
#include "libjaguar/Index.hpp"

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
		return true;
	}
}