#ifndef DARK_BIG_NUM_BLOCK_INFO_HPP
#define DARK_BIG_NUM_BLOCK_INFO_HPP

#include "dyn_array.hpp"
#include <array>
#include <bit>
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
		static constexpr auto block_total_bytes = sizeof(type);
		static constexpr auto block_total_bits = block_total_bytes * 8 - 2;
		static constexpr auto block_max_value = accumulator_t{1} << block_total_bits;
		static constexpr auto block_lower_mask = block_max_value - 1;

		static constexpr auto total_acc_bytes = sizeof(accumulator_t);
		static constexpr auto total_acc_bits = total_acc_bytes * 8;

		static constexpr auto total_bits = block_total_bytes * 8;
		static constexpr auto max_value = accumulator_t{1} << total_bits;
		static constexpr auto lower_mask = max_value - 1;
		
		// For NTT
		static constexpr accumulator_t mod		= 71 * (accumulator_t{1} << 57) + 1;
		static constexpr accumulator_t generator = 3;
		
		static constexpr auto calculate_blocks_from_bytes(std::size_t bytes) noexcept {
			// INFO: we need ceil
			// (3 + 8 - 1) / 8 => (3 + 7) => 1
			return (bytes + block_total_bytes - 1) / block_total_bytes;
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
		static constexpr auto block_total_bytes = sizeof(type);
		static constexpr auto block_total_bits = block_total_bytes * 8 - 2;
		static constexpr auto block_max_value = accumulator_t{1} << block_total_bits;
		static constexpr auto block_lower_mask = block_max_value - 1;

		static constexpr auto total_acc_bytes = sizeof(accumulator_t);
		static constexpr auto total_acc_bits = total_acc_bytes * 8;
		static constexpr auto total_bits = block_total_bytes * 8;
		static constexpr auto max_value = accumulator_t{1} << total_bits;
		static constexpr auto lower_mask = max_value - 1;
		
		
		// For NTT
		static constexpr accumulator_t mod		= 43 * (accumulator_t{1} << 26) + 1;
		static constexpr accumulator_t generator = 3;

		static constexpr auto calculate_blocks_from_bytes(std::size_t bytes) noexcept {
			// INFO: we need ceil
			// (3 + 4 - 1) / 4 => (3 + 3) => 1
			return (bytes + block_total_bytes - 1) / block_total_bytes;
		} 
	};
#endif

	inline static constexpr auto combine_two_blocks(
		BlockInfo::accumulator_t a0,
		BlockInfo::accumulator_t a1
	) noexcept -> BlockInfo::accumulator_t {
		return BlockInfo::block_max_value * a1 + a0;
	}

	inline static constexpr auto split_into_blocks(
		BlockInfo::accumulator_t n
	) noexcept -> std::tuple<BlockInfo::type, BlockInfo::type, BlockInfo::type> {
		BlockInfo::type a0 = n % BlockInfo::block_max_value;
		auto temp = n / BlockInfo::block_max_value;
		BlockInfo::type a1 = temp % BlockInfo::block_max_value;

		return { a0, a1, static_cast<BlockInfo::type>(temp / BlockInfo::block_max_value) };
	}

	template <std::size_t M>
	struct BinaryModularInv {
		static_assert(M & (M - 1), "Mod should be a power of 2");
	private:
		static constexpr auto calculate_modular_inverse(BlockInfo::type n) noexcept -> BlockInfo::accumulator_t {
			BlockInfo::accumulator_t nr = 1;

			// INFO: Use Newton-Raphson iteration to find `nr` (modular multiplicative inverse)
			// total iterations required log2(number_of_bits) = log2(8) = 3	
			for (auto i = 0u; i < 3; ++i) {
				nr *= 2 - n * nr; 
				nr &= M;
			}
			return nr;
		}

		static constexpr auto lookup_table = []{
			std::array<std::uint8_t, 128> res;

			for (auto i = 0u; i < 128u; ++i) {
				auto inv = calculate_modular_inverse(2 * i + 1); // only odd numbers
				res[i] = inv & 0xff;
			}	

			return res;
		}();

	public:
		constexpr auto operator()(BlockInfo::type n) const noexcept -> BlockInfo::accumulator_t {
			assert((n & 1) && "number should be an odd");
			
			// 1. initial guess
			// We divide by two since inverses are stored in previous position since we
			// calculate inverses for only odd numbers.
			BlockInfo::accumulator_t inv = lookup_table[(n & 0xff) >> 1];

			// 2. Refine approximation using Newton iterations
			//    Each iteration doubles the number of correct bits
			inv = (1 - n * inv) * inv + inv;  // 16 bits
			inv = (1 - n * inv) * inv + inv;  // 32 bits
			if constexpr (sizeof(n) == 8) {
				inv = (1 - n * inv) * inv + inv;  // 32 bits
			}

			return inv;
		}

		constexpr auto inv(BlockInfo::type n) const noexcept -> BlockInfo::accumulator_t {
			assert(n != 0);
            if (n & 1) return this->operator()(n);

			auto n_z = std::countr_zero(n);
			constexpr auto mod_z = std::countr_zero(M);

			assert((n_z <= mod_z) && "Mod should be bigger than the 'n'");

			auto n_odd = n >> n_z;
			constexpr auto mod_odd = M >> mod_z;
			
			BlockInfo::accumulator_t inv = mod_odd == 1 ? 1 : this->operator()(n_odd);

			auto const power_correction = mod_z - n_z;
			return (inv << power_correction) & M;
		}	
	};

} // namespace dark::internal

#endif // DARK_BIG_NUM_BLOCK_INFO_HPP
