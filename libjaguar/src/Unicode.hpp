#pragma once

#include <cstdint>
#include <string>

namespace libjaguar {
	inline bool CheckUTF8(const std::string& string) {
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
}