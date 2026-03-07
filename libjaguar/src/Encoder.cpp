#include "libjaguar/Encoder.hpp"
#include "libjaguar/Index.hpp"
#include "libjaguar/MathTypes.hpp"
#include "libjaguar/TypeTags.hpp"
#include "libjaguar/ValueHeader.hpp"

#include <climits>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace libjaguar {
	Encoder::Encoder(Writer&& writer) : writer(std::move(writer)), writerValid(true) {}

	Encoder::Encoder(Encoder&& other) : writer(std::move(other.writer)), writerValid(true) {
		other.writerValid = false;
	}

	Encoder& Encoder::operator=(Encoder&& other) {
		if(this != &other) {
			writer = std::move(other.writer);
			writerValid = true;
			other.writerValid = false;
		}
		return *this;
	}

	Writer&& Encoder::ReleaseWriter() && {
		if(!writerValid) throw std::runtime_error("Encoder has no valid writer!");
		return std::move(writer);
	}

	void Encoder::_WriteNum(TypeTag type, uint64_t asBits) {
		switch(type) {
			case TypeTag::Float32:
				writer.WriteInteger<uint32_t>(static_cast<uint32_t>(asBits));
				break;
			case TypeTag::Float64:
				writer.WriteInteger<uint64_t>(asBits);
				break;
			case TypeTag::SInt8:
			case TypeTag::SInt16:
			case TypeTag::SInt32:
			case TypeTag::SInt64: {
				//This looks weird but it's just converting the type tag to the approriate number of bytes for the type
				uint8_t bytes = std::pow(2, (static_cast<uint8_t>(type) & 0xF) - 0xA);
				int64_t val = std::bit_cast<int64_t, uint64_t>(asBits);
				if(std::abs(val) > (std::pow(2, bytes * 8) / 2 - 1) && val > std::pow(2, bytes * 8) / -2) throw std::runtime_error("Provider returned value too large for the requested type!");
				switch(bytes) {
					case 1:
						writer.WriteInteger<uint8_t>(asBits);
						break;
					case 2:
						writer.WriteInteger<uint16_t>(asBits);
						break;
					case 4:
						writer.WriteInteger<uint32_t>(asBits);
						break;
					case 8:
						writer.WriteInteger<uint64_t>(asBits);
						break;
				}
				break;
			}
			case TypeTag::UInt8:
			case TypeTag::UInt16:
			case TypeTag::UInt32:
			case TypeTag::UInt64: {
				//This looks weird but it's just converting the type tag to the approriate number of bytes for the type
				uint8_t bytes = std::pow(2, (static_cast<uint8_t>(type) & 0xF) - 0xA);
				if(asBits > (std::pow(2, bytes * 8) - 1)) throw std::runtime_error("Provider returned value too large for the requested type!");
				switch(bytes) {
					case 1:
						writer.WriteInteger<int8_t>(asBits);
						break;
					case 2:
						writer.WriteInteger<int16_t>(asBits);
						break;
					case 4:
						writer.WriteInteger<int32_t>(asBits);
						break;
					case 8:
						writer.WriteInteger<int64_t>(asBits);
						break;
				}
				break;
			}
			default: break;
		}
	}

	void Encoder::_WriteValue(const ValueEntry& entry, PayloadProvider* provider) {
		//Check type
		if((static_cast<uint8_t>(entry.type) & 0xF0) >> 4 == 0x3) throw std::runtime_error("Cannot write a scope type as a value!");

		//Build header
		ValueHeader header = {};
		header.name = entry.name;
		header.type = entry.type;
		if(static_cast<uint8_t>(entry.type) <= 0xC) {
			//Buffer objects
			if(entry.size >= std::pow(2, 24)) throw std::runtime_error("String is too long!");
			header.size = entry.size;
		} else if(entry.type == TypeTag::Vector || entry.type == TypeTag::Matrix) {
			if(uint8_t asByte = static_cast<uint8_t>(entry.elementType); asByte < 0x0E || asByte > 0x2D) throw std::runtime_error("Invalid element type for vector/matrix!");
			header.elementType = entry.elementType;
			if(entry.width < 2 || entry.width > 4) throw std::runtime_error("Invalid vector/matrix width!");
			header.width = entry.width;
			if(entry.type == TypeTag::Matrix) {
				if(entry.height < 2 || entry.height > 4) throw std::runtime_error("Invalid matrix height!");
				header.height = entry.height;
			}
		}

		//Write the header
		writer.WriteHeader(header);

		//Start writing the data
		constexpr std::size_t chunkSize = 64 * 1024;//64 KiB (one KiB is 1024 bytes)
		switch(header.type) {
			//Buffers are TODO
			case TypeTag::String: {
			}
			case TypeTag::ByteBuffer:
			case TypeTag::Substream: {
			}
			case TypeTag::Boolean:
				writer.WriteBool(provider->Boolean(entry.id));
				break;
			case TypeTag::Float32:
				_WriteNum(TypeTag::Float32, (uint64_t)std::bit_cast<uint32_t, float>(provider->Float32(entry.id)));
				break;
			case TypeTag::Float64:
				_WriteNum(TypeTag::Float64, std::bit_cast<uint64_t, double>(provider->Float64(entry.id)));
				break;
			case TypeTag::SInt8:
			case TypeTag::SInt16:
			case TypeTag::SInt32:
			case TypeTag::SInt64:
				_WriteNum(header.type, std::bit_cast<uint64_t, int64_t>(provider->SignedInt(entry.id, std::pow(2, (static_cast<uint8_t>(header.type) & 0xF) - 0xA) * 8)));
				break;
			case TypeTag::UInt8:
			case TypeTag::UInt16:
			case TypeTag::UInt32:
			case TypeTag::UInt64:
				_WriteNum(header.type, provider->UnsignedInt(entry.id, std::pow(2, (static_cast<uint8_t>(header.type) & 0xF) - 0xA) * 8));
				break;
			case TypeTag::Vector: {
				for(uint8_t x = 0; x < header.width; ++x) {
					uint64_t val = [&]() -> uint64_t {
						switch(header.elementType) {
							case TypeTag::Float32:
								return (uint64_t)std::bit_cast<uint32_t, float>(provider->Float32Vec(entry.id, (VecComponent)x));
							case TypeTag::Float64:
								return std::bit_cast<uint64_t, double>(provider->Float64Vec(entry.id, (VecComponent)x));
							case TypeTag::SInt8:
							case TypeTag::SInt16:
							case TypeTag::SInt32:
							case TypeTag::SInt64:
								return std::bit_cast<uint64_t, int64_t>(provider->SignedIntVec(entry.id, (VecComponent)x, std::pow(2, (static_cast<uint8_t>(header.type) & 0xF) - 0xA) * 8));
							case TypeTag::UInt8:
							case TypeTag::UInt16:
							case TypeTag::UInt32:
							case TypeTag::UInt64:
								return provider->UnsignedIntVec(entry.id, (VecComponent)x, std::pow(2, (static_cast<uint8_t>(header.type) & 0xF) - 0xA) * 8);
							default: return 0;
						}
					}();
					_WriteNum(header.elementType, val);
				}
			}
			case TypeTag::Matrix: {
				for(uint8_t x = 0; x < header.width; ++x) {
					for(uint8_t y = 0; y < header.height; ++x) {
						uint64_t val = [&]() -> uint64_t {
							switch(header.elementType) {
								case TypeTag::Float32:
									return (uint64_t)std::bit_cast<uint32_t, float>(provider->Float32Mat(entry.id, x, y));
								case TypeTag::Float64:
									return std::bit_cast<uint64_t, double>(provider->Float64Mat(entry.id, x, y));
								case TypeTag::SInt8:
								case TypeTag::SInt16:
								case TypeTag::SInt32:
								case TypeTag::SInt64:
									return std::bit_cast<uint64_t, int64_t>(provider->SignedIntMat(entry.id, x, y, std::pow(2, (static_cast<uint8_t>(header.type) & 0xF) - 0xA) * 8));
								case TypeTag::UInt8:
								case TypeTag::UInt16:
								case TypeTag::UInt32:
								case TypeTag::UInt64:
									return provider->UnsignedIntMat(entry.id, x, y, std::pow(2, (static_cast<uint8_t>(header.type) & 0xF) - 0xA) * 8);
								default: return 0;
							}
						}();
						_WriteNum(header.elementType, val);
					}
				}
			}
			default: break;
		}
	}

	void Encoder::_WriteScope(const Index& index, const ScopeEntry& entry, PayloadProvider* provider) {
		//Write all values
		for(const ValueEntry& val : entry.subvalues) {
			_WriteValue(val, provider);
		}

		//Write all scopes
		for(const ScopeEntry& scope : entry.subscopes) {
			//Header

			//Body
		}

		//If not root, write boundary
		if(entry.id != index.root.id) writer->put(static_cast<uint8_t>(TypeTag::ScopeBoundary));
	}
}