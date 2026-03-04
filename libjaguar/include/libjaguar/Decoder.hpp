#pragma once

#include "DllHelper.hpp"
#include "Index.hpp"
#include "Reader.hpp"
#include "ValueHeader.hpp"
#include "libjaguar/Index.hpp"
#include "libjaguar/StructuredTypeLayout.hpp"
#include "libjaguar/TypeTags.hpp"
#include <optional>
#include <stdexcept>

namespace libjaguar {
	/**
	 * @brief Stateful Jaguar stream interpreter and index builder
	 *
	 * @note This class does not return any values; it only builds a structure.
	 * Your stream must be seekable to allow rewinding if you want to later read those values using the produced Index.
	 *
	 * @warning Because this class owns the Reader (and thus the stream), <b>do not let RAII destroy it</b> if you want to continue using the stream.
	 * Be sure to call @c ReleaseReader first to get the Reader back.
	 *
	 * <b>This class is move-only!</b>
	 */
	class LJAPI Decoder {
	  public:
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
		 * @brief Release the reader for use outside the decoder
		 *
		 * @note This function requires you to move from the decoder to prevent further use of this object without a reader, like so:
		 * @code {.cpp}
		 * Reader myReader = std::move(myDecoder).ReleaseReader();
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
		 * @throws std::runtime_error If parsing errors occurred or the stream has not yet been parsed
		 */
		const Index& GetIndex() const {
			if(!index.has_value()) throw std::runtime_error("Stream has not yet been parsed; no index is available!");
			if(!failFlag) return index.value();
			throw std::runtime_error("Cannot obtain the index; parsing errors occurred!");
		}

		/**
		 * @brief Parse the Jaguar stream structure until EOF is reached or the decoder encounters invalid data
		 *
		 * @throws std::runtime_error If parsing errors occurred --- this will invalidate the decoder
		 * @throws std::runtime_error If the stream has already been parsed
		 */
		void Parse();

		/**
		 * @brief Check if the decoder has encountered parsing errors
		 *
		 * @return The failure flag
		 */
		bool Failed() {
			return failFlag;
		}

	  private:
		Reader reader;
		std::optional<Index> index;
		bool readerValid = true;
		bool failFlag = false;
		uint8_t nest = 0;

		struct ScopeExpectations {
			TypeTag type;
			std::size_t fieldCount;
			std::string typeID;
			bool rootFlag;
		};

		void _ParseScopeInternal(ScopeEntry& scope, ScopeExpectations expectations, const std::string& scopePath);
		ValueEntry _ParseValueInternal(const ValueHeader& header, const std::string& scopePath);
	};
}