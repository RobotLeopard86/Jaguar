#include "libjaguar/Writer.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/ValueHeader.hpp"
#include "Utilities.hpp"

#include <istream>
#include <array>
#include <iterator>
#include <stdexcept>
#include <algorithm>
#include <cmath>

namespace libjaguar {
	Writer::Writer(std::unique_ptr<std::ostream>&& ostream) : stream(std::move(ostream)) {}

	Writer::Writer(Writer&& other) : stream(std::move(other.stream)) {}

	Writer& Writer::operator=(Writer&& other) {
		if(this != &other) stream = std::move(other.stream);
		return *this;
	}

	std::ostream* Writer::operator->() {
		return (stream ? stream.get() : nullptr);
	}

	std::ostream* Writer::operator*() {
		return (stream ? stream.get() : nullptr);
	}

	void Writer::_WriteIntegerInternal(uint64_t value, uint8_t bits) {
		if(!stream) throw std::runtime_error("Cannot perform operations without a backing stream!");

		//Write out integer in little endian
		const uint8_t bytes = bits / 8;
		uint64_t work = value;
		for(uint8_t i = 0; i < bytes; ++i) {
			//Get the lowest 8 bits of the work value and write it
			const uint8_t byte = (work & 0xFF);
			stream->put(byte);

			//Discard just-written bits and move everything else 8 bits right
			work >>= 8;
		}
	}

	void Writer::_WriteBufferInternal(std::span<unsigned char>& value) {
		if(!stream) throw std::runtime_error("Cannot perform operations without a backing stream!");

		stream->write(reinterpret_cast<const char*>(value.data()), value.size_bytes());
	}

	void Writer::WriteBool(bool value) {
		if(!stream) throw std::runtime_error("Cannot perform operations without a backing stream!");

		uint8_t val = (value ? 1 : 0);
		stream->put(val);
	}

	void Writer::WriteString(const std::string& value) {
		if(!stream) throw std::runtime_error("Cannot perform operations without a backing stream!");
		if(!CheckUTF8(value)) throw std::runtime_error("String is not valid UTF-8!");
		if(value.size() >= std::pow(2, 24)) throw std::runtime_error("String is longer than maximum legal size!");

		stream->write(value.data(), value.size());
	}

	void Writer::WriteStringFromStream(std::istream* istream, std::size_t length) {
		if(!stream) throw std::runtime_error("Cannot perform operations without a backing stream!");
		if(istream == nullptr) throw std::runtime_error("Cannot write buffer from a null source stream!");
		if(!(*istream)) throw std::runtime_error("Cannot write buffer from an invalid source stream!");

		//Wrapper class for checking UTF-8 before writing to real output
		class CheckWrapperBuf : public std::streambuf {
		  public:
			explicit CheckWrapperBuf(std::streambuf* dest)
			  : outSB(dest) {}

		  protected:
			std::streamsize xsputn(const char* s, std::streamsize n) override {
				std::string_view input {s, static_cast<std::size_t>(n)};

				//Append to carry buffer
				carry.append(input.data(), input.size());

				//Process only full 4-byte chunks
				std::size_t full = carry.size() & ~std::size_t(3);
				if(full > 0) {
					std::string_view chunk {carry.data(), full};
					if(!CheckUTF8(chunk))
						throw std::runtime_error("Cannot write non-UTF-8 data to a string!");
					if(outSB->sputn(chunk.data(), full) != static_cast<std::streamsize>(full))
						return 0;

					//Remove processed bytes, keep remainder (0–3 bytes)
					carry.erase(0, full);
				}
				return n;
			}

			int overflow(int ch) override {
				if(ch == traits_type::eof())
					return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
				char c = static_cast<char>(ch);
				return xsputn(&c, 1) == 1 ? ch : traits_type::eof();
			}

			int sync() override {
				if(!carry.empty()) {
					if(!CheckUTF8(carry))
						return -1;
					if(outSB->sputn(carry.data(), carry.size()) != static_cast<std::streamsize>(carry.size()))
						return -1;
					carry.clear();
				}
				return outSB->pubsync();
			}

		  private:
			std::streambuf* outSB;
			std::string carry;
		};

		///...and the corresponding stream
		class CheckWrapperOStream : public std::ostream {
		  public:
			explicit CheckWrapperOStream(std::ostream& out)
			  : std::ostream(&buf), buf(out.rdbuf()) {}

		  private:
			CheckWrapperBuf buf;
		};

		//Copy stream-to-stream efficiently while checking UTF-8
		CheckWrapperOStream wrapper(*stream);
		std::copy_n(std::istreambuf_iterator<char>(*istream), length, std::ostreambuf_iterator<char>(wrapper));
		stream->flush();
	}

