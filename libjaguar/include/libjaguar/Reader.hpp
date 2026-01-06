#pragma once

#include "DllHelper.hpp"
#include "Value.hpp"
#include "Traits.hpp"

#include <istream>
#include <memory>

namespace libjaguar {
	/**
	 * @brief Low-level stateless Jaguar stream parser
	 *
	 * The sole purpose of this class is to read the stream and extract value data. It does @b not persist data between calls and is thus not compliant with the specification on its own. This class puts data directly from the stream into
	 * returned structures; it is the consumer's responsibility to validate this data.
	 *
	 * This class is move-only!
	 */
	class LJAPI Reader {
	  public:
		/**
		 * @brief Create a reader, providing it exclusive ownership of the stream to read from
		 *
		 * @param istream The stream containing Jaguar data
		 */
		Reader(std::unique_ptr<std::istream>&& istream);

		///@cond
		Reader(const Reader&) = delete;
		Reader& operator=(const Reader&) = delete;
		Reader(Reader&&);
		Reader& operator=(Reader&&);
		///@endcond

		/**
		 * @brief Access the underlying stream to perform operations outside of the parser
		 *
		 * This is to allow for applications to still control the stream, while ensuring that only one Reader can use it at a time
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::istream* operator->();

		/**
		 * @brief Access the underlying stream to perform operations outside of the parser
		 *
		 * This is to allow for applications to still control the stream, while ensuring that only one Reader can use it at a time
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::istream* operator*();

		/**
		 * @brief Read a value header from the stream
		 *
		 * @return The read ValueHeader
		 */
		ValueHeader ReadHeader();

		/**
		 * @brief Read an integer value from the stream
		 *
		 * @tparam T The integer type - signed or unsigned from 8 to 64 bits
		 *
		 * @return The read integer
		 */
		template<integer T>
		T ReadInteger() {
			return static_cast<T>(_ReadIntegerInternal(bits_v<T>));
		}

		/**
		 * @brief Read a floating-point value from the stream
		 *
		 * @tparam T The type - float or double
		 *
		 * @return The read floating-point value
		 */
		template<std::floating_point T>
			requires std::is_same_v<T, float> || std::is_same_v<T, double>
		T ReadFloat() {
			if constexpr(std::is_same_v<T, float>) {
				return std::bit_cast<float, uint32_t>(static_cast<uint32_t>(_ReadIntegerInternal(32)));
			} else {
				return std::bit_cast<double, uint64_t>(_ReadIntegerInternal(64));
			}
		}

		/**
		 * @brief Read a boolean value from the stream
		 *
		 * @return The read boolean
		 */
		bool ReadBool();

		/**
		 * @brief Read a string from the stream
		 *
		 * @param length The length of the string to read
		 *
		 * @return The read string
		 */
		std::string ReadString(uint32_t length);

		//TODO: ReadBuffer --- not sure what the API shape should look like yet

	  private:
		std::unique_ptr<std::istream> stream;

		uint64_t _ReadIntegerInternal(uint8_t bits);
	};
}