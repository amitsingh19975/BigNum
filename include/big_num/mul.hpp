#ifndef DARK_BIG_NUM_MUL_HPP
#define DARK_BIG_NUM_MUL_HPP

#include "big_num/allocator.hpp"
#include "dyn_array.hpp"
#include "block_info.hpp"
#include <algorithm>
#include <cassert>

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
		BlockInfo::blocks_t& out,
		BlockInfo::accumulator_t val
	) noexcept -> BlockInfo::accumulator_t {
		auto carry = val;
		for (auto index = 0zu; index < out.size() && carry; ++index) {
			auto res = safe_add_helper(out[index], carry);
			out[index] = res.first;
			carry = res.second;
		}
		return carry;
	}

	inline static constexpr auto safe_add_helper(
		BlockInfo::blocks_t& out,
		BlockInfo::blocks_t const& a
	) noexcept -> BlockInfo::accumulator_t {
		auto carry = BlockInfo::accumulator_t{};
		auto const size = std::min(out.size(), a.size());
		for (auto i = 0zu; i < size; ++i) {
			auto [acc, c] = safe_add_helper(out[i], a[i] + carry);
			out[i] = acc;
			carry = c;
		}
		auto t = out.to_borrowed(size);
		return safe_add_helper(t, carry);
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
		BlockInfo::blocks_t& out,
		BlockInfo::accumulator_t val
	) noexcept -> BlockInfo::accumulator_t {
		auto borrow = val;
		for (auto index = 0zu; index < out.size() && borrow; ++index) {
			auto res = safe_sub_helper(out[index], borrow);
			out[index] = res.first;
			borrow = res.second;
		}
		return borrow;
	}
	
	inline static constexpr auto safe_sub_helper(
		BlockInfo::blocks_t& out,
		BlockInfo::blocks_t const& a
	) noexcept -> BlockInfo::accumulator_t {
		auto borrow = BlockInfo::accumulator_t{};
		auto const size = std::min(out.size(), a.size());
		for (auto i = 0zu; i < size; ++i) {
			auto [acc, c] = safe_sub_helper(out[i], a[i], borrow);
			out[i] = acc;
			borrow = c;
		}

		auto t = out.to_borrowed(size);
		return safe_sub_helper(t, borrow);
	}

	inline static constexpr auto naive_mul(
		BlockInfo::blocks_t& out,
		BlockInfo::blocks_t const& lhs,
		BlockInfo::blocks_t const& rhs
	) noexcept -> void {
		using acc_t = BlockInfo::accumulator_t;
		out.resize(lhs.size() + rhs.size());

		auto const lhs_size = lhs.size();
		auto const rhs_size = rhs.size();

		for (auto i = 0zu; i < lhs_size; ++i) {
			auto l = acc_t{lhs[i]};

			auto carry = acc_t {};

			for (auto j = 0zu; j < rhs_size; ++j) {
				auto r = acc_t{rhs[j]};

				auto [acc, c] = safe_add_helper(out[i + j], l * r + carry);

				out[i + j] = acc;
				carry = c;
			}
			
			auto temp = out.to_borrowed(rhs_size + i);
			safe_add_helper(temp, carry);
		}
	}

	template <std::size_t NaiveThreshold = 32>
	inline static constexpr auto karatsuba_mul_helper(
		BlockInfo::blocks_t& out,
		BlockInfo::blocks_t const& lhs,
		BlockInfo::blocks_t const& rhs,
		std::size_t size,
		std::size_t depth = 0
	) noexcept -> void {
		using block_t = typename BlockInfo::type;
		using acc_t = typename BlockInfo::accumulator_t;

		if (size <= NaiveThreshold) {
			auto old_size = out.size();
			naive_mul(out, lhs, rhs);
			out.resize(old_size, 0);
			return;
		}

		// lhs * rhs = z2 * (2^b)^2 + z1 * (2^b) + z0
		// z0 = x_l * y_l
		// z1 = z3 - z0 - z2
		// z2 = x_u * y_u
		// z3 = (x_l + x_u) * (y_l + y_u)
		
		auto const half = size >> 1;
		auto const low = half;
		auto const high = size - half;

		auto xl = lhs.to_borrowed(0, half);
		auto xu = lhs.to_borrowed(half);
		auto yl = rhs.to_borrowed(0, half);
		auto yu = rhs.to_borrowed(half);

		assert(!xl.is_owned());
		assert(!xu.is_owned());
		assert(!yl.is_owned());
		assert(!yu.is_owned());

		auto x_sum = DynArray<block_t>(high * 2 + /* carry */ 1, 0);
		auto y_sum = DynArray<block_t>(high * 2 + /* carry */ 1, 0);
		auto const sz = (high << 1) + 1;
		auto z0 = DynArray<block_t>(sz, 0);
		auto z2 = DynArray<block_t>(sz, 0);
		auto z3 = DynArray<block_t>(sz, 0);

		auto l_carry = acc_t{};
		auto r_carry = acc_t{};

		{
			auto i = 0zu;
			for (; i < half; ++i) {
				auto [x, xc] = safe_add_helper(xl[i], xu[i] + l_carry);
				auto [y, yc] = safe_add_helper(yl[i], yu[i] + r_carry);
				l_carry = xc;
				r_carry = yc;

				x_sum[i] = x;
				y_sum[i] = y;
			}
		

			for (; i < high; ++i) {
				auto [x, xc] = safe_add_helper(xu[i], l_carry);
				auto [y, yc] = safe_add_helper(yu[i], r_carry);

				x_sum[i] = x;
				l_carry = xc;

				y_sum[i] = y;
				r_carry = yc;
			}
		}
		
		auto const mid_size = high + static_cast<bool>(l_carry | r_carry);
		if (mid_size > high) {
			x_sum[high] = l_carry & BlockInfo::lower_mask;
			y_sum[high] = r_carry & BlockInfo::lower_mask;
		}

		/*std::println("{}: xl: {}\nxu: {}\nyl: {}\nyu: {}\nxs: {}\nys: {}\n", depth, xl, xu, yl, yu, x_sum, y_sum);*/

		karatsuba_mul_helper<NaiveThreshold>(z0, xl, yl, low, depth + 1);
		karatsuba_mul_helper<NaiveThreshold>(z2, xu, yu, high, depth + 1);
		karatsuba_mul_helper<NaiveThreshold>(z3, x_sum, y_sum, mid_size, depth + 1);

		auto o1 = out.to_borrowed(0, low << 1);
		auto o2 = out.to_borrowed(half, mid_size << 1);
		auto o3 = out.to_borrowed(half << 1, high << 1);
		safe_add_helper(o1, z0);
		safe_add_helper(o2, z3);
		safe_add_helper(o3, z2);

		/*std::println("\n========{}==========", depth);*/
		/*std::println("z0: {}\nz2: {}\nz3: {}\n", z0, z2, z3);*/
		auto z0_t = z0.to_borrowed(0, sz);
		auto z2_t = z2.to_borrowed(0, sz);
		safe_add_helper(z0_t, z2_t);


		auto b = acc_t{};
		for (auto i = 0zu; i < sz; ++i) {
			auto [v, bb] = safe_sub_helper(out[i + half], z0[i], b);
			out[i + half] = v;
			b = bb;
		}
	}

	template <std::size_t NaiveThreshold = 32>
	inline static constexpr auto karatsuba_mul(
		BlockInfo::blocks_t& out,
		BlockInfo::blocks_t const& lhs,
		BlockInfo::blocks_t const& rhs
	) noexcept -> void {
		auto temp_size = std::max(lhs.size(), rhs.size());
		out.resize(temp_size << 1, 0);

		utils::TempAllocatorScope scope;
		
		auto buff_a = DynArray<BlockInfo::type>(temp_size + 1, 0);
		auto buff_b = DynArray<BlockInfo::type>(temp_size + 1, 0);

		std::copy(lhs.begin(), lhs.end(), buff_a.begin());
		std::copy(rhs.begin(), rhs.end(), buff_b.begin());

		auto size =	std::max(lhs.size(), rhs.size());
		size += (size & 1);

		karatsuba_mul_helper<NaiveThreshold>(
			out,
			buff_a,
			buff_b,
			size
		);
	}
	

} // namespace dark::internal::integer

#endif // DARK_BIG_NUM_MUL_HPP