	void Writer::WriteBufferFromStream(std::istream* istream, std::size_t length) {
		if(!stream) throw std::runtime_error("Cannot perform operations without a backing stream!");
		if(istream == nullptr) throw std::runtime_error("Cannot write buffer from a null source stream!");
		if(!(*istream)) throw std::runtime_error("Cannot write buffer from an invalid source stream!");

		//Copy stream-to-stream efficiently
		std::copy_n(std::istreambuf_iterator<char>(*istream), length, std::ostreambuf_iterator<char>(*stream));
		stream->flush();
	}

	void Writer::WriteHeader(const ValueHeader& header, bool noIdentifier) {
		if(!stream) throw std::runtime_error("Cannot perform operations without a backing stream!");

		//Scope boundary edge-case
		if(header.type == TypeTag::ScopeBoundary) {
			stream->put(static_cast<uint8_t>(TypeTag::ScopeBoundary));
			return;
		}

		//Basic checks for other types
		if(header.name.size() < 1 || header.name.size() > UINT8_MAX) throw std::runtime_error("Header name string is invalid length!");
		if(!CheckUTF8(header.name)) throw std::runtime_error("Header name string is not valid UTF-8!");
		if(header.type == TypeTag::StructuredObj) {
			if(header.typeID.size() < 1 || header.typeID.size() > UINT8_MAX) throw std::runtime_error("Header type ID string is invalid length!");
			if(!CheckUTF8(header.typeID)) throw std::runtime_error("Header type ID string is not valid UTF-8!");
		}

		//Write identifier
		if(!noIdentifier) {
			//Write type tag
			stream->put(static_cast<uint8_t>(header.type));

			//Write name string
			_WriteIntegerInternal(header.name.size(), 8);
			WriteString(header.name);
		}

		//Write type-specific data
		switch(header.type) {
			case TypeTag::List:
				stream->put(static_cast<uint8_t>(header.elementType));
				if(header.elementType == TypeTag::StructuredObj) {
					_WriteIntegerInternal(header.typeID.size(), 8);
					WriteString(header.typeID);
				} else if(header.elementType == TypeTag::Vector || header.elementType == TypeTag::Matrix) {
					stream->put(static_cast<uint8_t>(header.nestedElementType));
					_WriteIntegerInternal(header.width, 8);
					if(header.elementType == TypeTag::Matrix) _WriteIntegerInternal(header.height, 8);
				}
				_WriteIntegerInternal(header.size, bits_v<decltype(header.size)>);
				break;
			case TypeTag::Vector:
				stream->put(static_cast<uint8_t>(header.elementType));
				_WriteIntegerInternal(header.width, bits_v<decltype(header.width)>);
				break;
			case TypeTag::Matrix:
				stream->put(static_cast<uint8_t>(header.elementType));
				_WriteIntegerInternal(header.width, bits_v<decltype(header.width)>);
				_WriteIntegerInternal(header.height, bits_v<decltype(header.height)>);
				break;
			case TypeTag::StructuredObj:
				_WriteIntegerInternal(header.typeID.size(), 8);
				WriteString(header.typeID);
				break;
			case TypeTag::StructuredObjTypeDecl:
			case TypeTag::UnstructuredObj:
				_WriteIntegerInternal(header.fieldCount, bits_v<decltype(header.fieldCount)>);
				break;
			case TypeTag::String:
			case TypeTag::ByteBuffer:
				_WriteIntegerInternal(header.size, bits_v<decltype(header.size)>);
				break;
			default: break;
		}
	}
}