#pragma once

#include "DllHelper.hpp"
#include "Index.hpp"
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
		 * @brief Get a reference to the Reader being used
		 *
		 * @return The reader
		 *
		 * @throws std::runtime_error If the reader object is invalid due to moving
		 */
		Reader& GetReader();

		/**
		 * @brief Access the stream structure index
		 *
		 * @return The index
		 *
		 * @throws
		 */
		const Index& GetIndex();

	  private:
		Reader reader;
		bool readerValid = true;
	};
}