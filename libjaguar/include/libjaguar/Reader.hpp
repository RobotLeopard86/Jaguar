#pragma once

#include "DllHelper.hpp"
#include "ValueHeader.hpp"
#include "Traits.hpp"
#include "ScopedView.hpp"

#include <bit>
#include <istream>
#include <cstdint>
#include <type_traits>
#include <memory>

namespace libjaguar {
	/**
	 * @brief Low-level stateless Jaguar stream reader
	 *
	 * The sole purpose of this class is to read the stream and extract value data. It does @b not persist data between calls and is thus not compliant with the specification on its own. This class puts data directly from the stream into
	 * returned structures; it is the consumer's responsibility to validate this data. Errors will only be thrown when they present a technical limitation (e.g. invalid UTF-8).
	 *
	 * <b>This class is move-only!</b>
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
		 * This is to allow for applications to still control the stream, while ensuring that ownership stays with the Reader
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::istream* operator->();

		/**
		 * @brief Access the underlying stream to perform operations outside of the parser
		 *
		 * This is to allow for applications to still control the stream, while ensuring that ownership stays with the Reader
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::istream* operator*();

		/**
		 * @brief Read a value header from the stream
		 *
		 * @return The read ValueHeader
		 *
		 * @throws std::runtime_error If the TypeTag found is invalid
		 * @throws std::runtime_error If the value name string is empty or not valid UTF-8
		 * @throws std::runtime_error If a element TypeTag is invalid (e.g. for a list)
		 * @throws std::runtime_error If an IO error occurs while reading
		 */
		ValueHeader ReadHeader();

		/**
		 * @brief Read an integer value from the stream
		 *
		 * @tparam T The integer type - signed or unsigned from 8 to 64 bits
		 *
		 * @return The read integer
		 *
		 * @throws std::runtime_error If an IO error occurs while reading
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
		 *
		 * @throws std::runtime_error If an IO error occurs while reading
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
		 *
		 * @throws std::runtime_error If the read value is not a possible boolean
		 * @throws std::runtime_error If an IO error occurs while reading
		 */
		bool ReadBool();

		/**
		 * @brief Read a string from the stream
		 *
		 * @param length The length of the string to read
		 *
		 * @return The read string
		 *
		 * @throws std::runtime_error If the read string is invalid UTF-8
		 * @throws std::runtime_error If the requested length is larger than the 24-bit integer limit for allowed string lengths
		 * @throws std::runtime_error If an IO error occurs while reading
		 */
		std::string ReadString(uint32_t length);

		/**
		 * @brief Access a region of bytes from the stream
		 *
		 * @param length The size in bytes of the region to access
		 *
		 * @return A non-owning pointer to a scoped view to read from that region, replacing the previous view if one exists
		 *
		 * @warning While a view is active, the rest of the Reader's functionality will be disabled. Use the @c DiscardAll function
		 * to deactivate the view and continue reading.
		 *
		 * @warning The lifetime of the scoped view is controlled by the Reader. This handle will remain valid until one of the following occurs:
		 * 1. @c ReadBuffer is called again, invalidating the view this handle references
		 * 2. The Reader is destroyed
		 * 3. The Reader is moved from
		 */
		SVHandle ReadBuffer(uint32_t length);

	  private:
		std::unique_ptr<std::istream> stream;
		std::unique_ptr<ScopedView> view;
		std::shared_ptr<bool> viewState;

		uint64_t _ReadIntegerInternal(uint8_t bits);
		void VerifyOk();
	};
}