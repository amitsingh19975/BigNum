#ifndef DARK_BIG_NUM_BITWISE_HPP
#define DARK_BIG_NUM_BITWISE_HPP

#include "block_info.hpp"
#include <algorithm>
#include <cstddef>

namespace dark::internal {

	inline static constexpr auto logical_left_shift(
		BlockInfo::blocks_t& out,
		std::size_t shift
	) noexcept -> void {
		auto const size = out.size();
		auto blocks_to_shift = shift / BlockInfo::block_total_bits;
		if (blocks_to_shift >= size) {
			out.resize(0);
			return;
		}
		
		if (blocks_to_shift > 0) {
			// |xxxxx|xxxxx|xxxxx|-----|-----|
			std::move(out.rbegin() + static_cast<std::ptrdiff_t>(blocks_to_shift), out.rend(), out.rbegin());
			std::fill_n(out.data(), blocks_to_shift, 0);
		}


		auto const remaining = shift % BlockInfo::block_total_bits;

		if (remaining == 0) return;
		auto last_bits = BlockInfo::accumulator_t{};
		for (auto i = blocks_to_shift; i < size; ++i) {
			auto block = BlockInfo::accumulator_t{out[i]};
			block = (block << remaining) | last_bits;
			out[i] = block & BlockInfo::block_lower_mask;
			last_bits = (block >> BlockInfo::block_total_bits);
		}
	}


	 // a >> b
	inline static constexpr auto logical_right_shift(
		BlockInfo::blocks_t& out,
		std::size_t shift
	) noexcept -> void {
		auto size = out.size();
		auto blocks_to_shift = shift / BlockInfo::block_total_bits;
		if (blocks_to_shift >= size) {
			out.resize(0);
			return;
		}
		
		if (blocks_to_shift > 0) {
			// |----|----|xxxxx|xxxxx|xxxxx|
			std::move(out.begin() + blocks_to_shift, out.end(), out.begin());
			out.resize(out.size() - blocks_to_shift);
			size = out.size();
		}


		auto const remaining = shift % BlockInfo::block_total_bits;

		if (remaining == 0) return;
		auto const mask = (BlockInfo::accumulator_t{1} << remaining) - 1;
		auto last_bits = BlockInfo::accumulator_t{};	
		for (auto i = 0zu; i < size; ++i) {
			auto const idx = size - i - 1;
			auto block = BlockInfo::accumulator_t{out[idx]};
			auto const bit_shift = BlockInfo::block_total_bits - remaining;
			auto last_bit_mask = (last_bits << bit_shift);
			out[idx] = ((block >> remaining) | last_bit_mask) & BlockInfo::block_lower_mask;
			last_bits = block & mask;
		}
	}
} // namespace dark::internal

#endif // DARK_BIG_NUM_BITWISE_HPP
