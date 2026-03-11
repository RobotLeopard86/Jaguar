#pragma once

#include "DllHelper.hpp"
#include "Writer.hpp"
#include "Index.hpp"
#include "libjaguar/MathTypes.hpp"
#include "libjaguar/TypeTags.hpp"

#include <cstdint>

namespace libjaguar {
	/**
	 * @brief Interface for Encoder value requisition
	 *
	 * @note When implementing this, use exceptions to communicate failure to the encoder.
	 */
	class LJAPI PayloadProvider {
	  public:
		/**
		 * @brief Called to request a string value; may be called multiple times for large strings
		 *
		 * @param id The ID of the value to fetch
		 * @param out The stream to write output to (use @c write, not @c operator<< to ensure exact transfer)
		 * @param chunkSize The requested amount of string data to send (used for large strings); will always be aligned to 4 bytes except for at the end of strings to ensure that all sequences are valid UTF-8
		 * @param offset Where in the string to start reading from
		 *
		 * @warning If the requested amount of data is not written (too much or too little), encoding will <b>halt</b>.
		 * @warning If the returned data is not proper UTF-8, encoding will <b>halt</b>.
		 */
		virtual void String(uint64_t id, std::ostream& out, std::size_t chunkSize, std::size_t offset) = 0;

		/**
		 * @brief Called to request a byte buffer value (for byte buffers or substreams); may be called multiple times for large buffers
		 *
		 * @param id The ID of the value to fetch
		 * @param out The stream to write output to (use @c write, not @c operator<< to avoid interpretation of bytes as a string)
		 * @param chunkSize The requested amount of data to send
		 * @param offset Where in the buffer to start reading from*
		 *
		 * @warning If the requested amount of data is not written (too much or too little), encoding will <b>halt</b>.
		 */
		virtual void Buffer(uint64_t id, std::ostream& out, std::size_t chunkSize, std::size_t offset) = 0;

		/**
		 * @brief Called to request a boolean value
		 *
		 * @param id The ID of the value to fetch
		 *
		 * @return The boolean value
		 */
		virtual bool Boolean(uint64_t id) = 0;

		/**
		 * @brief Called to request a signed integer value
		 *
		 * @param id The ID of the value to fetch
		 * @param bits The bit width of the integer being requested (e.g., 32 for an int32_t)
		 *
		 * @return The value
		 *
		 * @warning If the provided value is larger than is possible to fit in the requested bit width, encoding will <b>halt</b>.
		 */
		virtual int64_t SignedInt(uint64_t id, uint8_t bits) = 0;

		/**
		 * @brief Called to request an unsigned integer value
		 *
		 * @param id The ID of the value to fetch
		 * @param bits The bit width of the integer being requested (e.g., 32 for a uint32_t)
		 *
		 * @return The value
		 *
		 * @warning If the provided value is larger than is possible to fit in the requested bit width, encoding will <b>halt</b>.
		 */
		virtual uint64_t UnsignedInt(uint64_t id, uint8_t bits) = 0;

		/**
		 * @brief Called to request a single-precision (32-bit) floating point value
		 *
		 * @param id The ID of the value to fetch
		 *
		 * @return The value
		 */
		virtual float Float32(uint64_t id) = 0;

		/**
		 * @brief Called to request a double-precision (64-bit) floating point value
		 *
		 * @param id The ID of the value to fetch
		 *
		 * @return The value
		 */
		virtual double Float64(uint64_t id) = 0;

		/**
		 * @brief Called to request a signed integer component of a vector; may be called multiple times to fill the entire vector
		 *
		 * @param id The ID of the vector being requested
		 * @param component The component being requested
		 * @param bits The bit width of the integer being requested (e.g., 32 for an int32_t)
		 *
		 * @return The element value
		 *
		 * @warning If the provided value is larger than is possible to fit in the requested bit width, encoding will <b>halt</b>.
		 */
		virtual int64_t SignedIntVec(uint64_t id, VecComponent component, uint8_t bits) = 0;

		/**
		 * @brief Called to request an unsigned integer component of a vector; may be called multiple times to fill the entire vector
		 *
		 * @param id The ID of the vector being requested
		 * @param component The component being requested
		 * @param bits The bit width of the integer being requested (e.g., 32 for a uint32_t)
		 *
		 * @return The element value
		 *
		 * @warning If the provided value is larger than is possible to fit in the requested bit width, encoding will <b>halt</b>.
		 */
		virtual uint64_t UnsignedIntVec(uint64_t id, VecComponent component, uint8_t bits) = 0;

		/**
		 * @brief Called to request a single-precision (32-bit) floating point of a vector; may be called multiple times to fill the entire vector
		 *
		 * @param id The ID of the vector being requested
		 * @param component The component being requested
		 *
		 * @return The element value
		 */
		virtual float Float32Vec(uint64_t id, VecComponent component) = 0;

