#ifndef DARK_BIG_NUM_MUL_HPP
#define DARK_BIG_NUM_MUL_HPP

#include "block_info.hpp"
#include <algorithm>
#include <cassert>
#include <print>
#include <span>

namespace dark::internal::integer {
	
	inline static constexpr auto safe_add_helper(
		BlockInfo::accumulator_t lhs,
		BlockInfo::accumulator_t rhs
	) noexcept -> std::pair<BlockInfo::type, BlockInfo::type> {
		auto acc = lhs + rhs;
		auto res = acc & BlockInfo::lower_mask;
		auto carry = acc >> BlockInfo::total_bits;
		return {res, carry};
	}

	inline static constexpr auto safe_add_helper(
		BlockInfo::type* out,
		std::size_t index,
		std::size_t size,
		BlockInfo::accumulator_t val
	) noexcept -> BlockInfo::accumulator_t {
		auto carry = val;
		for (; index < size && carry; ++index) {
			auto res = safe_add_helper(out[index], carry);
			out[index] = res.first;
			carry = res.second;
		}
		return carry;
	}

	inline static constexpr auto safe_add_helper(
		BlockInfo::type* out,
		BlockInfo::type const* a,
		std::size_t size,
		std::size_t offset = 0
	) noexcept -> BlockInfo::accumulator_t {
		auto carry = BlockInfo::accumulator_t{};
		for (auto i = 0zu; i < size; ++i) {
			auto [acc, c] = safe_add_helper(out[i + offset], a[i] + carry);
			out[i + offset] = acc;
			carry = c;
		}
		return carry;
	}
	
	inline static constexpr auto safe_sub_helper(
		BlockInfo::accumulator_t lhs,
		BlockInfo::accumulator_t rhs,
		BlockInfo::accumulator_t prev_borrow = 0
	) noexcept -> std::pair<BlockInfo::type, BlockInfo::type> {
		// 00 * 10^0 + 00 * 10^1 + 00 * 10^2 + 01 * 10^3
		// 01 * 10^0
		// (10 + 0 - 1) * 10^0 + (10 + 0 - 1) * 10^1 + (10 + 0 - 1) * 10^2 + (1 - 1) * 10^3 
		auto const borrow1 = (lhs < prev_borrow);
		auto const new_lhs = lhs + borrow1 * BlockInfo::max_value - prev_borrow;
		auto const borrow2 = new_lhs < rhs;
		auto const acc = new_lhs + borrow2 * BlockInfo::max_value - rhs;
		auto const res = acc & BlockInfo::lower_mask;
		return {res, borrow1 | borrow2};
	}
	
	inline static constexpr auto safe_sub_helper(
		BlockInfo::type* out,
		std::size_t index,
		std::size_t size,
		BlockInfo::accumulator_t val
	) noexcept -> BlockInfo::accumulator_t {
		auto borrow = val;
		for (; index < size && borrow; ++index) {
			auto res = safe_sub_helper(out[index], borrow);
			out[index] = res.first;
			borrow = res.second;
		}
		return borrow;
	}
	
	inline static constexpr auto safe_sub_helper(
		BlockInfo::type* out,
		BlockInfo::type const* a,
		std::size_t size,
		std::size_t offset = 0
	) noexcept -> BlockInfo::accumulator_t {
		auto borrow = BlockInfo::accumulator_t{};
		for (auto i = 0zu; i < size; ++i) {
			auto [acc, c] = safe_sub_helper(out[i + offset], a[i], borrow);
			out[i + offset] = acc;
			borrow = c;
		}
		return borrow;
	}

	inline static constexpr auto naive_mul(
		BlockInfo::type* out,
		std::size_t out_size,
		BlockInfo::type const* lhs,
		std::size_t lhs_size,
		BlockInfo::type const* rhs,
		std::size_t rhs_size
	) noexcept -> void {
		assert(lhs_size != 0 && rhs_size != 0 && (out_size == lhs_size + rhs_size));
		assert(out != nullptr);

		if (lhs_size < rhs_size) {
			std::swap(lhs_size, rhs_size);
			std::swap(lhs, rhs);
		}
		
		for (auto i = 0zu; i < lhs_size; ++i) {
			BlockInfo::accumulator_t l = lhs[i];

			auto carry = BlockInfo::accumulator_t {};

			for (auto j = 0zu; j < rhs_size; ++j) {
				BlockInfo::accumulator_t r = rhs[j];

				auto [acc, c] = safe_add_helper(out[i + j], l * r + carry);

				out[i + j] = acc;
				carry = c;
			}
			
			safe_add_helper(out, rhs_size + i, out_size, carry);
		}
	}

