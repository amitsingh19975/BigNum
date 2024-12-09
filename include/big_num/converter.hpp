#ifndef DARK_BIG_NUM_CONVERTER_HPP
#define DARK_BIG_NUM_CONVERTER_HPP

#include "basic.hpp"
#include "utils.hpp"
#include "type_traits.hpp"
#include <type_traits>
#include "format.hpp"

namespace dark::internal {

	template <typename I>	
	inline static constexpr auto naive_base_convert(
		is_basic_integer auto& out,
		std::string_view num,
		std::size_t from_base
	) -> void {
		out.resize(num.size(), 0);
		utils::convert_to_block_radix(out.data(), num, from_base);
		out.trim_zero();
	}

	
	template <typename I, std::size_t Naive>
	inline static constexpr auto dc_base_convert(
		is_basic_integer auto& out,
		std::string_view num,
		std::size_t from_base
	) -> void {
		auto const size = num.size();

		if (size <= Naive) {
			naive_base_convert<I>(out, num, from_base);
			return;
		}

		auto const mid = size >> 1;

		auto lhs = num.substr(0, mid);
		auto rhs = num.substr(mid);
		
		auto ln = I{};
		auto rn = I{};
		
		dc_base_convert<I, Naive>(ln, lhs, from_base);
		dc_base_convert<I, Naive>(rn, rhs, from_base);

		auto base = I(from_base);
		base.pow_mut(rhs.size());
		ln.mul_mut(base);
		ln.add_mut(rn);
		out = std::move(ln);	
	}
	
	template <std::size_t Naive = 5, std::size_t DC = 100'000>
	inline static constexpr auto base_convert(
		is_basic_integer auto& out,
		std::string_view num,
		Radix from_radix
	) -> void {
		using integer_t = std::decay_t<std::remove_cvref_t<decltype(out)>>;
		auto const size = num.size();
		auto const from_base = static_cast<std::size_t>(from_radix);

		if (size <= Naive) {
			naive_base_convert<integer_t>(out, num, from_base);
		} else if (size <= DC) {
			dc_base_convert<integer_t, Naive>(out, num, from_base);
		}
	}

} // dark::internal

#endif // DARK_BIG_NUM_CONVERTER_HPP