		/**
		 * @brief Called to request a double-precision (64-bit) floating point of a vector; may be called multiple times to fill the entire vector
		 *
		 * @param id The ID of the vector being requested
		 * @param component The component being requested
		 *
		 * @return The element value
		 */
		virtual double Float64Vec(uint64_t id, VecComponent component) = 0;

		/**
		 * @brief Called to request a signed integer component of a matrix; may be called multiple times to fill the entire matrix
		 *
		 * @param id The ID of the vector being requested
		 * @param x The x coordinate of the value being requested (column # starting from 0 at left)
		 * @param y The y coordinate of the value being requested (row # starting from 0 at top)
		 * @param bits The bit width of the integer being requested (e.g., 32 for an int32_t)
		 *
		 * @return The element value
		 *
		 * @warning If the provided value is larger than is possible to fit in the requested bit width, encoding will <b>halt</b>.
		 */
		virtual int64_t SignedIntMat(uint64_t id, uint8_t x, uint8_t y, uint8_t bits) = 0;

		/**
		 * @brief Called to request an unsigned integer component of a matrix; may be called multiple times to fill the entire matrix
		 *
		 * @param id The ID of the vector being requested
		 * @param x The x coordinate of the value being requested (column # starting from 0 at left)
		 * @param y The y coordinate of the value being requested (row # starting from 0 at top)
		 * @param bits The bit width of the integer being requested (e.g., 32 for a uint32_t)
		 *
		 * @return The element value
		 *
		 * @warning If the provided value is larger than is possible to fit in the requested bit width, encoding will <b>halt</b>.
		 */
		virtual uint64_t UnsignedIntMat(uint64_t id, uint8_t x, uint8_t y, uint8_t bits) = 0;

		/**
		 * @brief Called to request a single-precision (32-bit) floating point of a matrix; may be called multiple times to fill the entire matrix
		 *
		 * @param id The ID of the vector being requested
		 * @param x The x coordinate of the value being requested (column # starting from 0 at left)
		 * @param y The y coordinate of the value being requested (row # starting from 0 at top)
		 *
		 * @return The element value
		 */
		virtual float Float32Mat(uint64_t id, uint8_t x, uint8_t y) = 0;

		/**
		 * @brief Called to request a double-precision (64-bit) floating point of a matrix; may be called multiple times to fill the entire matrix
		 *
		 * @param id The ID of the vector being requested
		 * @param x The x coordinate of the value being requested (column # starting from 0 at left)
		 * @param y The y coordinate of the value being requested (row # starting from 0 at top)
		 *
		 * @return The element value
		 */
		virtual double Float64Mat(uint64_t id, uint8_t x, uint8_t y) = 0;
	};

	/**
	 * @brief Stateful Jaguar stream builder
	 *
	 * <b>This class is move-only!</b>
	 */
	class LJAPI Encoder {
	  public:
		/**
		 * @brief Create a encoder that will own and maintain a Writer
		 *
		 * @param writer The writer to use
		 */
		Encoder(Writer&& writer);

		///@cond
		Encoder(const Encoder&) = delete;
		Encoder& operator=(const Encoder&) = delete;
		Encoder(Encoder&&);
		Encoder& operator=(Encoder&&);
		///@endcond

		/**
		 * @brief Release the writer for use outside the encoder
		 *
		 * @note This function requires you to move from the encoder to prevent further use of this object without a writer, like so:
		 * @code {.cpp}
		 * Writer myWriter = std::move(myEncoder).ReleaseWriter();
		 * @endcode
		 *
		 * @return The writer
		 *
		 * @throws std::runtime_error If the writer object is invalid due to moving
		 */
		Writer&& ReleaseWriter() &&;

		/**
		 * @brief Write the provided structure and data to the stream
		 *
		 * @param index The Index to write
		 * @param provider The provider to use for acquiring index entry payloads
		 *
		 * @throw std::runtime_error If any types in the index fail type layout validation
		 * @throw std::runtime_error If the index is invalidly structured (e.g., a ValueEntry with an object type)
		 * @throw std::runtime_error If the provider fails to provide data
		 */
		template<typename T>
			requires std::is_base_of_v<PayloadProvider, T>
		void Write(const Index& index, const T& provider) {
			_Write(index, const_cast<PayloadProvider*>(static_cast<const PayloadProvider*>(&provider)));
		}

	  private:
		Writer writer;
		bool writerValid = true;

		void _WriteNum(TypeTag type, uint64_t asBits);
		void _WriteValue(const ValueEntry& entry, PayloadProvider* provider);
		void _WriteScope(const Index& index, const ScopeEntry& entry, PayloadProvider* provider);
		void _Write(const Index& index, PayloadProvider* provider);
	};
}