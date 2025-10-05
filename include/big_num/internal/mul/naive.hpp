#ifndef AMT_BIG_NUM_INTERNAL_MUL_NAIVE_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_NAIVE_HPP

#include "../add_sub.hpp"
#include "../base.hpp"
#include "../integer_parse.hpp"
#include "../logical_bitwise.hpp"
#include "ui.hpp"
#include <bit>
#include <span>
#include <type_traits>

namespace big_num::internal {
    namespace detail {
        template <std::integral T>
        inline static constexpr auto full_width_naive_mul(
            T lhs,
            T rhs
        ) noexcept -> std::pair<T /*lo*/, T /*hi*/> {
            using val_t = T;
            #ifdef UI_HAS_INT128
            if constexpr (sizeof(val_t) == 8) {
                auto l = ui::uint128_t(lhs);
                auto r = ui::uint128_t(rhs);
                auto res = l * r;
                return {
                    static_cast<val_t>(res),
                    static_cast<val_t>(res >> (sizeof(val_t) * 8))
                };
            }
            #endif
            // (A_hi*x + A_lo) * (B_hi*x + B_lo)
            // = A_hi * B_hi * x^2 + (A_lo * B_hi + A_hi * B_lo) * x + A_lo * B_lo 
            static constexpr auto half = (sizeof(val_t) * 8) >> 1;
            static constexpr auto mask = (val_t{1} << half) - 1;

            auto const alo = lhs & mask;
            auto const ahi = (lhs >> half);

            auto const blo = rhs & mask;
            auto const bhi = (rhs >> half);

            auto chi0 = ahi * bhi;
            auto clo0 = alo * blo;

            auto cmid0 = alo * bhi;
            auto cmid1 = ahi * blo;

            auto [cmid, carry] = ui::addc(cmid0, cmid1);
            carry = (carry << half) | (cmid >> half);
            cmid <<= half;

            auto [clo, carry1] = ui::addc(clo0, cmid);
            carry += carry1;

            auto chi = chi0 + carry;
            return { clo, chi };
        }
    } // namespace detail

    template <std::size_t Bits = MachineConfig::bits, std::integral T>
        requires std::is_unsigned_v<T>
    inline static constexpr auto mul_impl(
        T lhs,
        T rhs
    ) noexcept -> std::pair<T /*lo*/, T /*hi*/> {
        if constexpr (Bits == sizeof(T) * 8) {
            return detail::full_width_naive_mul(lhs, rhs);
        } else {
            static constexpr auto mask = (T{1} << Bits) - 1;
            if constexpr (sizeof(T) == 8) {
                auto [lo, hi] = detail::full_width_naive_mul(lhs, rhs);
                return {
                    lo & mask,
                    hi + (lo >> Bits)
                };
            } else {
                using acc_t = accumulator_t<T>;
                auto l = acc_t{lhs};
                auto r = acc_t{rhs};
                auto res = l * r;
                return {
                    static_cast<T>(res & mask),
                    static_cast<T>(res >> Bits)
                };
            }
        }
    }

    template <bool IsSameBuffer = false>
    inline static constexpr auto naive_mul(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> lhs,
        Integer::value_type rhs
    ) noexcept -> void {
        if (rhs == 0 || lhs.empty()) return;

        if (rhs & (rhs - 1)) {
            auto [res, c] = mul_impl<MachineConfig::bits>(lhs[0], rhs);
            out[0] = res;

            auto i = 1zu;
            for (; i < lhs.size(); ++i) {
                auto l = lhs[i];
                auto [v, tc] = abs_add(l, c);
                out[i] = v;
                c = tc;
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
                auto [res, c] = mul_impl<MachineConfig::bits>(lhs[0], R);
                out[0] = res;

                auto i = 1zu;
                for (; i < lhs.size(); ++i) {
                    auto l = lhs[i];
                    auto [v, tc] = abs_add(l, c);
                    out[i] = v;
                    c = tc;
                }

                out[i] = static_cast<Integer::value_type>(c);
            } else {
                if constexpr (!IsSameBuffer) {
                    std::copy(lhs.begin(), lhs.end(), out.begin());
                }
                shift_left<static_cast<std::size_t>(std::bit_width(R)) - 1>(out);
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

        using val_t = MachineConfig::uint_t;

        if (lhs.size() < 2) {
            naive_mul(out, rhs, lhs.data()[0]);
            return;
        } else if (rhs.size() < 2) {
            naive_mul(out, lhs, rhs.data()[0]);
            return;
        }

        for (auto i = 0zu; i < lhs.size(); ++i) {
            auto l = lhs[i];
            auto c = val_t{};
            auto j = 0zu;

            for (; j < rhs.size(); ++j) {
                auto o = out[i + j];
                auto r = rhs[j];
                auto [m, mc] = mul_impl<MachineConfig::bits>(l, r);
                auto [v, tc] = abs_add(o, m, c);
                out[i + j] = v;
                c = tc + mc;
            }

            auto k = j + i;
            auto ls = std::span(out.data() + k, out.size() - k);
            auto rs = std::span(&c, 1);
            abs_add(ls, rs);
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
