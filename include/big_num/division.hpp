#ifndef DARK_BIG_NUM_DIVISION_HPP
#define DARK_BIG_NUM_DIVISION_HPP

#include "type_traits.hpp"
#include <cassert>

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

	// 2. Large-integer methods
	// TODO: https://pure.mpg.de/rest/items/item_1819444_4/component/file_2599480/content
	// Could be implemented to reduce division time complexity.
		

} // namespace dark::internal::integer

#endif // DARK_BIG_NUM_DIVISION_HPP
