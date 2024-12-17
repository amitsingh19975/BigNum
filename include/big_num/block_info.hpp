#ifndef DARK_BIG_NUM_BLOCK_INFO_HPP
#define DARK_BIG_NUM_BLOCK_INFO_HPP

#include "big_num/dyn_array.hpp"
#include <cstddef>
#include <cstdint>

namespace dark::internal {

constexpr auto neareast_power_of_2(std::size_t num) noexcept -> std::size_t {
	if ((num & (num - 1)) == 0) return num;
	--num;
	num |= (num >> 1);	
	num |= (num >> 2);	
	num |= (num >> 4);	
	num |= (num >> 8);	
	num |= (num >> 16);	
	num |= (num >> 32);	
	++num;
	return num;
}

#if defined(UINT128_MAX) || defined(__SIZEOF_INT128__)
	struct BlockInfo {
		using type = std::uint64_t;
		using accumulator_t = __uint128_t;
		using blocks_t = DynArray<type>;
		static constexpr auto total_bytes = sizeof(type);
		static constexpr auto total_bits = total_bytes * 8 - 2;
		static constexpr auto max_value = accumulator_t{1} << total_bits;
		static constexpr auto lower_mask = max_value - 1;
		static constexpr auto total_acc_bytes = sizeof(accumulator_t);
		static constexpr auto total_acc_bits = total_acc_bytes * 8;
		static constexpr auto ntt_total_bits = total_bytes * 8;
		static constexpr auto ntt_max_value = accumulator_t{1} << ntt_total_bits;
		static constexpr auto ntt_lower_mask = ntt_max_value - 1;
		
		// For NTT
		static constexpr accumulator_t mod		= 71 * (accumulator_t{1} << 57) + 1;
		static constexpr accumulator_t generator = 3;
		
		static constexpr auto calculate_blocks_from_bytes(std::size_t bytes) noexcept {
			// INFO: we need ceil
			// (3 + 8 - 1) / 8 => (3 + 7) => 1
			return (bytes + total_bytes - 1) / total_bytes;
		}
		/**/
		/*template <std::size_t N>*/
		/*static constexpr auto get_mod() const noexcept -> accumulator_t {}*/
	};
#else
	struct BlockInfo {
		using type = std::uint32_t;
		using accumulator_t = std::uint64_t;  
		using blocks_t = DynArray<type>;
		static constexpr auto total_bytes = sizeof(type);
		static constexpr auto total_bits = total_bytes * 8 - 2;
		static constexpr auto max_value = accumulator_t{1} << total_bits;
		static constexpr auto lower_mask = max_value - 1;
		static constexpr auto total_acc_bytes = sizeof(accumulator_t);
		static constexpr auto total_acc_bits = total_acc_bytes * 8;
		static constexpr auto ntt_total_bits = total_bytes * 8;
		static constexpr auto ntt_max_value = accumulator_t{1} << ntt_total_bits;
		static constexpr auto ntt_lower_mask = ntt_max_value - 1;
		
		// For NTT
		static constexpr accumulator_t mod		= 43 * (accumulator_t{1} << 26) + 1;
		static constexpr accumulator_t generator = 3;

		static constexpr auto calculate_blocks_from_bytes(std::size_t bytes) noexcept {
			// INFO: we need ceil
			// (3 + 4 - 1) / 4 => (3 + 3) => 1
			return (bytes + total_bytes - 1) / total_bytes;
		} 
	};
#endif

} // namespace dark::internal

#endif // DARK_BIG_NUM_BLOCK_INFO_HPP
