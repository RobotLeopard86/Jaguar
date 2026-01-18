#pragma once

#include "DllHelper.hpp"
#include "Traits.hpp"

#include <span>
#include <istream>
#include <memory>

namespace libjaguar {
	/**
	 * @brief Abstraction for accessing a scoped area of the byte stream
	 *
	 * This class may not be copied or moved; it belongs to the Reader that created it. As such, it may be invalidated by that Reader at any time.
	 * You should avoid performing other stream operations until you are done with this object.
	 */
	class LJAPI ScopedView {
	  public:
		/**
		 * @brief Read some bytes from the stream into the buffer
		 *
		 * @param out The destination to write to
		 * @param byteCount The number of bytes to read
		 *
		 * @throws std::runtime_error If the byte count to read exceeds the size of the output buffer
		 * @throws std::runtime_error If the byte count to read exceeds the number of remaining bytes
		 * @throws std::runtime_error If an IO error occurs while reading
		 * @throws std::runtime_error If the view is invalid
		 */
		template<byte_range R>
		void Read(R& out, uint32_t byteCount) {
			std::span<std::byte> span(out.begin(), out.end());
			_ReadInternal(span, byteCount);
		}

		/**
		 * @brief Check how many bytes remain in the scoped view that may be read
		 *
		 * @return The number of bytes left
		 *
		 * @throws std::runtime_error If the view is invalid
		 */
		uint32_t GetBytesRemaining() const;

		/**
		 * @brief Discard a certain amount of bytes
		 * @param byteCount The number of bytes to read
		 *
		 * @throws std::runtime_error If the byte count to read exceeds the number of remaining bytes
		 * @throws std::runtime_error If an IO error occurs
		 * @throws std::runtime_error If the view is invalid
		 */
		void Discard(uint32_t byteCount);

		/**
		 * @brief Discard the rest of the bytes in the view and advance the underlying stream to the end of the view
		 *
		 * @throws std::runtime_error If the view is invalid
		 */
		void DiscardAll();

		/**
		 * @brief Check if the view is still valid
		 *
		 * @return The view's validity state
		 */
		bool IsValid() noexcept {
			if(!stream->good()) valid = false;
			return valid;
		}

		///@cond
		ScopedView(const ScopedView&) = delete;
		ScopedView& operator=(const ScopedView&) = delete;
		ScopedView(ScopedView&&) = delete;
		ScopedView& operator=(const ScopedView&&) = delete;
		///@endcond

	  private:
		ScopedView(std::istream* streamPtr, std::streamoff size);
		friend class Reader;
		friend class ScopedViewStreambuf;

		std::istream* stream;
		std::streampos end;
		bool valid;
		bool eof;

		void _ReadInternal(std::span<std::byte>& out, uint32_t byteCount);
	};

	/**
	 * @brief Safe, move-only, non-owning wrapper for accessing a ScopedView
	 */
	class LJAPI SVHandle {
	  public:
		///@cond
		SVHandle(const SVHandle&) = delete;
		SVHandle& operator=(const SVHandle&) = delete;
		SVHandle(SVHandle&& other) : view(std::exchange(other.view, nullptr)), valid(std::exchange(other.valid, {})) {}
		SVHandle& operator=(SVHandle&&);
		///@endcond

		ScopedView* operator->() {
			if(valid && *valid) return view;
			throw std::runtime_error("Cannot access an invalid scoped view!");
		}

		bool IsHandleValid() {
			if(valid && *valid) return view->IsValid();
			return false;
		}

	  private:
		ScopedView* view;
		std::shared_ptr<bool> valid;

		SVHandle() {}

		friend class Reader;
	};
}