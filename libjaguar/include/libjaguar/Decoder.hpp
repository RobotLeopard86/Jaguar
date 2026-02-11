#pragma once

#include "DllHelper.hpp"
#include "Index.hpp"
#include "Reader.hpp"

namespace libjaguar {
	/**
	 * @brief Stateful Jaguar stream interpreter and index builder
	 *
	 * @note This class does not return any values; it only builds a structure.
	 * Your stream must be seekable to allow rewinding if you want to later read those values using the produced Index.
	 *
	 * <b>This class is move-only!</b>
	 */
	class LJAPI Decoder {
		/**
		 * @brief Create a decoder that will own and maintain a Reader
		 *
		 * @param reader The reader to use
		 */
		explicit Decoder(Reader&& reader);

		///@cond
		Decoder(const Decoder&) = delete;
		Decoder& operator=(const Decoder&) = delete;
		Decoder(Decoder&&);
		Decoder& operator=(Decoder&&);
		///@endcond

		/**
		 * @brief Release the reader for use outside the decoder and invalidate it
		 *
		 * @note This function requires you to move from the decoder, like this:
		 * @code {.cpp}
		 * Reader myReader = std::move(myDecoder).GetReader();
		 * @endcode
		 *
		 * @return The reader
		 *
		 * @throws std::runtime_error If the reader object is invalid due to moving
		 */
		Reader&& ReleaseReader() &&;

		/**
		 * @brief Access the stream structure index
		 *
		 * @return The index
		 *
		 * @throws std::runtime_error If parsing errors occurred
		 */
		const Index& GetIndex() const;

		/**
		 * @brief Parse the Jaguar stream structure until EOF is reached or the decoder encounters invalid data
		 *
		 * @throws std::runtime_error If parsing errors occurred --- this will invalidate the decoder
		 */
		void Parse();

		/**
		 * @brief Check if the decoder has encountered parsing errors
		 *
		 * @return The failure flag
		 */
		bool Failed();

	  private:
		Reader reader;
		Index index;
		bool readerValid = true;
		bool failFlag = false;
	};
}