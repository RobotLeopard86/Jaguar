#pragma once

#include "DllHelper.hpp"

#include <ostream>

namespace libjaguar {
	/**
	 * @brief Low-level stateless Jaguar stream writer
	 *
	 * The sole purpose of this class is to write values to the stream, not to keep track of the data context
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
		 * @brief Access the underlying stream to perform operations outside of the parser
		 *
		 * This is to allow for applications to still control the stream, while ensuring that only one Writer can use it at a time
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::ostream* operator->();

	  private:
		std::ostream stream;
		bool moved = false;
	};
}