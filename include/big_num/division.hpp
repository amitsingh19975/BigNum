#ifndef DARK_BIG_NUM_DIVISION_HPP
#define DARK_BIG_NUM_DIVISION_HPP

#include "error.hpp"
#include "type_traits.hpp"
#include <cassert>
#include <expected>

namespace dark::internal {

	// 1. Long division
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
