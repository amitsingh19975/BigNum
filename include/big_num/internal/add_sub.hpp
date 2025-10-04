#ifndef AMT_BIG_NUM_INTERNAL_ADD_SUB_HPP
#define AMT_BIG_NUM_INTERNAL_ADD_SUB_HPP

#include "base.hpp"
#include "cmp.hpp"
#include "integer_parse.hpp"
#include "integer.hpp"
#include "ui.hpp"
#include <type_traits>

namespace big_num::internal {
    inline static constexpr auto abs_add(
        std::span<Integer::value_type>       lhs,
        std::span<Integer::value_type const> rhs
    ) -> void {
        if (lhs.empty()) return;

        auto o = lhs.data();
        auto b = rhs.data();
        auto osz = lhs.size();
        auto bsz = std::min(rhs.size(), osz);

        using simd_t = MachineConfig::simd_uint_t;
        static constexpr auto N = simd_t::elements;
        using sacc_t = MachineConfig::simd_acc_t;
        using acc_t = MachineConfig::acc_t;

        auto c = sacc_t::zeroed();

        auto i = std::size_t{};
        if (!std::is_constant_evaluated()) {
            auto nsz = bsz - bsz % N;
            for (; i < nsz; i += N) {
                auto l = ui::cast<acc_t>(simd_t::load(o + i, N));
                auto r = ui::cast<acc_t>(simd_t::load(b + i, N));
                auto [q, tc] = ui::addc<MachineConfig::bits>(l, r, c);
                std::copy_n(q.data(), N, o + i);
                c[0] = tc;
            }
        }

        auto carry = c[0];
        for (; i < bsz; ++i) {
            auto l = static_cast<acc_t>(o[i]);
            auto r = static_cast<acc_t>(b[i]);
            auto q = l + r + carry;
            o[i] = static_cast<MachineConfig::uint_t>(q & MachineConfig::mask);
            carry = q >> MachineConfig::bits;
        }

        auto sz = osz;
        while (i < sz && carry) {
            auto l = static_cast<acc_t>(o[i]);
            auto r = carry;
            auto q = l + r;
            o[i] = static_cast<MachineConfig::uint_t>(q & MachineConfig::mask);
            carry = q >> MachineConfig::bits;
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
        out.fill(0);

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

        using simd_t = MachineConfig::simd_uint_t;
        static constexpr auto N = simd_t::elements;
        using sacc_t = MachineConfig::simd_acc_t;
        using acc_t = MachineConfig::acc_t;

        auto c = sacc_t::zeroed();

        auto i = std::size_t{};
        if (!std::is_constant_evaluated()) {
            auto nsz = bsz - bsz % N;
            for (; i < nsz; i += N) {
                auto l = ui::cast<acc_t>(simd_t::load(o + i, N));
                auto r = ui::cast<acc_t>(simd_t::load(b + i, N)) + c;
                auto [q, tc] = ui::subc<MachineConfig::bits>(l, r, c);
                std::copy_n(q.data(), N, o + i);
                c[0] = tc;
            }
        }

        auto carry = c[0];
        auto helper = [&carry, &i, o](acc_t l, acc_t r) {
            auto is_small = l < r;
            auto q = l - r;
            o[i] = static_cast<MachineConfig::uint_t>(q & MachineConfig::mask);
            carry = is_small;
        };

        for (; i < bsz; ++i) {
            auto l = static_cast<acc_t>(o[i]);
            auto r = static_cast<acc_t>(b[i]) + carry;
            helper(l, r);
        }

        auto sz = osz;
        while (carry && i < sz) {
            auto l = static_cast<acc_t>(o[i]);
            auto r = carry;
            helper(l, r);
            ++i;
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
        out.fill(0);

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
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_ADD_SUB_HPP
