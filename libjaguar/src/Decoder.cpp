#include "libjaguar/Decoder.hpp"
#include "libjaguar/ValueHeader.hpp"

#include <stdexcept>

namespace libjaguar {
	Decoder::Decoder(Reader&& reader) : reader(std::move(reader)), readerValid(true), failFlag(false) {}

	Decoder::Decoder(Decoder&& other) : reader(std::move(other.reader)), readerValid(true), failFlag(false) {
		other.readerValid = false;
	}

	Decoder& Decoder::operator=(Decoder&& other) {
		if(this != &other) {
			reader = std::move(other.reader);
			readerValid = true;
			failFlag = false;
			other.readerValid = false;
		}
		return *this;
	}

	Reader&& Decoder::ReleaseReader() && {
		if(!readerValid) throw std::runtime_error("Decoder has no valid reader!");
		return std::move(reader);
	}

	void Decoder::Parse() {
		if(!readerValid) throw std::runtime_error("Decoder has no valid reader!");
		if(index.has_value()) throw std::runtime_error("Stream has already been parsed!");

		while(!reader->eof()) {
			//Read next header
			ValueHeader header = reader.ReadHeader();

			//Do the appropriate thing
			switch(header.type) {
				case TypeTag::String:
				case TypeTag::ByteBuffer:
				case TypeTag::Substream:
				case TypeTag::Boolean:
				case TypeTag::Float32:
				case TypeTag::Float64:
				case TypeTag::SInt8:
				case TypeTag::SInt16:
				case TypeTag::SInt32:
				case TypeTag::SInt64:
				case TypeTag::UInt8:
				case TypeTag::UInt16:
				case TypeTag::UInt32:
				case TypeTag::UInt64:
				case TypeTag::List:
				case TypeTag::UnstructuredObj:
				case TypeTag::StructuredObj:
				case TypeTag::StructuredObjTypeDecl:
				case TypeTag::ScopeBoundary:
				case TypeTag::Vector:
				case TypeTag::Matrix: break;
			}
		}
	}
}