#pragma once

#include <cstdint>
#include <type_traits>

namespace libjaguar {
	///@cond
	template<typename T>
	struct is_integer : public std::false_type {
	};

	template<>
	struct is_integer<uint8_t> : public std::true_type {};
	template<>
	struct is_integer<int8_t> : public std::true_type {};
	template<>
	struct is_integer<uint16_t> : public std::true_type {};
	template<>
	struct is_integer<int16_t> : public std::true_type {};
	template<>
	struct is_integer<uint32_t> : public std::true_type {};
	template<>
	struct is_integer<int32_t> : public std::true_type {};
	template<>
	struct is_integer<uint64_t> : public std::true_type {};
	template<>
	struct is_integer<int64_t> : public std::true_type {};

	template<typename T>
	inline constexpr bool is_integer_v = is_integer<T>::value;

	template<typename T>
	concept integer = is_integer_v<T>;

	template<typename T>
	struct is_number : public std::false_type {};

	template<integer T>
	struct is_number<T> : public std::true_type {};
	template<>
	struct is_number<float> : public std::true_type {};
	template<>
	struct is_number<double> : public std::true_type {};

	template<typename T>
	inline constexpr bool is_number_v = is_number<T>::value;

	template<typename T>
	concept number = is_number_v<T>;

	template<number T>
	struct bits {};

	template<number T>
		requires is_integer_v<T> && std::is_unsigned_v<T>
	struct bits<T> : public bits<std::make_signed_t<T>> {};

	template<>
	struct bits<int8_t> : public std::integral_constant<uint8_t, 8> {
	};
	template<>
	struct bits<int16_t> : public std::integral_constant<uint8_t, 16> {
	};
	template<>
	struct bits<int32_t> : public std::integral_constant<uint8_t, 32> {
	};
	template<>
	struct bits<int64_t> : public std::integral_constant<uint8_t, 64> {
	};
	template<>
	struct bits<float> : public std::integral_constant<uint8_t, 32> {
	};
	template<>
	struct bits<double> : public std::integral_constant<uint8_t, 64> {
	};

	template<number T>
	inline constexpr uint8_t bits_v = bits<T>::value;
	///@endcond
}