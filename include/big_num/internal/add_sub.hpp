#ifndef AMT_BIG_NUM_INTERNAL_ADD_SUB_HPP
#define AMT_BIG_NUM_INTERNAL_ADD_SUB_HPP

#include "base.hpp"
#include "cmp.hpp"
#include "integer_parse.hpp"
#include "integer.hpp"
#include "ui.hpp"
#include <type_traits>

namespace big_num::internal {
    template <std::size_t Bits = MachineConfig::bits, std::integral T>
        requires std::is_unsigned_v<T>
    inline static constexpr auto abs_add(
        T lhs,
        T rhs
    ) noexcept -> std::pair<T /*val*/, T /*carry*/> {
        if constexpr (Bits == sizeof(T) * 8) {
            return ui::addc(lhs, rhs);
        } else {
            using acc_t = accumulator_t<T>;
            static constexpr auto mask = (acc_t{1} << Bits) - 1;
            auto res = acc_t{lhs} + acc_t{rhs};
            return {
                static_cast<T>(res & mask),
                static_cast<T>(res >> Bits)
            };
        }
    }

    template <std::size_t Bits = MachineConfig::bits, std::integral T>
        requires std::is_unsigned_v<T>
    inline static constexpr auto abs_add(
        T lhs,
        T rhs,
        T carry
    ) noexcept -> std::pair<T /*val*/, T /*carry*/> {
        if constexpr (Bits == sizeof(T) * 8) {
            return ui::addc(lhs, rhs, carry);
        } else {
            using acc_t = accumulator_t<T>;
            static constexpr auto mask = (acc_t{1} << Bits) - 1;
            auto res = acc_t{lhs} + acc_t{rhs} + acc_t{carry};
            return {
                static_cast<T>(res & mask),
                static_cast<T>(res >> Bits)
            };
        }
    }

    template <std::size_t Bits = MachineConfig::bits, std::integral T>
        requires std::is_unsigned_v<T>
    inline static constexpr auto abs_sub(
        T lhs,
        T rhs
    ) noexcept -> std::pair<T /*val*/, T /*carry*/> {
        if constexpr (Bits == sizeof(T) * 8) {
            return ui::subc(lhs, rhs);
        } else {
            using acc_t = accumulator_t<T>;
            static constexpr auto mask = (acc_t{1} << Bits) - 1;
            auto res = acc_t{lhs} - acc_t{rhs};
            return {
                static_cast<T>(res & mask),
                static_cast<T>(lhs < rhs)
            };
        }
    }

    template <std::size_t Bits = MachineConfig::bits, std::integral T>
        requires std::is_unsigned_v<T>
    inline static constexpr auto abs_sub(
        T lhs,
        T rhs,
        T carry
    ) noexcept -> std::pair<T /*val*/, T /*carry*/> {
        if constexpr (Bits == sizeof(T) * 8) {
            return ui::subc(lhs, rhs, carry);
        } else {
            using acc_t = accumulator_t<T>;
            static constexpr auto mask = (acc_t{1} << Bits) - 1;

            auto r = acc_t{rhs} + acc_t{carry};
            auto res = acc_t{lhs} - r;
            return {
                static_cast<T>(res & mask),
                static_cast<T>(lhs < rhs)
            };
        }
    }

    inline static constexpr auto abs_add(
        std::span<Integer::value_type>       lhs,
        std::span<Integer::value_type const> rhs
    ) -> void {
        if (lhs.empty()) return;

        auto o = lhs.data();
        auto b = rhs.data();
        auto osz = lhs.size();
        auto bsz = std::min(rhs.size(), osz);

        using val_t = Integer::value_type;
        using simd_t = MachineConfig::simd_uint_t;
        static constexpr auto N = simd_t::elements;

        auto carry = val_t{};

        auto i = std::size_t{};
        if constexpr (MachineConfig::bits < sizeof(val_t) * 8) {
            if (!std::is_constant_evaluated()) {
                auto c = simd_t::zeroed();
                auto nsz = bsz - bsz % N;
                for (; i < nsz; i += N) {
                    auto l = simd_t::load(o + i, N);
                    auto r = simd_t::load(b + i, N);
                    auto [q, tc] = ui::masked_addc<MachineConfig::bits>(l, r, c);
                    std::copy_n(q.data(), N, o + i);
                    c[0] = tc;
                }
                carry = static_cast<val_t>(c[0]);
            }
        }

        for (; i < bsz; ++i) {
            auto l = o[i];
            auto r = b[i];
            auto [v, c] = abs_add(l, r, carry);
            o[i] = v;
            carry = c;
        }

        auto sz = osz;
        while (i < sz && carry) {
            auto l = o[i];
            auto [v, c] = abs_add(l, carry);
            o[i] = v;
            carry = c;
        }
    }

    inline static constexpr auto abs_add(
        std::span<Integer::value_type>       out,
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) -> void {
        auto a = lhs;
        auto b = rhs;
        auto o = out;
        if (a.size() >= b.size()) {
            std::copy(a.begin(), a.end(), o.begin());
            abs_add(o, b);
        } else {
            std::copy(b.begin(), b.end(), o.begin());
            abs_add(o, a);
        }
    }

