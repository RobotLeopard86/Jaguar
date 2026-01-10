#pragma once

#include "DllHelper.hpp"
#include "Value.hpp"
#include "Traits.hpp"

#include <bit>
#include <ostream>
#include <cstdint>
#include <ranges>
#include <type_traits>
#include <span>
#include <memory>

namespace libjaguar {
	/**
	 * @brief Low-level stateless Jaguar stream writer
	 *
	 * The sole purpose of this class is to write values to the stream, not to keep track of the data context, so misuse will result in an improperly formatted stream. Errors will only be thrown when they present a
	 * technical limitation (e.g. invalid UTF-8). Structural issues are ignored.
	 *
	 * <b>This class is move-only!</b>
	 */
	class LJAPI Writer {
	  public:
		/**
		 * @brief Create a writer, providing it exclusive ownership of the stream to write to
		 *
		 * @param ostream The stream into which to write Jaguar data
		 */
		Writer(std::unique_ptr<std::ostream>&& ostream);

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
		 *
		 * @throws std::runtime_error If the provided name string is invalid UTF-8 or has the wrong length
		 * @throws std::runtime_error If the provided type ID string is invalid UTF-8 or has the wrong length (for types requiring that)
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
		 *
		 * @throws std::runtime_error If the string is not valid UTF-8
		 * @throws std::runtime_error If the string is longer than the 24-bit integer limit for string legnths
		 */
		void WriteString(const std::string& value);

		/**
		 * @brief Write a buffer to the stream
		 *
		 * @tparam R The container type to source the buffer from
		 *
		 * @param value The buffer to write
		 */
		template<byte_range R>
		void WriteBuffer(const R& value) {
			std::span<std::byte> span(value.begin(), value.end());
			_WriteBufferInternal(span);
		}

		/**
		 * @brief Write a buffer to the stream from another stream
		 *
		 * @param istream The stream to source data from (must not be @c nullptr)
		 * @param length The length (in bytes) of data to copy
		 *
		 * @throws std::runtime_error If the source stream is null or invalid
		 * @throws std::runtime_error If reading from the source stream fails
		 */
		void WriteBufferFromStream(std::istream* istream, std::size_t length);

	  private:
		std::unique_ptr<std::ostream> stream;

		void _WriteIntegerInternal(uint64_t value, uint8_t bits);
		void _WriteBufferInternal(std::span<std::byte>& value);
	};
}