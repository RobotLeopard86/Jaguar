#pragma once

#include "DllHelper.hpp"
#include "Writer.hpp"
#include "libjaguar/Index.hpp"

namespace libjaguar {
	/**
	 * @brief Stateful and contextual Jaguar data writer
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
		 * Writer myReader = std::move(myEncoder).ReleaseWriter();
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
		 *
		 * @throw std::runtime_error If any types in the index fail type layout validation
		 */
		void Write(const Index& index);

	  private:
		Writer writer;
		bool writerValid = true;
	};
}