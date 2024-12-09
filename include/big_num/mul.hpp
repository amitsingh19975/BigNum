#ifndef DARK_BIG_NUM_MUL_HPP
#define DARK_BIG_NUM_MUL_HPP

#include "block_info.hpp"
#include <algorithm>
#include <cassert>
#include <span>

namespace dark::internal::integer {
	
	inline constexpr auto safe_add_helper(
		BlockInfo::accumulator_t lhs,
		BlockInfo::accumulator_t rhs
	) noexcept -> std::pair<BlockInfo::type, BlockInfo::type> {
		auto acc = lhs + rhs;
		auto res = acc & BlockInfo::lower_mask;
		auto carry = acc >> BlockInfo::total_bits;
		return {res, carry};
	}

	inline constexpr auto safe_add_helper(
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

	inline constexpr auto safe_add_helper(
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
	
	inline constexpr auto safe_sub_helper(
		BlockInfo::accumulator_t lhs,
		BlockInfo::accumulator_t rhs
	) noexcept -> std::pair<BlockInfo::type, BlockInfo::type> {
		auto acc = lhs - rhs;
		auto res = acc & BlockInfo::lower_mask;
		auto borrow = (acc >> (BlockInfo::total_acc_bits - 1)) & 1;
		return {res, borrow};
	}
	
	inline constexpr auto safe_sub_helper(
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
	
	inline constexpr auto safe_sub_helper(
		BlockInfo::type* out,
		BlockInfo::type const* a,
		std::size_t size,
		std::size_t offset = 0
	) noexcept -> BlockInfo::accumulator_t {
		auto borrow = BlockInfo::accumulator_t{};
		for (auto i = 0zu; i < size; ++i) {
			auto [acc, c] = safe_sub_helper(out[i + offset], a[i] + borrow);
			out[i + offset] = acc;
			borrow = c;
		}
		return borrow;
	}

	constexpr auto naive_mul(
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

				auto mul = l * r;
				auto first_add = safe_add_helper(mul, carry);
				auto [acc, c] = safe_add_helper(out[i + j], first_add.first);

				out[i + j] = acc;
				carry = (c + first_add.second) & BlockInfo::lower_mask;
			}
			
			safe_add_helper(out, rhs_size + i, out_size, carry);
		}
	}

	template <std::size_t MaxBuffLen, std::size_t NaiveThreshold = 32>
	constexpr auto karatsuba_mul_helper(
		BlockInfo::type* out,
		BlockInfo::type const* lhs,
		BlockInfo::type const* rhs,
		std::size_t size
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
		
		karatsuba_mul_helper<next_buff_len>(z0, xl, yl, low);
		karatsuba_mul_helper<next_buff_len>(z2, xu, yu, high);
		karatsuba_mul_helper<next_buff_len>(z3, x_sum, y_sum, mid_size);
	
		safe_add_helper(out + 0, z0, low * 2);
		safe_add_helper(out + half, z3, mid_size * 2);
		safe_add_helper(out + size, z2, high * 2);

		/*std::println("z0: {}", std::span(z0, half << 1));*/
		/*std::println("z2: {}", std::span(z2, high << 1));*/
		/*std::println("z3: {}\n\n", std::span(z3, mid_size << 1));*/

		auto borrow = acc_t{};
		for (auto i = 0zu; i < mid_size * 2; ++i) {
			auto sub = acc_t{z0[i]} + acc_t{z2[i]} + borrow;
			auto [acc, b] = safe_sub_helper(out[i + half], sub);
			out[i + half] = acc;
			borrow = b;
		}

		out[mid_size << 1] -= borrow;
		
	}

	template <std::size_t MaxLen, std::size_t NaiveThreshold = 32>
	constexpr auto karatsuba_mul(
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

		/*std::println("lhs: {},\nrhs: {}\n\n", std::span(buff_a, lhs_size), std::span(buff_b, rhs_size));*/

		auto size =	std::max(lhs_size, rhs_size);
		size += (size & 1);

		karatsuba_mul_helper<MaxLen, NaiveThreshold>(out, buff_a, buff_b, size);
	}
	

} // namespace dark::internal::integer

#endif // DARK_BIG_NUM_MUL_HPP
