#ifndef DARK_BIG_NUM_ADD_SUB_HPP
#define DARK_BIG_NUM_ADD_SUB_HPP

#include "block_info.hpp"
#include <algorithm>
#include <print>
#include <span>

namespace dark::internal {
	namespace integer {
		inline static constexpr auto safe_add_helper(
			BlockInfo::accumulator_t lhs,
			BlockInfo::accumulator_t rhs
		) noexcept -> std::pair<BlockInfo::type, BlockInfo::type> {
			auto acc = lhs + rhs;
			auto res = acc & BlockInfo::block_lower_mask;
			auto carry = acc >> BlockInfo::block_total_bits;
			return {res, carry};
		}

		inline static constexpr auto safe_add_helper(
			std::span<BlockInfo::type> out,
			BlockInfo::accumulator_t val
		) noexcept -> BlockInfo::type {
			auto carry = val;
			for (auto index = 0zu; index < out.size() && carry; ++index) {
				auto res = safe_add_helper(out[index], carry);
				out[index] = res.first;
				carry = res.second;
			}
			return carry & BlockInfo::block_lower_mask;
		}

        template <std::size_t BlockSize = 32>
        inline static constexpr auto safe_add_helper(
			std::span<BlockInfo::type> out,
			std::span<BlockInfo::type> const a,
			std::span<BlockInfo::type> const b
		) noexcept -> BlockInfo::type {
            auto const size = std::min(a.size(), b.size());
            assert(out.size() >= size);

            BlockInfo::type carry{};

            for (auto i = 0zu; i < size; i += BlockSize) {
                auto const ib = std::min(BlockSize, size - i);

                BlockInfo::type cs[2][BlockSize + 1] = {};
                BlockInfo::type temp[BlockSize] = {};

                cs[0][0] = carry;

                for (auto j = 0zu; j < ib; ++j) {
                    auto sum = a[i + j] + b[i + j] + cs[0][j];
                    temp[j] = sum & BlockInfo::block_lower_mask;
                    cs[1][j + 1] = sum >> BlockInfo::block_total_bits;
                }

                carry = cs[1][ib];

                for (auto j = 1zu; j < ib; ++j) {
                    auto const idx = j & 1;
                    auto const next_idx = idx ^ 1;
                    cs[idx][0] = 0;
                    for (auto k = 0zu; k < ib; ++k) {
                        auto sum = temp[k] + cs[idx][k];
                        temp[k] = sum & BlockInfo::block_lower_mask;
                        cs[next_idx][k + 1] = sum >> BlockInfo::block_total_bits;
                    }
                    carry += cs[next_idx][ib];
                }

                for (auto k = 0zu; k < ib; ++k) {
                    out[i + k] = temp[k];
                }
            }

            auto s = std::span(out.data() + size, out.size() - size);
            return safe_add_helper(s, carry);
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
			auto const new_lhs = lhs + borrow1 * BlockInfo::block_max_value - prev_borrow;
			auto const borrow2 = new_lhs < rhs;
			auto const acc = new_lhs + borrow2 * BlockInfo::block_max_value - rhs;
			auto const res = acc & BlockInfo::block_lower_mask;
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
