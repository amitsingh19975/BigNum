#ifndef AMT_BIG_NUM_INTERNAL_ADD_SUB_HPP
#define AMT_BIG_NUM_INTERNAL_ADD_SUB_HPP

#include "base.hpp"
#include "logical_bitwise.hpp"
#include "integer.hpp"
#include "ui.hpp"
#include <cstddef>
#include <type_traits>
#include <utility>

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
                static_cast<T>(lhs < r)
            };
        }
    }

    inline static constexpr auto abs_add(
        num_t       lhs,
        const_num_t const& rhs
    ) noexcept -> Integer::value_type {
        if (lhs.empty()) return {};
        auto tmp_r = rhs.trim_trailing_zeros();

        auto o = lhs.data();
        auto b = rhs.data();
        auto osz = lhs.size();
        auto bsz = std::min(tmp_r.size(), osz);

        using val_t = Integer::value_type;

        auto carry = val_t{};

        auto i = std::size_t{};
        if constexpr (MachineConfig::bits < sizeof(val_t) * 8) {
            if (!std::is_constant_evaluated()) {
                using simd_t = MachineConfig::simd_uint_t;
                static constexpr auto N = simd_t::elements;
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

        if (bsz < tmp_r.size()) carry = rhs[bsz - 1];

        auto sz = osz;
        while (i < sz && carry) {
            auto l = o[i];
            auto [v, c] = abs_add(l, carry);
            o[i++] = v;
            carry = c;
        }

        return carry;
    }

    inline static constexpr auto abs_add(
        num_t lhs,
        Integer::value_type rhs
    ) noexcept -> Integer::value_type {
        if (lhs.empty()) return {};
        auto c = rhs;
        auto i = 0zu;
        while (i < lhs.size() && c) {
            auto [v, tc] = abs_add(lhs[i], c);
            lhs[i++] = tc;
            c = tc;
        }

        return c;
    }

    inline static constexpr auto abs_add(
        num_t       out,
        const_num_t const& lhs,
        const_num_t const& rhs
    ) noexcept -> Integer::value_type {
        auto a = lhs;
        auto b = rhs;
        auto o = out;
        if (a.size() >= b.size()) {
            std::copy(a.begin(), a.end(), o.begin());
            return abs_add(o, b);
        } else {
            std::copy(b.begin(), b.end(), o.begin());
            return abs_add(o, a);
        }
    }

    inline static constexpr auto abs_add(
        num_t       out,
        const_num_t const&  lhs,
        Integer::value_type rhs
    ) noexcept -> Integer::value_type {
        auto a = lhs;
        auto b = rhs;
        auto o = out;

        std::copy(a.begin(), a.end(), o.begin());
        return abs_add(o, b);
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

        std::copy(a.begin(), a.end(), o.begin());
        abs_add(o, b);

        out.remove_trailing_empty_blocks();
    }

    template <bool Take2sComplement = false>
    inline static constexpr auto abs_sub(
        num_t       lhs,
        const_num_t const& rhs
    ) noexcept -> Integer::value_type {
        auto tmp_r = rhs.trim_trailing_zeros();

        auto o = lhs.data();
        auto b = rhs.data();
        auto osz = lhs.size();
        auto bsz = std::min(tmp_r.size(), osz);

        using val_t = Integer::value_type;

        auto carry = val_t{};

        auto i = std::size_t{};
        if constexpr (MachineConfig::bits < sizeof(val_t) * 8) {
            if (!std::is_constant_evaluated()) {
                using simd_t = MachineConfig::simd_uint_t;
                static constexpr auto N = simd_t::elements;
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

        if (bsz < tmp_r.size()) carry = rhs[bsz - 1];

        auto sz = osz;
        while (carry && i < sz) {
            auto [v, c] = abs_sub(o[i], carry);
            o[i++] = v;
            carry = c;
        }

        if constexpr (Take2sComplement) {
            if (carry) {
                twos_complement({ lhs.data(), i });
            }
        }
        return carry;
    }

    inline static constexpr auto abs_sub(
        num_t lhs,
        Integer::value_type rhs
    ) noexcept -> Integer::value_type {
        if (lhs.empty()) return {};
        auto c = rhs;
        auto i = 0zu;
        while (i < lhs.size() && c) {
            auto [v, tc] = abs_sub(lhs[i], c);
            lhs[i++] = tc;
            c = tc;
        }

        return c;
    }

    template <bool Take2sComplement = false>
    inline static constexpr auto abs_sub(
        num_t       out,
        const_num_t const& lhs,
        const_num_t const& rhs
    ) noexcept -> Integer::value_type {
        if (lhs.data() != out.data()) {
            std::copy(lhs.begin(), lhs.end(), out.begin());
        }
        return abs_sub<Take2sComplement>(out, rhs);
    }

    template <bool Take2sComplement = false>
    inline static constexpr auto abs_sub(
        num_t       out,
        const_num_t const&  lhs,
        Integer::value_type rhs
    ) noexcept -> Integer::value_type {
        if (lhs.data() != out.data()) {
            std::copy(lhs.begin(), lhs.end(), out.begin());
        }
        return abs_sub<Take2sComplement>(out, rhs);
    }

    template <bool Take2sComplement = false>
    inline static constexpr auto abs_sub(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> Integer::value_type {
        auto const bits = std::max(lhs.bits(), rhs.bits());
        out.resize(bits);

        auto a = lhs.to_span();
        auto b = rhs.to_span();
        auto o = out.to_span();

        // lhs <= rhs
        if (a.data() != o.data()) {
            std::copy(a.begin(), a.end(), o.begin());
        }
        auto c = abs_sub<Take2sComplement>(o, b);

        out.remove_trailing_empty_blocks();
        return c;
    }

    inline static constexpr auto add(
        num_t& out,
        const_num_t const& lhs,
        const_num_t const& rhs
    ) noexcept -> num_t::value_type {
        auto const ls = lhs.is_neg();
        auto const rs = rhs.is_neg();
        // both are -ve or +ve
        if (ls == rs) {
            auto c = abs_add(out, lhs, rhs);
            out.copy_sign(lhs);
            return c;
        } else if (ls && !rs) {
            // out = -lhs + rhs => rhs - lhs
            auto c = abs_sub<true>(out, rhs, lhs);
            out.set_neg(c);
            return c;
        } else {
            // out = lhs - rhs
            auto c = abs_sub<true>(out, lhs, rhs);
            out.set_neg(c);
            return c;
        }
    }

    inline static constexpr auto sub(
        num_t& out,
        const_num_t const& lhs,
        const_num_t const& rhs
    ) noexcept -> num_t::value_type;

    inline static constexpr auto sub(
        num_t& lhs,
        const_num_t const& rhs
    ) noexcept -> num_t::value_type;

    inline static constexpr auto add(
        num_t& lhs,
        const_num_t const& rhs
    ) noexcept -> num_t::value_type {
        auto const ls = lhs.is_neg();
        auto const rs = rhs.is_neg();
        if (ls == rs) {
            auto c = abs_add(lhs, rhs);
            return c;
        } else if (ls && !rs) {
            // out = -lhs + rhs => -(lhs - rhs)
            auto c = abs_sub<true>(lhs, rhs);
            lhs.set_neg(c == 0);
            return c;
        } else {
            // out = lhs - rhs
            auto c = abs_sub<true>(lhs, rhs);
            lhs.set_neg(c);
            return c;
        }
    }

    inline static constexpr auto add(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> Integer::value_type {
        auto const bits = std::max(lhs.bits(), rhs.bits());
        out.resize(bits + 1);

        auto a = lhs.to_span();
        auto b = rhs.to_span();
        auto o = out.to_span();

        // lhs <= rhs
        auto c = add(o, a, b);
        out.set_neg(o.is_neg());
        out.remove_trailing_empty_blocks();
        return c;
    }

    inline static constexpr auto sub(
        num_t& out,
        const_num_t const& lhs,
        const_num_t const& rhs
    ) noexcept -> num_t::value_type {
        auto const ls = lhs.is_neg();
        auto const rs = rhs.is_neg();
        // both are -ve or +ve
        if (ls != rs) {
            // out = lhs - (-rhs) => lhs + rhs
            // out = -lhs - rhs => -(lhs + rhs)
            auto c = abs_add(out, lhs, rhs);
            out.set_neg(lhs.is_neg());
            return c;
        } else if (ls && rs) {
            // out = -lhs - (-rhs) => -(lhs - rhs)
            auto c = abs_sub<true>(out, lhs, rhs);
            out.set_neg(c == 0);
            return c;
        } else {
            // out = lhs - rhs
            auto c = abs_sub<true>(out, lhs, rhs);
            out.set_neg(c);
            return c;
        }
    }

    inline static constexpr auto sub(
        num_t& lhs,
        const_num_t const& rhs
    ) noexcept -> num_t::value_type {
        auto const ls = lhs.is_neg();
        auto const rs = rhs.is_neg();
        // both are -ve or +ve
        if (ls != rs) {
            auto c = abs_add(lhs, rhs);
            return c;
        } else if (ls && rs) {
            // out = -lhs - (-rhs) => -lhs + rhs
            auto c = abs_sub<true>(lhs, rhs);
            lhs.set_neg(c == 0);
            return c;
        } else {
            // out = lhs - rhs
            auto c = abs_sub<true>(lhs, rhs);
            lhs.set_neg(c);
            return c;
        }
    }

    inline static constexpr auto sub(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> Integer::value_type {
        auto const bits = std::max(lhs.bits(), rhs.bits());
        out.resize(bits);

        auto a = lhs.to_span();
        auto b = rhs.to_span();
        auto o = out.to_span();

        auto c = sub(o, a, b);
        out.set_neg(o.is_neg());
        out.remove_trailing_empty_blocks();
        return c;
    }

    inline static constexpr auto inc(
        num_t lhs,
        Integer::value_type val = 1
    ) noexcept -> num_t::value_type {
        Integer::value_type c{val};
        auto i = 0zu;
        while (i < lhs.size() && c){
            auto r = lhs[i] + c;
            auto q = r & MachineConfig::mask;
            c = r >> MachineConfig::bits;
            lhs[i++] = static_cast<num_t::value_type>(q);
        }
        return c;
    }

    inline static constexpr auto dec(
        num_t lhs,
        Integer::value_type val = 1
    ) noexcept -> num_t::value_type {
        Integer::value_type c{val};
        auto i = 0zu;
        while (i < lhs.size() && c){
            auto r = lhs[i] - c;
            c = lhs[i] < c;
            auto q = r & MachineConfig::mask;
            lhs[i++] = static_cast<num_t::value_type>(q);
        }
        return c;
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_ADD_SUB_HPP
