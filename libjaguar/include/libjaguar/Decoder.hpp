#pragma once

#include "DllHelper.hpp"
#include "Reader.hpp"

namespace libjaguar {
	/**
	 * @brief Stateful Jaguar stream interpreter and index builder
	 *
	 * <b>This class is move-only!</b>
	 */
	class LJAPI Decoder {
		/**
		 * @brief Create a decoder that will own and maintain a Reader
		 *
		 * @param reader The reader to use
		 */
		Decoder(Reader&& reader);

		///@cond
		Decoder(const Decoder&) = delete;
		Decoder& operator=(const Decoder&) = delete;
		Decoder(Decoder&&);
		Decoder& operator=(Decoder&&);
		///@endcond

		/**
		 * @brief Access the underlying stream to perform operations outside of the decoder
		 *
		 * This is to allow for applications to still control the stream, while ensuring that ownership stays with the underlying Reader
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::istream* operator->();

		/**
		 * @brief Access the underlying stream to perform operations outside of the decoder
		 *
		 * This is to allow for applications to still control the stream, while ensuring that ownership stays with the underlying Reader
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::istream* operator*();

	  private:
		Reader reader;
		bool readerValid = true;
	};
}