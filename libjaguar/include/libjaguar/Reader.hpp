#pragma once

#include "DllHelper.hpp"

#include <istream>
#include <memory>

namespace libjaguar {
	/**
	 * @brief Low-level stateless Jaguar stream parser
	 *
	 * The sole purpose of this class is to read the stream and extract value data. It does @b not persist data between calls and is thus not compliant with the specification on its own.
	 *
	 * This class is move-only!
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
		 * This is to allow for applications to still control the stream, while ensuring that only one Reader can use it at a time
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::istream* operator->();

		/**
		 * @brief Access the underlying stream to perform operations outside of the parser
		 *
		 * This is to allow for applications to still control the stream, while ensuring that only one Reader can use it at a time
		 *
		 * @return The stream, or @c nullptr if this object has been moved from
		 */
		std::istream* operator*();

	  private:
		std::unique_ptr<std::istream> stream;
	};
}