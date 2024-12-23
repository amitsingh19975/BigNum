#ifndef DARK_BIG_NUM_CONVERTER_HPP
#define DARK_BIG_NUM_CONVERTER_HPP

#include "basic.hpp"
#include "operators.hpp"
#include "allocator.hpp"
#include "utils.hpp"
#include "type_traits.hpp"
#include <type_traits>

namespace dark::internal {

	template <typename I, std::size_t Naive>
	inline static constexpr auto dc_base_convert(
		is_basic_integer auto& out,
		std::string_view num,
		std::size_t from_base,
		std::size_t depth = 0
	) -> void {
		auto const size = num.size();

		if (size <= Naive) {
			out.dyn_arr().resize(size, 0);
			utils::convert_to_block_radix(out.data(), num, from_base);
			out.trim_zero();
			return;
		}

		auto const mid = size >> 1;

		auto lhs = num.substr(0, mid);
		auto rhs = num.substr(mid);
		
		auto ln = I{};
		auto rn = I{};
		
		dc_base_convert<I, Naive>(ln, lhs, from_base, depth + 1);
		dc_base_convert<I, Naive>(rn, rhs, from_base, depth + 1);

		auto base = I(from_base);
		base.pow_mut(rhs.size());
		
		out = ln * base + rn;
	}
	
	template <std::size_t Naive = 5, std::size_t DC = 100'000>
	inline static constexpr auto base_convert(
		is_basic_integer auto& out,
		std::string_view num,
		Radix from_radix
	) -> void {
		using integer_t = std::decay_t<decltype(out)>;
		auto const from_base = static_cast<std::size_t>(from_radix);

		utils::TempAllocatorScope scope;

		auto tout = integer_t{};
		dc_base_convert<integer_t, Naive>(tout, num, from_base);
	
		out.dyn_arr().clone_from(tout.dyn_arr());
		out.trim_zero();
	}

} // dark::internal

#endif // DARK_BIG_NUM_CONVERTER_HPP
