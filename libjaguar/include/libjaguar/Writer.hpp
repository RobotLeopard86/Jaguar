#pragma once

#include "DllHelper.hpp"
#include "Value.hpp"
#include "Traits.hpp"

#include <bit>
#include <ostream>
#include <cstdint>
#include <type_traits>
#include <span>

namespace libjaguar {
	/**
	 * @brief Low-level stateless Jaguar stream writer
	 *
	 * The sole purpose of this class is to write values to the stream, not to keep track of the data context, so misuse will result in an improperly formatted stream
	 *
	 * This class is move-only!
	 */
	class LJAPI Writer {
	  public:
		/**
		 * @brief Create a writer, providing it exclusive ownership of the stream to write to
		 *
		 * @param stream The stream into which to write Jaguar data
		 */
		Writer(std::ostream&& stream);

		///@cond
		Writer(const Writer&) = delete;
		Writer& operator=(const Writer&) = delete;
		Writer(Writer&&);
		Writer& operator=(Writer&&);
		///@endcond

		/**
		 * @brief Access the underlying stream to perform operations outside of the writer
		 *
		 * This is to allow for applications to still control the stream, while ensuring that only one Writer can use it at a time
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::ostream* operator->();

		/**
		 * @brief Access the underlying stream to perform operations outside of the writer
		 *
		 * This is to allow for applications to still control the stream, while ensuring that only one Writer can use it at a time
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::ostream* operator*();

		/**
		 * @brief Write a value header to the stream
		 *
		 * @param header The header to write
		 * @param noIdentifier Whether or not to omit the value identifier (not used in lists, for example)
		 */
		void WriteHeader(const ValueHeader& header, bool noIdentifier = false);

		/**
		 * @brief Write an integer value to the stream
		 *
		 * @tparam T The integer type - signed or unsigned from 8 to 64 bits
		 *
		 * @param value The integer to write
		 */
		template<integer T>
		void WriteInteger(T value) {
			_WriteIntegerInternal(uint64_t(value), bits_v<T>);
		}

		/**
		 * @brief Write a floating-point value to the stream
		 *
		 * @tparam T The type - float or double
		 *
		 * @param value The floating-point number to write
		 */
		template<std::floating_point T>
			requires std::is_same_v<T, float> || std::is_same_v<T, double>
		void WriteFloat(T value) {
			if constexpr(std::is_same_v<T, float>) {
				uint32_t asBits = std::bit_cast<uint32_t, float>(value);
				_WriteIntegerInternal(uint64_t(asBits), 32);
			} else {
				uint64_t asBits = std::bit_cast<uint64_t, double>(value);
				_WriteIntegerInternal(asBits, 64);
			}
		}

		/**
		 * @brief Write a boolean value to the stream
		 *
		 * @param value The boolean to write
		 */
		void WriteBool(bool value);

		/**
		 * @brief Write a string to the stream
		 *
		 * @param value The string to write
		 */
		void WriteString(const std::string& value);

		/**
		 * @brief Write a buffer to the stream
		 *
		 * @param value The buffer to write
		 */
		void WriteBuffer(const std::span<std::byte>& value);

		/**
		 * @brief Write a buffer to the stream from another stream
		 *
		 * @param istream The stream to source data from (must not be @c nullptr)
		 * @param length The length (in bytes) of data to copy
		 */
		void WriteBufferFromStream(std::istream* istream, std::size_t length);

	  private:
		std::ostream stream;
		bool moved = false;

		void _WriteIntegerInternal(uint64_t value, uint8_t bits);
	};
}