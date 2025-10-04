#ifndef AMT_BIG_NUM_INTERNAL_MUL_NAIVE_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_NAIVE_HPP

#include "../add_sub.hpp"
#include "../base.hpp"
#include "../integer_parse.hpp"
#include "big_num/internal/div/naive.hpp"
#include "big_num/internal/logical_bitwise.hpp"
#include "ui.hpp"
#include <bit>
#include <span>
#include <type_traits>

namespace big_num::internal {
    template <bool IsSameBuffer = false>
    inline static constexpr auto naive_mul(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> lhs,
        Integer::value_type rhs
    ) noexcept -> void {
        if (rhs == 0 || lhs.empty()) return;

        if (rhs & (rhs - 1)) {
            using acc_t = MachineConfig::acc_t;
            auto r = acc_t{rhs};
            auto q = acc_t{lhs[0]} * r;
            auto c = q / MachineConfig::max;
            out[0] = q % MachineConfig::max;

            auto i = 1zu;
            for (; i < lhs.size(); ++i) {
                auto l = acc_t{lhs[i]};
                q = l + c;
                out[i] = q % MachineConfig::max;
                c = q / MachineConfig::max;
            }

            out[i] = static_cast<Integer::value_type>(c);
        } else {
            if constexpr (!IsSameBuffer) {
                std::copy(lhs.begin(), lhs.end(), out.begin());
            }
            shift_left(out, static_cast<std::size_t>(std::bit_width(rhs)) - 1);
        }
    }

    template <Integer::value_type R, bool IsSameBuffer = false>
    inline static constexpr auto naive_mul(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> lhs
    ) noexcept -> void {
        if constexpr (R == 0) return;
        else {
            if (lhs.empty()) return;
            if constexpr (R & (R - 1)) {
                using acc_t = MachineConfig::acc_t;
                auto q = acc_t{lhs[0]} * R;
                auto c = q / MachineConfig::max;
                out[0] = q % MachineConfig::max;
                auto i = 1zu;
                for (; i < lhs.size(); ++i) {
                    auto l = acc_t{lhs[i]};
                    q = l + c;
                    out[i] = q % MachineConfig::max;
                    c = q / MachineConfig::max;
                }

                out[i] = static_cast<Integer::value_type>(c);
            } else {
                if constexpr (!IsSameBuffer) {
                    std::copy(lhs.begin(), lhs.end(), out.begin());
                }
                shift_left(out, static_cast<std::size_t>(std::bit_width(R)) - 1);
            }
        }
    }

    template <Integer::value_type R>
    inline static constexpr auto naive_mul(
        std::span<Integer::value_type> out
    ) noexcept -> void {
        naive_div<R, true>(out, out);
    }

    inline static constexpr auto naive_mul(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) noexcept -> void {
        if (lhs.size() == 0 || rhs.size() == 0) return;

        using simd_t = MachineConfig::simd_uint_t;
        using sacc_t = MachineConfig::simd_acc_t;
        using acc_t = MachineConfig::acc_t;

        static constexpr auto N = simd_t::elements;
        auto mask = sacc_t::load(MachineConfig::mask);

        if (lhs.size() < 2) {
            naive_mul(out, rhs, lhs.data()[0]);
            return;
        } else if (rhs.size() < 2) {
            naive_mul(out, lhs, rhs.data()[0]);
            return;
        }

        if (!std::is_constant_evaluated()) {
            auto lsz = lhs.size();
            auto rsz = rhs.size() - rhs.size() % N;
            for (auto i = 0zu; i < lsz; ++i) {
                auto l = sacc_t::load(lhs[i]);
                auto carry = sacc_t::zeroed();
                auto j = std::size_t{};
                for (; j < rsz; j += N) {
                    auto o = ui::cast<acc_t>(simd_t::load(out.data() + i + j, N)) + carry;
                    auto r = ui::cast<acc_t>(simd_t::load(rhs.data() + j, N));
                    auto res = ui::mul_acc(o, l, r, ui::op::add_t{});
                    auto q = res & mask;
                    auto tc = ui::shift_right<MachineConfig::bits>(res);
                    carry[0] = 0;
                    for (auto k = 0zu; k < N; ++k) {
                        carry[0] += tc[N - 1];
                        tc = ui::shift_right_lane<1>(tc);
                        res = q + tc;
                        q = res & mask;
                        tc = ui::shift_right<MachineConfig::bits>(res);
                    }
                    std::copy_n(q.data(), N, out.data() + i + j);
                }

                auto c = carry[0];
                for (; j < rhs.size(); ++j) {
                    auto o = static_cast<acc_t>(out[i + j]) + c;
                    auto tl = lhs[i];
                    auto r = static_cast<acc_t>(rhs[j]);
                    auto t = o + tl * r;
                    out[i + j] = static_cast<MachineConfig::uint_t>(t & MachineConfig::mask);
                    c = t >> MachineConfig::bits;
                }

                auto k = j + i;
                auto ls = std::span(out.data() + k, out.size() - k);
                auto tc = static_cast<MachineConfig::uint_t>(c);
                auto rs = std::span(&tc, 1);
                abs_add(ls, rs);
            }
        } else {
            for (auto i = 0zu; i < lhs.size(); ++i) {
                auto l = static_cast<acc_t>(lhs[i]);
                auto scalar_carry = acc_t{};
                auto j = 0zu;

                for (; j < rhs.size(); ++j) {
                    auto o = static_cast<acc_t>(out[i + j]) + scalar_carry;
                    auto r = static_cast<acc_t>(rhs[j]);
                    auto res = o + l * r;
                    out[i + j] = static_cast<MachineConfig::uint_t>(res & MachineConfig::mask);
                    scalar_carry = res >> MachineConfig::bits;
                }

                auto k = j + i;
                auto ls = std::span(out.data() + k, out.size() - k);
                auto c = static_cast<MachineConfig::uint_t>(scalar_carry);
                auto rs = std::span(&c, 1);
                abs_add(ls, rs);
            }
        }
    }

    inline static constexpr auto naive_mul_scalar(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> void {
        out.resize(lhs.size() + rhs.size());
        out.set_neg(static_cast<bool>(lhs.is_neg() ^ rhs.is_neg()));
        if (lhs.size() < 2) {
            naive_mul(out.to_span(), rhs.to_span(), lhs.data()[0]);
        } else {
            naive_mul(out.to_span(), lhs.to_span(), rhs.data()[0]);
        }
        remove_trailing_zeros(out);
    }

    template <Integer::value_type R>
    inline static constexpr auto naive_mul(
        Integer& out,
        Integer const& lhs
    ) -> void {
        out.resize(lhs.size() + MachineConfig::bits);
        out.set_neg(lhs.is_neg());
        naive_mul<R>(out.to_span(), lhs.to_span());
        remove_trailing_zeros(out);
    }

    inline static constexpr auto naive_mul(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> void {
        out.resize(lhs.bits() + rhs.bits() + MachineConfig::bits);
        out.set_neg(static_cast<bool>(lhs.is_neg() ^ rhs.is_neg()));
        naive_mul(out.to_span(), lhs.to_span(), rhs.to_span());
        remove_trailing_zeros(out);
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_MUL_NAIVE_HPP
