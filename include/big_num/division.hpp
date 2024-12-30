#ifndef DARK_BIG_NUM_DIVISION_HPP
#define DARK_BIG_NUM_DIVISION_HPP

#include "add_sub.hpp"
#include "block_info.hpp"
#include "type_traits.hpp"
#include <bit>
#include <cassert>
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
		/*using integer = std::decay_t<decltype(numerator)>;*/
		// N: a_0 * B^0 + a_1 * B^1 .... + a_m-1 * B^(m-1)
		// D: b_0 * B^0 + b_1 * B^1 .... + b_n-1 * B^(n-1)
		// Q: q_0 * B^0 + q_1 * B^1 .... + q_k-1 * B^(k-1), k <= m - n + 1

		auto const m = numerator.size();
		auto const n = denominator.size();

        if (n == 0) return false;
        if (m == 0) return true;

		// Cond 1: m >= n > 1
		if (m < n) return false;

		if (m < 3 && n < 3) {
            auto N = combine_two_blocks(numerator[0], numerator.size() > 1 ? numerator[1] : 0);
            auto D = combine_two_blocks(denominator[0], denominator.size() > 1 ? denominator[1] : 0);
			if (N % D != 0) return false;

            auto res = N / D;
            auto [r0, r1, r2] = split_into_blocks(res);
            quotient.dyn_arr().resize(3);
            quotient[0] = r0;
            quotient[1] = r1;
            quotient[2] = r2;
            quotient.trim_leading_zeros();
			return true;
		}

		// Cond 2: numerator >= denominator 
		if (numerator.abs_less(denominator)) return false;
	
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
			if (den.is_zero()) return false;
			if (num.is_zero()) return true;

			auto d = BlockInfo::block_total_bits - static_cast<std::size_t>(std::bit_width(den[n - 1]));
			auto I = std::max(h, m - n) - h;

			//   num = num * 2^d
			num <<= d;
			//   den = den * 2^d
			den <<= d;

			m = num.size();
			n = den.size();

            auto const last_den = den[n - 1];

			// 2.Initialize loop
			for (auto k = m - n - 1, i = m - 1; k > I + 1; --k, --i) {
				// 3.Calculate quotient digit
				if(num[i] >= last_den) {
					quotient[k] = BlockInfo::block_lower_mask;
				} else {
					auto q = combine_two_blocks(num[i - 1], num[i]) / last_den;
					quotient[k] = q & BlockInfo::block_lower_mask;
                    auto qk = quotient[k];
                    
                    // [a_i, a_(i-1), a_(i-2)]
                    auto t0 = num.to_borrowed_from_range(i - 2, i + 1);

                    auto b = den.to_borrowed(n - 1, 1) * qk;
                    auto b2 = den.to_borrowed(n - 2, 1) * qk;
                    b.shift_by_base(1);

                    b += b2;
                    
                    auto is_neg = safe_sub_helper(t0.dyn_arr(), t0.dyn_arr(), b.dyn_arr());

                    if (is_neg) {
                        quotient[k] -= 1;
                        auto d_temp = den.to_borrowed_from_range(n - 2, n);
                        safe_add_helper(t0.dyn_arr(), t0.dyn_arr(), d_temp);
                    }
                }

				// 4. Multiply and subtract
                auto j0 = h + k + 2;
                auto J = std::max(j0, m) - j0;
				
				// [a_i, ..., a_(k+J)]
				auto a = num.to_borrowed_from_range(k + J, i + 1);
				// [b_(n-3), ..., b_J]
				auto b = den.to_borrowed_from_range(J, n - 3 + 1) * quotient[k];
                
                auto is_neg = safe_sub_helper(a.dyn_arr(), a.dyn_arr(), b.dyn_arr());

				if (is_neg) {
					// 6. Add back
					quotient[k] -= 1;
                    auto b1 = den.to_borrowed_from_range(J, n - 2);
                    safe_add_helper(a.dyn_arr(), a.dyn_arr(), b1.dyn_arr());
				} 

                // 7. Remainder overflow?
                if (num[i] != 0) return false;

			}

            if (num[m - h] == 0 && num[m - h - 1] < m - h - 2) return false;

			// 10. Final remainder large small
            auto remainder = num.to_borrowed_from_range(m - h, I + 1);
            if (remainder >= den) return false;

            return true;
		};
	
		auto const low_order = [&quotient](auto num, auto den, std::size_t l) -> bool {
			// 1 <= l <= m - n + 1
			auto m = num.size();
			auto n = den.size();

			// 1. Right-shift and trim trailing zeros
            auto const zs = den.trim_trailing_zeros();
            num.pop_front(zs);

			if (den.is_zero()) return false;
			if (num.is_zero()) return true;

            assert(std::countr_zero(den[0]) <= std::countr_zero(num[0]));

			if (m < n) return false;
			auto d = static_cast<std::size_t>(std::countr_zero(den[0]));
			auto L = std::min(n, l);

			num >>= d;
			den >>= d;

            auto N = num.to_borrowed(0, l);
            auto& D = den;

			// 2. Compute modular inverse
            auto const b_inv = BinaryModularInv<BlockInfo::block_lower_mask>{}(D[0]);

			// 3. Initialize loop
			for (auto k = 0zu; k < l; ++k) {
				// 4. Calculate quotient digit
				quotient[k] = (b_inv * N[k]) & BlockInfo::block_lower_mask;

                if (l == k + 1) {
                    break;
                }

                // 6. Multiply and Subtract
                auto J = std::min(L, l - k);

                auto a0 = N.to_borrowed(k);
				auto b0 = D.to_borrowed(0, J);
                /*if (k >= 52 && k < 54) {*/
                /*    std::println("Before b0[{}]: {} * {}", k, b0.dyn_arr(), quotient[k]);*/
                /*}*/

                b0 = b0 * quotient[k];


                /*if (k >= 52 && k < 54) {*/
                /*    std::println("a0: {}\n\nb0: {}", a0.dyn_arr(), b0.dyn_arr());*/
                /*}*/

                safe_sub_helper(a0.dyn_arr(), a0.dyn_arr(), b0.dyn_arr());

                /*if (k >= 52 && k < 54) {*/
                /*    std::println("\nres: {}\n\n", a0.dyn_arr());*/
                /*}*/
                assert(N[k] == 0);
			}

            return true;
		};

        if (n < 3) {
            if (!low_order(numerator, denominator, quotient.size())) {
                return false;
            }
            quotient.trim_leading_zeros();
            return true;
        }
		
        std::size_t h = m;
    
        if (m < 3 * n - 6) {
            h = quotient.size() >> 1;
        } else {
            h = quotient.size() - n + 2;
        }

        std::size_t l = quotient.size() + 1 - h;

        /*std::println("h: {}, l {}, q: {}, m: {}, n: {}", h, l, quotient.size(), m, n);*/

        if (h > 0 && !high_order(numerator, denominator, h)) {
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
