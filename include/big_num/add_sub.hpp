#ifndef DARK_BIG_NUM_ADD_SUB_HPP
#define DARK_BIG_NUM_ADD_SUB_HPP

#include "block_info.hpp"
#include <algorithm>
#include <cstddef>
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

                BlockInfo::type cs[BlockSize + 1][BlockSize + 1] = {};
                BlockInfo::type temp[BlockSize] = {};

                cs[0][0] = carry;

                for (auto j = 0zu; j < ib; ++j) {
                    auto sum = a[i + j] + b[i + j] + cs[0][j];
                    temp[j] = sum & BlockInfo::block_lower_mask;
                    cs[1][j + 1] = sum >> BlockInfo::block_total_bits;
                }

                for (auto j = 1zu; j < ib; ++j) {
                    for (auto k = 0zu; k < ib; ++k) {
                        auto sum = temp[k] + cs[j][k];
                        temp[k] = sum & BlockInfo::block_lower_mask;
                        cs[j + 1][k + 1] = sum >> BlockInfo::block_total_bits;
                    }
                }

                carry = 0;

                for (auto j = 0zu; j < BlockSize; ++j) {
                    carry += cs[j][ib];
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
		) noexcept -> BlockInfo::type {
			auto borrow = val;
			for (auto index = 0zu; index < out.size() && borrow; ++index) {
				auto res = safe_sub_helper(out[index], borrow);
				out[index] = res.first;
				borrow = res.second;
			}
			return borrow & BlockInfo::block_lower_mask;
		}

        template <std::size_t BlockSize = 32>
		inline static constexpr auto safe_sub_helper(
			std::span<BlockInfo::type> out,
			std::span<BlockInfo::type> const a,
			std::span<BlockInfo::type> const b
		) noexcept -> BlockInfo::type {
			auto const size = std::min(a.size(), b.size());
            assert(out.size() >= size);
			auto borrow = BlockInfo::type{};
            
            constexpr auto max_value = BlockInfo::block_max_value & BlockInfo::block_lower_mask;

            constexpr auto sub_helper = [](BlockInfo::type lhs, BlockInfo::type rhs) noexcept -> std::pair<BlockInfo::type, BlockInfo::type> {
                auto b = lhs < rhs;
                return { (lhs + (max_value * b - rhs)) & BlockInfo::block_lower_mask, b };
            };


	        for (auto i = 0zu; i < size; i += BlockSize) {
                auto const ib = std::min(BlockSize, size - i);

                BlockInfo::type bs[BlockSize + 1][BlockSize + 1] = {};
                BlockInfo::type temp[BlockSize] = {};
                bs[0][0] = borrow;

                for (auto j = 0zu; j < ib; ++j) {
                    auto [v0, b0] = sub_helper(a[i + j], bs[0][j]);
                    auto [v1, b1] = sub_helper(v0, b[i + j]);
                    temp[j] = v1;
                    bs[1][j + 1] = b0 | b1;
                }


                for (auto j = 1zu; j < ib; ++j) {
                    for (auto k = 0zu; k < ib; ++k) {
                        auto [v0, b0] = sub_helper(temp[k], bs[j][k]);
                        bs[j + 1][k + 1] = b0;
                        temp[k] = v0;
                    }
                }

                borrow = 0;
                for (auto j = 0zu; j < BlockSize; ++j) {
                    borrow += bs[j][ib];
                }

                for (auto j = 0zu; j < ib; ++j) {
                    out[i + j] = temp[j];
                }
            }

			auto t = std::span{out.data() + size, out.size() - size};
			return safe_sub_helper(t, borrow);
		}
	} // namespace integner

} // namespace dark::internal

#endif // DARK_BIG_NUM_ADD_SUB_HPP
