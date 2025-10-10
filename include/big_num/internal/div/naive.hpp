#ifndef AMT_BIG_NUM_INTERNAL_DIV_NAIVE_HPP
#define AMT_BIG_NUM_INTERNAL_DIV_NAIVE_HPP

#include "../integer.hpp"
#include "../base.hpp"
#include "../logical_bitwise.hpp"
#include "../cmp.hpp"
#include "../add_sub.hpp"
#include <bit>
#include <span>

// TODO: fix signs
namespace big_num::internal {
    namespace detail {
        inline static constexpr auto fast_div(
            std::span<Integer::value_type> out_q,
            std::span<Integer::value_type> out_r,
            std::span<Integer::value_type const> num,
            std::span<Integer::value_type const> den
        ) noexcept -> bool {
            using acc_t = MachineConfig::acc_t;
            if (num.size() > 2 || den.size() > 2) return false;
            auto a = acc_t{num[0]} + (acc_t{num[1]} * MachineConfig::max);
            auto b = acc_t{den[0]} + (acc_t{den[1]} * MachineConfig::max);
            auto q = a / b;
            auto r = a % b;
            for (auto i = 0zu; i < 2; ++i) {
                out_q[i] = q % MachineConfig::max;
                out_r[i] = r % MachineConfig::max;
                q /= MachineConfig::max;
                r /= MachineConfig::max;
            }
            return true;
        }
    } // namespace detail

    template <bool BoundaryCheck = true>
    inline static constexpr auto naive_div(
        NumberSpan<Integer::value_type> out_q,
        NumberSpan<Integer::value_type> out_r,
        NumberSpan<Integer::value_type const> const& num,
        NumberSpan<Integer::value_type const> const& den
    ) noexcept -> bool {
        if constexpr (BoundaryCheck) {
            if (den.empty()) return false;
            if (num.empty()) return true;
        }
        if (detail::fast_div(out_q, out_r, num, den)) return true;

        auto m = num.bits();
        auto n = den.size();

        [[maybe_unused]] auto q = num.size() - n;
        assert(out_q.size() >= q && "quotient should have enough space");
        assert(out_r.size() >= n && "remainder should have enough space");

        for (auto i = m; i > 0; --i) {
            auto j = i - 1;
            shift_left(out_r, 1);
            set_integer_bit(
                out_r,
                0,
                get_integer_bit(num, j)
            );
            // den <= rem
            auto is_rem_bigger = abs_less_equal(den, out_r);
            if (is_rem_bigger) {
                abs_sub(out_r, den);
                set_integer_bit(out_q, j, true);
            }
        }
        return true;
    }

    /**
     * @returns true of division successful; otherwise false if division by zero
    */
    inline static constexpr auto naive_div(
        Integer& out_q,
        Integer& out_r,
        Integer const& num,
        Integer const& den
    ) -> bool {
        if (den.empty()) return false;
        if (num.empty()) return true;

        auto sz = num.size();
        out_q.resize(sz * MachineConfig::bits);
        out_r.resize(sz * MachineConfig::bits);

        naive_div<false>(out_q.to_span(), out_r.to_span(), num.to_span(), den.to_span());

        out_q.remove_trailing_empty_blocks();
        out_r.remove_trailing_empty_blocks();
        return true;
    }

    /**
     * @returns true of division successful; otherwise false if division by zero
    */
    inline static constexpr auto naive_div(
        Integer& out_q,
        Integer const& num,
        Integer const& den
    ) -> bool {
        if (den.empty()) return false;
        if (num.empty()) return true;
        auto sz = num.size();
        out_q.resize(sz);

        Integer out_r{};
        out_r.resize(sz);

        naive_div<false>(out_q.to_span(), out_r.to_span(), num.to_span(), den.to_span());
        out_r.destroy();

        out_q.remove_trailing_empty_blocks();
        return true;
    }

    template <Integer::value_type Den, bool IsSameBuffer = false>
        requires (Den > 0)
    inline static constexpr auto naive_div(
        NumberSpan<Integer::value_type> out_q,
        NumberSpan<Integer::value_type const> const& num
    ) noexcept -> Integer::value_type {
        assert(out_q.size() == num.size());

        if (num.empty()) return {};

        if constexpr (Den & (Den - 1)) {
            using acc_t = MachineConfig::acc_t;
            auto c = acc_t{};
            for (auto i = num.size(); i > 0; --i) {
                auto j = i - 1;
                auto e = (c << MachineConfig::bits) | num[j];
                auto q = e / Den;
                c = e % Den;
                out_q[j] = static_cast<Integer::value_type>(q);
            }
            return static_cast<Integer::value_type>(c);
        } else {
            auto r = num[0] & (Den - 1);
            if constexpr (!IsSameBuffer) {
                std::copy(num.begin(), num.end(), out_q.begin());
            }
            shift_right<static_cast<std::size_t>(std::bit_width(Den) - 1)>(out_q);
            return r;
        }
    }

    template <Integer::value_type Den>
        requires (Den > 0)
    inline static constexpr auto naive_div(
       NumberSpan<Integer::value_type>& out
    ) noexcept -> Integer::value_type {
        return naive_div<Den, true>(out, out);
    }

    template <bool IsSameBuffer = false>
    inline static constexpr auto naive_div(
        NumberSpan<Integer::value_type> out_q,
        NumberSpan<Integer::value_type const> const& num,
        Integer::value_type den
    ) noexcept -> Integer::value_type {
        assert(den > 0);
        assert(out_q.size() == num.size());

        if (num.empty()) return {};

        if (den & (den - 1)) {
            using acc_t = MachineConfig::acc_t;
            auto c = acc_t{};
            for (auto i = num.size(); i > 0; --i) {
                auto j = i - 1;
                auto e = (c << MachineConfig::bits) | num[j];
                auto q = e / den;
                c = e % den;
                out_q[j] = static_cast<Integer::value_type>(q);
            }
            return static_cast<Integer::value_type>(c);
        } else {
            auto r = num[0] & (den - 1);
            if constexpr (!IsSameBuffer) {
                std::copy(num.begin(), num.end(), out_q.begin());
            }
            shift_right(out_q, static_cast<std::size_t>(std::bit_width(den) - 1));
            return r;
        }
    }

    inline static constexpr auto naive_div(
        NumberSpan<Integer::value_type> out,
        Integer::value_type den
    ) noexcept -> Integer::value_type {
        return naive_div<true>(out, out, den);
    }

    template <Integer::value_type Den>
        requires (Den > 0)
    inline static constexpr auto naive_div(
        Integer& out_q,
        Integer& out_r,
        Integer const& num
    ) noexcept -> void {
        if (num.empty()) return;
        auto size = num.size();
        out_q.resize(size * MachineConfig::bits);
        out_r.resize(MachineConfig::bits);

        auto r = naive_div<Den>(out_q.to_span(), num.to_span());
        out_r.data()[0] = r;

        out_q.remove_trailing_empty_blocks();
        out_r.remove_trailing_empty_blocks();
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_DIV_NAIVE_HPP
