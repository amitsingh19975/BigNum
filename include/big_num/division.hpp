#ifndef DARK_BIG_NUM_DIVISION_HPP
#define DARK_BIG_NUM_DIVISION_HPP

#include "big_num/block_info.hpp"
#include "big_num/error.hpp"
#include "type_traits.hpp"
#include <cassert>
#include <cmath>
#include <expected>
#include <ostream>
#include <type_traits>

namespace dark::internal {

	// 1. Restoring division
	// NOTE: Ensure quotient has enough space
	inline static constexpr auto restoring_div(
		is_signed_basic_integer auto const& numerator,
		is_signed_basic_integer auto const& denominator,
		is_signed_basic_integer auto& quotient,
		is_signed_basic_integer auto& remainder
	) noexcept -> std::expected<void, BigNumError> {
		if (denominator.is_zero()) return std::unexpected(BigNumError::DivideByZero);
		using type = std::decay_t<std::remove_cvref_t<decltype(remainder)>>;
		assert(quotient.bits() >= numerator.bits() && "quotient should have enough space");
		auto n = quotient.size() * BlockInfo::total_bits;
		remainder = numerator;
		auto d_res = denominator.shift_left(n, true);
		if (!d_res) return std::unexpected(d_res.error());
		auto& d = *d_res;
		auto two = type(2);
		
		for (auto i = n; i > 0; --i) {
			auto const idx = i - 1;
			{
				auto res = 	remainder.mul(two);
				if (!res) return std::unexpected(res.error());
				remainder = std::move(*res);
			}
			{
				auto res = remainder.sub_mut(d);
				if (!res) return std::unexpected(res.error());
			}

			if (remainder.is_neg()) {	
				quotient.set_bit(idx, 0);
				{
					auto res = remainder.add_mut(d);
					if (!res) return std::unexpected(res.error());
				}
			} else {
				quotient.set_bit(idx, 1);
			}
		}
		return {};
	}

	// 1.5. Long division
	inline static constexpr auto long_div(
		is_basic_integer auto const& numerator,
		is_basic_integer auto const& denominator,
		is_basic_integer auto& quotient,
		is_basic_integer auto& remainder
	) noexcept -> std::expected<void, BigNumError> {
		auto n = numerator.bits();
		assert(quotient.bits() >= n && "quotient should have enough space");
		assert(remainder.bits() >= n && "remainder should have enough space");
		if (denominator.is_zero()) return std::unexpected(BigNumError::DivideByZero);

		for (auto i = n; i > 0; --i) {
			auto const idx = i - 1;
			{
				auto res = remainder.shift_left_mut(1, true);
				if (!res) return std::unexpected(res.error());
			}
			
			remainder.set_bit(0, numerator.get_bit(idx), true);
			if (remainder >= denominator) {
				{
					auto res = remainder.sub_mut(denominator);
					if (!res) return std::unexpected(res.error());
				}
				quotient.set_bit(idx, 1);
			}
		}	
		return {};
	}
	// 2. Newton–Raphson division
	// 3. Large-integer methods
		

} // namespace dark::internal

#endif // DARK_BIG_NUM_DIVISION_HPP
