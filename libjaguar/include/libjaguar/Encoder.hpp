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
		 * @brief Get a reference to the Writer being used
		 *
		 * @return The writer
		 *
		 * @throws std::runtime_error If the writer object is invalid due to moving
		 */
		Writer& GetWriter();

	  private:
		Writer writer;
		bool writerValid = true;
	};
}