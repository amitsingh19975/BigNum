#ifndef DARK_BIG_NUM_BITWISE_HPP
#define DARK_BIG_NUM_BITWISE_HPP

#include "block_info.hpp"
#include <algorithm>

namespace dark::internal {

	inline static constexpr auto logical_left_shift(
		BlockInfo::type* out,
		std::size_t size,
		std::size_t shift
	) noexcept -> void {
		auto blocks_to_shift = shift / BlockInfo::total_bits;
		if (blocks_to_shift >= size) {
			std::fill_n(out, size, 0);
			return;
		}
		
		if (blocks_to_shift > 0) {
			// |xxxxx|xxxxx|xxxxx|-----|-----|
			for (auto i = size; i > blocks_to_shift; --i) {
				out[i - 1] = out[i - 1 - blocks_to_shift];
			}
			std::fill_n(out, blocks_to_shift, 0);
		}


		auto const remaining = shift % BlockInfo::total_bits;

		if (remaining == 0) return;
		auto last_bits = BlockInfo::accumulator_t{};	
		for (auto i = blocks_to_shift; i < size; ++i) {
			auto block = BlockInfo::accumulator_t{out[i]};
			block = (block << remaining) | last_bits;
			out[i] = block & BlockInfo::lower_mask;
			last_bits = (block >> BlockInfo::total_bits);
		}
	}

	inline static constexpr auto logical_right_shift(
		BlockInfo::type* out,
		std::size_t size,
		std::size_t shift
	) noexcept -> void {
		auto blocks_to_shift = shift / BlockInfo::total_bits;
		if (blocks_to_shift >= size) {
			std::fill_n(out, size, 0);
			return;
		}
		
		if (blocks_to_shift > 0) {
			// |----|----|xxxxx|xxxxx|xxxxx|
			for (auto i = blocks_to_shift; i < size; ++i) {
				out[i - blocks_to_shift] = out[i];
			}
			std::fill_n(out + size - blocks_to_shift, blocks_to_shift, 0);
		}


		auto const remaining = shift % BlockInfo::total_bits;

		if (remaining == 0) return;
		auto const mask = (BlockInfo::accumulator_t{1} << remaining) - 1;
		auto last_bits = BlockInfo::accumulator_t{};	
		for (auto i = size - blocks_to_shift; i > 0; --i) {
			auto const idx = i - 1;
			auto block = BlockInfo::accumulator_t{out[idx]};
			auto const bit_shift = BlockInfo::total_bits - remaining;
			auto last_bit_mask = (last_bits << bit_shift);
			out[idx] = ((block >> remaining) | last_bit_mask) & BlockInfo::lower_mask;
			last_bits = block & mask;
		}
	}
} // namespace dark::internal

#endif // DARK_BIG_NUM_BITWISE_HPP