    inline static constexpr auto abs_add(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> void {
        auto const bits = std::max(lhs.bits(), rhs.bits());
        out.resize(bits + 1);

        auto o = out.to_span();
        auto a = lhs.to_span();
        auto b = rhs.to_span();

        if (a.size() >= b.size()) {
            out.set_neg(lhs.is_neg());
            std::copy(a.begin(), a.end(), o.begin());
            abs_add(o, b);
        } else {
            out.set_neg(rhs.is_neg());
            std::copy(b.begin(), b.end(), o.begin());
            abs_add(o, a);
        }
        remove_trailing_zeros(out);
    }

    inline static constexpr auto abs_sub(
        std::span<Integer::value_type>       lhs,
        std::span<Integer::value_type const> rhs
    ) -> bool {
        assert(lhs.size() >= rhs.size());

        auto o = lhs.data();
        auto b = rhs.data();
        auto osz = lhs.size();
        auto bsz = rhs.size();

        using val_t = Integer::value_type;
        using simd_t = MachineConfig::simd_uint_t;
        static constexpr auto N = simd_t::elements;

        auto carry = val_t{};

        auto i = std::size_t{};
        if constexpr (MachineConfig::bits < sizeof(val_t) * 8) {
            if (!std::is_constant_evaluated()) {
                auto c = simd_t::zeroed();
                auto nsz = bsz - bsz % N;
                for (; i < nsz; i += N) {
                    auto l = simd_t::load(o + i, N);
                    auto r = simd_t::load(b + i, N);
                    auto [q, tc] = ui::masked_subc<MachineConfig::bits>(l, r, c);
                    std::copy_n(q.data(), N, o + i);
                    c[0] = tc;
                }
                carry = static_cast<val_t>(c[0]);
            }
        }

        for (; i < bsz; ++i) {
            auto [v, c] = abs_sub(o[i], b[i], carry);
            o[i] = v;
            carry = c;
        }

        auto sz = osz;
        while (carry && i < sz) {
            auto [v, c] = abs_sub(o[i], carry);
            o[i++] = v;
            carry = c;
        }
        return carry;
    }

    template <bool Swap = true>
    inline static constexpr auto abs_sub(
        std::span<Integer::value_type>       out,
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) -> void {
        auto a = lhs;
        auto b = rhs;
        auto o = out;

        if constexpr (Swap) {
            // a < b
            // TODO: use size to compare and merge the less logic into the loop to avoid calling
            // `abs_less`.
            if (abs_less(lhs, rhs)) {
                std::copy(b.begin(), b.end(), o.begin());
                abs_sub(o, a);
            } else {
                std::copy(a.begin(), a.end(), o.begin());
                abs_sub(o, b);
            }
        } else {
            // lhs >= rhs
            std::copy(b.begin(), b.end(), o.begin());
            abs_sub(o, a);
        }
    }

    template <bool Swap = true>
    inline static constexpr auto abs_sub(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> void {
        auto const bits = std::max(lhs.bits(), rhs.bits());
        out.resize(bits + 1);

        auto a = lhs.to_span();
        auto b = rhs.to_span();
        auto o = out.to_span();

        if constexpr (Swap) {
            // a < b
            // TODO: use size to compare and merge the less logic into the loop to avoid calling
            // `abs_less`.
            if (abs_less(lhs, rhs)) {
                out.set_neg(!rhs.is_neg());
                std::copy(b.begin(), b.end(), o.begin());
                abs_sub(o, a);
            } else {
                out.set_neg(lhs.is_neg());
                std::copy(a.begin(), a.end(), o.begin());
                abs_sub(o, b);
            }
        } else {
            // lhs >= rhs
            out.set_neg(lhs.is_neg());
            std::copy(b.begin(), b.end(), o.begin());
            abs_sub(o, a);
        }

        remove_trailing_zeros(out);
    }

    inline static constexpr auto add(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> void {
        if (lhs.is_neg() == rhs.is_neg()) {
            abs_add(out, lhs, rhs);
        } else {
            // |-lhs| < |-rhs|
            if (abs_less(lhs, rhs)) {
                abs_sub<false>(out, rhs, lhs);
            } else {
                abs_sub<false>(out, lhs, rhs);
            }
        }
    }

    inline static constexpr auto sub(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> void {
        if (lhs.is_neg() == rhs.is_neg()) {
            abs_sub(out, lhs, rhs);
        } else {
            abs_add(out, lhs, rhs);
        }
    }

    inline static constexpr auto inc(
        std::span<Integer::value_type> lhs,
        Integer::value_type val = 1
    ) -> Integer::value_type {
        Integer::value_type c{val};
        auto i = 0zu;
        while (i < lhs.size() && c){
            auto r = lhs[i] + c;
            auto q = r & MachineConfig::mask;
            c = r >> MachineConfig::bits;
            lhs[i++] = static_cast<Integer::value_type>(q);
        }
        return c;
    }

    inline static constexpr auto dec(
        std::span<Integer::value_type> lhs,
        Integer::value_type val = 1
    ) -> Integer::value_type {
        Integer::value_type c{val};
        auto i = 0zu;
        while (i < lhs.size() && c){
            auto r = lhs[i] - c;
            c = lhs[i] < c;
            auto q = r & MachineConfig::mask;
            lhs[i++] = static_cast<Integer::value_type>(q);
        }
        return c;
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_ADD_SUB_HPP
