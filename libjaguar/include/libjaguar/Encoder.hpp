#pragma once

#include "DllHelper.hpp"
#include "Writer.hpp"

namespace libjaguar {
	/**
	 * @brief Stateful and contextual Jaguar data writer
	 *
	 * <b>This class is move-only!</b>
	 */
	class LJAPI Encoder {
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
		 * @brief Access the underlying stream to perform operations outside of the encoder
		 *
		 * This is to allow for applications to still control the stream, while ensuring that ownership stays with the underlying Writer
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::ostream* operator->();

		/**
		 * @brief Access the underlying stream to perform operations outside of the encoder
		 *
		 * This is to allow for applications to still control the stream, while ensuring that ownership stays with the underlying Writer
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::ostream* operator*();

	  private:
		Writer writer;
		bool writerValid = true;
	};
}