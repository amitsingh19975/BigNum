#ifndef DARK_BIG_NUM_DIVISION_HPP
#define DARK_BIG_NUM_DIVISION_HPP

#include "add_sub.hpp"
#include "block_info.hpp"
#include "type_traits.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace dark::internal::integer {

	inline static constexpr auto fast_div(
		is_basic_integer auto const& numerator,
		is_basic_integer auto const& denominator,
		is_basic_integer auto& quotient,
		is_basic_integer auto& remainder
	) noexcept -> bool {
		auto n = numerator.bits();
		auto m = denominator.bits();
		if (n > 2 || m > 2) return false;
		if (n == 0 || numerator.is_zero()) return true;

		// TODO: should we throw divide-by-zero exception, return expected or return nothing?
		if (denominator.is_zero()) return true;
		
		auto num = combine_two_blocks(numerator[0], numerator.size() == 1 ? 0 : numerator[1]);
		auto den = combine_two_blocks(denominator[0], numerator.size() == 1 ? 0 : denominator[1]);
		
		auto q = num / den;
		auto r = num % den;
		auto [q0, q1, q2] = split_into_blocks(q);
		auto [r0, r1, r2] = split_into_blocks(r);

		quotient[0] = q0;
		quotient[1] = q1;
		quotient[2] = q2;
		remainder[0] = r0;
		remainder[1] = r1;
		remainder[2] = r2;
		return true;
	}


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

	// 2. Large-integer methods
	// TODO: https://pure.mpg.de/rest/items/item_1819444_4/component/file_2599480/content
	// Could be implemented to reduce division time complexity.
		

	// 3. Bidirectional Exact Integer Division
	// https://core.ac.uk/download/pdf/82429412.pdf
	// INFO: This does not return remainder since exact means remainder is zero.
	// @return true If remainder is 0 otherwise false
	inline static constexpr auto exact_div(
		is_basic_integer auto const& numerator,
		is_basic_integer auto const& denominator,
		is_basic_integer auto& quotient
	) noexcept -> bool {
		using integer = std::decay_t<decltype(numerator)>;
		// N: a_0 * B^0 + a_1 * B^1 .... + a_m-1 * B^(m-1)
		// D: b_0 * B^0 + b_1 * B^1 .... + b_n-1 * B^(n-1)
		// Q: q_0 * B^0 + q_1 * B^1 .... + q_k-1 * B^(k-1), k <= m - n + 1

		auto const m = numerator.size();
		auto const n = denominator.size();

		assert(m > 2 && "expecting numerator to be greater than 2");
		assert(n > 2 && "expecting denominator to be greater than 2");

		// Cond 1: m >= n > 1
		if (m < n) return false;

		if (m == n && m == 1) {
			auto N = numerator[0];
			auto D = denominator[0];
			if (N % D != 0) return false;

			quotient[0] = N / D;
			return true;
		}

		// Cond 2: numerator >= denominator 
		if (numerator < denominator) return false;
	
		quotient.dyn_arr().resize(m - n + 1, 0);
		// High-order part of quotient
		auto const high_order = [&quotient](auto num, auto den, std::size_t h) -> bool {
			// 1 < h <= m - n +1, where h is length of high order part
			// Q: { q_0, q_1, ..., q_(m - n - h + 1)}
		
			auto m = num.size();
			auto n = den.size();
			if (m < n) return false;
			if (m == n && num[m - 1] < den[n - 1]) return false;

			// 1. Normalize: remove leading zeros;
			num.trim_leading_zeros();
			den.trim_leading_zeros();
			if (den.is_zero()) return false;
			if (num.is_zero()) return true;

			auto d = BlockInfo::block_total_bits - static_cast<std::size_t>(std::bit_width(den[n - 1]));
			auto I = std::max(0zu, m - n - h);

			//   num = num * 2^d
			num <<= d;
			//   den = den * 2^d
			den <<= d;
			
			m = num.size();
			n = den.size();

			// 2.Initialize loop
			auto const k_begin = I + 1;
			for (auto i = 3zu, k = k_begin; i < m && k < m - n; ++i, k++) {
				auto ir = m - i - 1;
				auto kr = m - n - k - 1;

				// 3.Calculate quotient digit
				if(num[ir] >= den[n - 1]) {
					quotient[kr] = BlockInfo::block_lower_mask;
				} else {
					auto q = combine_two_blocks(num[ir - 1], num[ir]) / den[n - 1];
					quotient[kr] = q & BlockInfo::block_lower_mask;

					// [a_(i-2), a_(i-1), a_i]
					auto t0 = num.to_borrowed_from_range(ir - 2, ir + 1);
					auto b0 = integer(den[n - 1]) * q;
					auto b1 = integer(den[n - 2]) * q;
					auto t1 = b0 + b1;
                    auto t2 = t0 - t1;
					if (t2.is_neg()) {
						quotient[kr] -= 1;
						// [b_(n-1), b_(n-2)]
						auto t3 = den.to_borrowed(n - 2, n);
                        t2 += t3;
                        num.replace_range(t2.dyn_arr(), ir - 2);
					}
                }

				// 4. Multiply and subtract
				auto J = static_cast<std::size_t>(std::max(0ll, static_cast<std::ptrdiff_t>(m - h) - 2ll - static_cast<std::ptrdiff_t>(k) ));

				
				// [a_i, ..., a_(k+J)]
				auto a = num.to_borrowed_from_range(kr + J, ir + 1);;
				// [b_(n-1), ..., b_J]
				auto b = den.to_borrowed_from_range(J, n);
				auto temp = b * quotient[kr];
				if (a < temp) {
					// 6. Add back
					quotient[kr] -= 1;
				} else {
                    auto t0 = a - b;
                    num.replace_range(t0.dyn_arr(), kr + J, ir + 1);
                    // 7. Remainder overflow?
                    if (num[ir] != 0) return false;
                    num.trim_leading_zeros();
                }
			}

            if (num[m - h] == 0 && num[m - h - 1] < m - h - 2) return false;

			// TODO: 10. Final remainder large small
            auto remainder = num.to_borrowed_from_range(m - h, m - n + 1 - h);
            if (remainder >= den) return false;
            return true;
		};
	
		auto const low_order = [&quotient](auto num, auto den, std::size_t l) -> bool {
			// 1 <= l <= m - n + 1
			auto m = num.size();
			auto n = den.size();

			// 1. Right-shift and trim trailing zeros
			num.trim_trailing_zeros();
			den.trim_trailing_zeros();
			if (den.is_zero()) return false;
			if (num.is_zero()) return true;

			auto d = BlockInfo::block_total_bits - static_cast<std::size_t>(std::bit_width(den[0]));
			auto L = std::min(n, l);

			if (n == 0) return false;
			if (m == 0) return true;
			if (m < n) return false;

			num >>= d;
			den >>= d;

			// 2. Compute modular inverse
			auto const b_inv = BinaryModularInv<BlockInfo::block_lower_mask>{}.inv(den[0]);

			// 3. Initialize loop
			for (auto k = 0zu; k < l; ++k) {
				if (num.is_zero()) break;

				// 4. Calculate quotient digit
				quotient[k] = (b_inv * num[k]) & BlockInfo::block_lower_mask;
				if (k == l - 1) break;

				auto const J = std::min(L, l - k);
				auto b0 = den.to_borrowed(0, J) * quotient[k];
				auto a0 = num.to_borrowed_from_range(k, l);
                auto t = a0 - b0;
                num.replace_range(t.dyn_arr(), k, l);
				num.trim_trailing_zeros();
			}

            return true;
		};
		
        std::size_t h = m;
    
        if (n > 3 && m < 3 * n - 6) {
            h = (m - n) >> 1;
        } else if (n != 1) {
            h = m - 2 * (n + 1);
        }

        auto const l = m - h;

        if (!high_order(numerator, denominator, h)) {
            return false;
        }

        if (l > 0 && !low_order(numerator, denominator, l)) {
            return false;
        }

        quotient.trim_leading_zeros();

        return true;
	} 

} // namespace dark::internal::integer

#endif // DARK_BIG_NUM_DIVISION_HPP
