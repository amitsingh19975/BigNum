#ifndef DARK_BIG_NUM_DIVISION_HPP
#define DARK_BIG_NUM_DIVISION_HPP

#include "type_traits.hpp"
#include <cassert>
#include <utility>

namespace dark::internal::integer {

	// 1. Long division
	inline static constexpr auto long_div(
		is_basic_integer auto const& numerator,
		is_basic_integer auto const& denominator,
		is_basic_integer auto& quotient,
		is_basic_integer auto& remainder
	) noexcept -> void {
		auto n = numerator.bits();
		assert(quotient.bits() >= n && "quotient should have enough space");
		assert(remainder.bits() >= n && "remainder should have enough space");

		// TODO: should we throw divide-by-zero exception, return expected or return nothing?
		if (denominator.is_zero()) return;

		for (auto i = n; i > 0; --i) {
			auto const idx = i - 1;
			remainder.shift_left_mut(1, true);
			
			remainder.set_bit(0, numerator.get_bit(idx), true);
			if (remainder >= denominator) {
				remainder.sub_mut(denominator);
				quotient.set_bit(idx, 1);
			}
		}	
	}

	inline static constexpr auto log2_approx(std::size_t x) noexcept -> std::size_t {
		// find the nearest power of 2;
		// log2(2^k) = k * log2(2) ~ k
		auto count = 0zu;
		while (x) {
			x >>= 1;
			++count;
		}
		return count;
	}

	// 2. Newton–Raphson division
	inline static constexpr auto newton_raphson_div(
		is_basic_integer auto const& numerator,
		is_basic_integer auto const& denominator,
		is_basic_integer auto& quotient,
		is_basic_integer auto& remainder
	) noexcept -> void {
		// 1. Find 'k' such that max number of bit need to reperesent remainder
		auto const k = denominator.bits();
		auto factor = remainder;
		factor.set_bit(0, true, true);
		factor.shift_left_mut(k, true);

		// 2. number of iteration is equal to log2(number_of_bits(numerator)) 
		auto const iter = log2_approx(numerator.bits());
		auto x = numerator;
		x.shift_right_mut(k >> 1);

		for (auto i = 0zu; i < iter; ++i) {
			// 3. x(n+1) = x(n) * (2^k - d * x(n)) / 2^k
			auto t1 = denominator.mul(x); // d * x
			
			auto t2 = factor.sub(t1); // 2^k - t1

			auto res = t2.shift_right_mut(k); // t2 / 2^k

			auto t3 = x.mul(*t2); // x * t2
			x = std::move(t3);
		}

		quotient = std::move(x);
	}

	// 3. Large-integer methods
		

} // namespace dark::internal::integer

#endif // DARK_BIG_NUM_DIVISION_HPP