	template <std::size_t MaxBuffLen, std::size_t NaiveThreshold = 32>
	inline static constexpr auto karatsuba_mul_helper(
		BlockInfo::type* out,
		BlockInfo::type const* lhs,
		BlockInfo::type const* rhs,
		std::size_t size,
		std::size_t depth = 0
	) noexcept -> void {
		using block_t = typename BlockInfo::type;
		using acc_t = typename BlockInfo::accumulator_t;

		if (size <= NaiveThreshold) {
			naive_mul(out, size << 1, lhs, size, rhs, size);
			return;
		}

		constexpr auto next_buff_len = MaxBuffLen >= 32 ? (MaxBuffLen >> 1) : MaxBuffLen;

		// lhs * rhs = z2 * (2^b)^2 + z1 * (2^b) + z0
		// z0 = x_l * y_l
		// z1 = z3 - z0 - z2
		// z2 = x_u * y_u
		// z3 = (x_l + x_u) * (y_l + y_u)
		
		auto const half = size >> 1;
		auto const low = half;
		auto const high = size - half;

		auto xl = lhs;
		auto xu = lhs + half;
		auto yl = rhs;
		auto yu = rhs + half;

		block_t z0[MaxBuffLen << 1] = {0};
		block_t z2[MaxBuffLen << 1] = {0};
		block_t z3[MaxBuffLen << 1] = {0};
		block_t x_sum[MaxBuffLen] = {0};
		block_t y_sum[MaxBuffLen] = {0};

		auto l_carry = acc_t{};
		auto r_carry = acc_t{};
		for (auto i = 0zu; i < half; ++i) {
			auto [x, xc] = safe_add_helper(xl[i], xu[i] + l_carry);
			auto [y, yc] = safe_add_helper(yl[i], yu[i] + r_carry);
			l_carry = xc;
			r_carry = yc;

			x_sum[i] = x;
			y_sum[i] = y;
		}
		
		auto const mid_size = high;
		for (auto i = half; i < mid_size; ++i) {
			auto [x, xc] = safe_add_helper(xu[i], l_carry);
			auto [y, yc] = safe_add_helper(yu[i], r_carry);
			l_carry = xc;
			r_carry = yc;

			x_sum[i] = x;
			y_sum[i] = y;
		}

		karatsuba_mul_helper<next_buff_len>(z0, xl, yl, low, depth + 1);
		karatsuba_mul_helper<next_buff_len>(z2, xu, yu, high, depth + 1);
		karatsuba_mul_helper<next_buff_len>(z3, x_sum, y_sum, mid_size, depth + 1);

		safe_add_helper(out + 0, z0, low * 2);
		safe_add_helper(out + half, z3, mid_size * 2);
		safe_add_helper(out + (half << 1), z2, high * 2);


		auto sz = (mid_size << 1) + 1;
		safe_add_helper(z0, z2, sz);

		auto b = acc_t{};
		for (auto i = 0zu; i < sz; ++i) {
			auto [v, bb] = safe_sub_helper(out[i + half], z0[i], b);
			out[i + half] = v;
			b = bb;
		}
	}

	template <std::size_t MaxLen, std::size_t NaiveThreshold = 32>
	inline static constexpr auto karatsuba_mul(
		BlockInfo::type* out,
		[[maybe_unused]] std::size_t out_size,
		BlockInfo::type const* lhs,
		std::size_t lhs_size,
		BlockInfo::type const* rhs,
		std::size_t rhs_size
	) noexcept -> void {
		assert(out_size <= 2 * MaxLen);
		BlockInfo::type buff_a[MaxLen] = {0};
		BlockInfo::type buff_b[MaxLen] = {0};

		std::copy_n(lhs, std::min(MaxLen, lhs_size), buff_a);
		std::copy_n(rhs, std::min(MaxLen, rhs_size), buff_b);

		auto size =	std::max(lhs_size, rhs_size);
		size += (size & 1);

		karatsuba_mul_helper<MaxLen, NaiveThreshold>(out, buff_a, buff_b, size);
	}
	

} // namespace dark::internal::integer

#endif // DARK_BIG_NUM_MUL_HPP
