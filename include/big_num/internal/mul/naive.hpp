#ifndef AMT_BIG_NUM_INTERNAL_MUL_NAIVE_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_NAIVE_HPP

#include "../add_sub.hpp"
#include "../base.hpp"
#include "../logical_bitwise.hpp"
#include "ui.hpp"
#include <bit>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <type_traits>
#include <vector>

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
        num_t& out,
        const_num_t const& lhs,
        Integer::value_type rhs
    ) noexcept -> void {
        if (rhs == 0 || lhs.empty()) return;
        out.copy_sign(lhs);

        if (rhs & (rhs - 1)) {
            for (auto i = 0zu; i < lhs.size(); ++i) {
                auto l = lhs[i];
                auto r = rhs;

                auto [res, mc] = mul_impl<MachineConfig::bits>(l, r);
                auto [s, c] = abs_add(out[i], res); 
                out[i] = s;
                c += mc;

                auto k = i + 1;
                while (k < out.size() && c) {
                    auto [v, tc] = abs_add(out[k], c);
                    c = tc;
                    out[k++] = v;
                }
            }
        } else {
            if constexpr (!IsSameBuffer) {
                std::copy(lhs.begin(), lhs.end(), out.begin());
            }
            shift_left(out, static_cast<std::size_t>(std::bit_width(rhs)) - 1);
        }
    }

    template <Integer::value_type R, bool IsSameBuffer = false>
    inline static constexpr auto naive_mul(
        num_t& out,
        const_num_t const& lhs
    ) noexcept -> void {
        if constexpr (R == 0) return;
        else {
            if (lhs.empty()) return;
            out.copy_sign(lhs);

            if constexpr (R & (R - 1)) {
                for (auto i = 0zu; i < lhs.size(); ++i) {
                    auto l = lhs[i];
                    constexpr auto r = R;

                    auto [res, mc] = mul_impl<MachineConfig::bits>(l, r);
                    auto [s, c] = abs_add(out[i], res); 
                    out[i] = s;
                    c += mc;

                    auto k = i + 1;
                    while (k < out.size() && c) {
                        auto [v, tc] = abs_add(out[k], c);
                        c = tc;
                        out[k++] = v;
                    }
                }
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
        num_t& out
    ) noexcept -> void {
        naive_div<R, true>(out, out);
    }

    inline static constexpr auto naive_mul(
        num_t& out,
        const_num_t const& lhs,
        const_num_t const& rhs
    ) noexcept -> void {
        if (lhs.size() == 0 || rhs.size() == 0) return;

        using val_t = MachineConfig::uint_t;

        auto a = lhs.trim_trailing_zeros();
        auto b = rhs.trim_trailing_zeros();

        if (a.size() < 2) {
            naive_mul(out, b, a.data()[0]);
            return;
        } else if (b.size() < 2) {
            naive_mul(out, a, b.data()[0]);
            return;
        }

        out.set_neg(a.is_neg() ^ b.is_neg());

        for (auto i = 0zu; i < a.size(); ++i) {
            auto l = a[i];
            auto c = val_t{};

            for (auto j = 0zu; j < b.size(); ++j) {
                auto o = out[i + j];
                auto r = b[j];
                auto [m, mc] = mul_impl<MachineConfig::bits>(l, r);
                auto [v, tc] = abs_add(o, m, c);
                out[i + j] = v;
                c = tc + mc;
            }

            auto k = b.size() + i;
            while (k < out.size() && c) {
                auto [v, tc] = abs_add(out[k], c);
                c = tc;
                out[k++] = v;
            }
        }
    }

    // TODO: experiment with inplace square
    // inline static constexpr auto naive_square(
    //     std::span<Integer::value_type> out,
    //     std::span<Integer::value_type const> a
    // ) noexcept -> void {
    //     if (a.size() == 0) return;
    //
    //     // using val_t = MachineConfig::uint_t;
    //
    //     assert(out.size() >= a.size() * 2);
    //
    //     auto const helper = [out](std::size_t j, Integer::value_type c) {
    //         while (j < out.size() && c) {
    //             auto [s1, c1] = abs_add(out[j], c);
    //             out[j++] = s1;
    //             c = c1;
    //         }
    //     };
    //
    //     auto const n = a.size();
    //     // Squaring diagonals
    //     for (auto i = n; i > 0; --i) {
    //         auto ii = i - 1;
    //         auto [v, c] = mul_impl(a[ii], a[ii]);
    //         auto [s0, c0] = abs_add(out[ii << 1], v);
    //         c += c0;
    //         helper((ii << 1) + 1, c);
    //     }
    //
    //     // Cross terms (2*a[i]*a[j])
    //     for (auto i = 0zu; i < n; ++i) {
    //         for (auto j = i + 1; j < n; ++j) {
    //             auto ii = n - i - 1;
    //             auto jj = n - j - 1;
    //
    //             auto [v, c] = mul_impl(a[ii], a[jj] * 2);
    //             auto k = ii + jj;
    //             auto [s0, c0] = abs_add(out[k], v);
    //             c += c0;
    //             helper(k + 1, c);
    //         }
    //     }
    // }

    inline static constexpr auto naive_mul_scalar(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> void {
        out.resize(lhs.bits() + rhs.bits() + 1);
        out.set_neg(static_cast<bool>(lhs.is_neg() != rhs.is_neg()));
        auto o = out.to_span();
        if (lhs.size() < 2) {
            naive_mul(o, rhs.to_span(), lhs.data()[0]);
        } else {
            naive_mul(o, lhs.to_span(), rhs.data()[0]);
        }
        out.remove_trailing_empty_blocks();
    }

    template <Integer::value_type R>
    inline static constexpr auto naive_mul(
        Integer& out,
        Integer const& lhs
    ) -> void {
        out.resize(lhs.bits() + MachineConfig::bits);
        out.set_neg(lhs.is_neg());
        naive_mul<R>(out.to_span(), lhs.to_span());
        out.remove_trailing_empty_blocks();
    }

    inline static constexpr auto naive_mul(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> void {
        out.resize(lhs.bits() + rhs.bits() + MachineConfig::bits);
        auto o = out.to_span();
        naive_mul(o, lhs.to_span(), rhs.to_span());
        out.set_neg(o.is_neg());
        out.remove_trailing_empty_blocks();
    }

    inline static constexpr auto naive_square(
        Integer& out,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        auto size = out.size();
        auto a = std::pmr::vector<Integer::value_type>(
            out.data(),
            out.data() + static_cast<std::ptrdiff_t>(out.size()),
            resource
        );
        out.resize(size * 2 * MachineConfig::bits);
        out.fill(0);
        out.set_neg(false);
        auto l = std::span(a.data(), a.size());
        auto o = out.to_span();
        naive_mul(o, { l }, { l });
        out.remove_trailing_empty_blocks();
    }

} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_MUL_NAIVE_HPP
