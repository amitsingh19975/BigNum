#ifndef DARK_BIG_NUM_ADD_SUB_HPP
#define DARK_BIG_NUM_ADD_SUB_HPP

#include "block_info.hpp"
#include <span>

namespace dark::internal {

	namespace integer {
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
			std::span<BlockInfo::type> out,
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
			std::span<BlockInfo::type> out,
			std::span<BlockInfo::type> const a
		) noexcept -> BlockInfo::accumulator_t {
			auto carry = BlockInfo::accumulator_t{};
			auto const size = std::min(out.size(), a.size());
			for (auto i = 0zu; i < size; ++i) {
				auto [acc, c] = safe_add_helper(out[i], a[i] + carry);
				out[i] = acc;
				carry = c;
			}
			auto t = std::span{out.data() + size, out.size() - size};
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
			std::span<BlockInfo::type> out,
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
			std::span<BlockInfo::type> out,
			std::span<BlockInfo::type> const a,
			BlockInfo::accumulator_t val
		) noexcept -> BlockInfo::accumulator_t {
			auto borrow = val;
			auto const sz = std::min(out.size(), a.size());
			for (auto index = 0zu; index < sz && borrow; ++index) {
				auto res = safe_sub_helper(a[index], borrow);
				out[index] = res.first;
				borrow = res.second;
			}
			return borrow;
		}
		
		inline static constexpr auto safe_sub_helper(
			std::span<BlockInfo::type> out,
			std::span<BlockInfo::type> const a
		) noexcept -> BlockInfo::accumulator_t {
			auto borrow = BlockInfo::accumulator_t{};
			auto const size = std::min(out.size(), a.size());
			for (auto i = 0zu; i < size; ++i) {
				auto [acc, c] = safe_sub_helper(out[i], a[i], borrow);
				out[i] = acc;
				borrow = c;
			}

			auto t = std::span{out.data() + size, out.size() - size};
			return safe_sub_helper(t, borrow);
		}

		inline static constexpr auto safe_sub_helper(
			std::span<BlockInfo::type> out,
			std::span<BlockInfo::type> const a,
			std::span<BlockInfo::type> const b
		) noexcept -> BlockInfo::accumulator_t {
			auto borrow = BlockInfo::accumulator_t{};
			auto const size = std::min(out.size(), a.size());
			for (auto i = 0zu; i < size; ++i) {
				auto [acc, c] = safe_sub_helper(a[i], b[i], borrow);
				out[i] = acc;
				borrow = c;
			}

			auto t = std::span{out.data() + size, out.size() - size};
			auto at = std::span{a.data() + size, a.size() - size};
			return safe_sub_helper(t, at, borrow);
		}

	} // namespace integner

} // namespace dark::internal

#endif // DARK_BIG_NUM_ADD_SUB_HPP
