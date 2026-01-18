#pragma once

#include "DllHelper.hpp"
#include "Traits.hpp"
#include <stdexcept>

namespace libjaguar {
	///@cond
	template<number T, uint8_t C>
		requires(C < 1 || C > 4)
	class Vector {};
	///@endcond

	/**
	 * @brief 2-component vector
	 *
	 * @tparam T The contained type
	 */
	template<number T>
	class LJAPI Vector<T, 2> {
	  public:
		//This union stuff allows the two names to refer to the same storage for XY/RG access

		///@c x and @c r are aliases for each other to allow for using XY or RG naming
		union {
			T x, r;
		};

		///@c y and @c g are aliases for each other to allow for using XY or RG naming
		union {
			T y, g;
		};
	};

	/**
	 * @brief 3-component vector
	 *
	 * @tparam T The contained type
	 */
	template<number T>
	class LJAPI Vector<T, 3> {
	  public:
		//This union stuff allows the two names to refer to the same storage for XYZ/RGB access

		///@c x and @c r are aliases for each other to allow for using XYZ or RGB naming
		union {
			T x, r;
		};

		///@c y and @c g are aliases for each other to allow for using XYZ or RGB naming
		union {
			T y, g;
		};

		///@c z and @c b are aliases for each other to allow for using XYZ or RGB naming
		union {
			T z, b;
		};
	};

	/**
	 * @brief 4-component vector
	 *
	 * @tparam T The contained type
	 */
	template<number T>
	class LJAPI Vector<T, 4> {
	  public:
		//This union stuff allows the two names to refer to the same storage for XYZW/RGBA access

		///@c x and @c r are aliases for each other to allow for using XYZW or RGBA naming
		union {
			T x, r;
		};

		///@c y and @c g are aliases for each other to allow for using XYZW or RGBA naming
		union {
			T y, g;
		};

		///@c z and @c b are aliases for each other to allow for using XYZW or RGBA naming
		union {
			T z, b;
		};

		///@c w and @c a are aliases for each other to allow for using XYZW or RGBA naming
		union {
			T w, a;
		};
	};

	/**
	 * @brief Column-major layout matrix
	 *
	 * @tparam T The contained type
	 */
	template<number T, uint8_t W, uint8_t H>
		requires(W >= 2 && W <= 4 && H >= 2 && H <= 4)
	class LJAPI Matrix {
	  public:
		/**
		 * @brief Access a column of data
		 *
		 * @param col The column index to retrieve
		 *
		 * @return A reference to the column data
		 *
		 * @throws std::runtime_error If an out-of-bounds column is requested
		 */
		std::array<T, H>& operator[](uint8_t col) {
			if(col > (W - 2)) throw std::runtime_error("Out of bounds matrix access");
			return data[col];
		}

		/**
		 * @brief Access a column of data (const)
		 *
		 * @param col The column index to retrieve
		 *
		 * @return A const reference to the column data
		 *
		 * @throws std::runtime_error If an out-of-bounds column is requested
		 */
		const std::array<T, H>& operator[](uint8_t col) const {
			if(col > (W - 2)) throw std::runtime_error("Out of bounds matrix access");
			return data[col];
		}

	  private:
		std::array<std::array<T, H>, W> data;
	};
}