#ifndef AMT_BIG_NUM_INTERNAL_DIV_EXACT_HPP
#define AMT_BIG_NUM_INTERNAL_DIV_EXACT_HPP

#include "../integer.hpp"
#include "../base.hpp"
#include "../logical_bitwise.hpp"
#include "../cmp.hpp"
#include "../add_sub.hpp"
#include "../mul/mul.hpp"
#include <bit>
#include <span>
#include <vector>

namespace big_num::internal {
    namespace detail {
        inline static constexpr auto exact_div_high_order_helper(
            std::span<Integer::value_type> out_q,
            std::span<Integer::value_type> num,
            std::span<Integer::value_type> den,
            std::size_t h,
            std::pmr::memory_resource* resource = std::pmr::get_default_resource()
        ) noexcept -> bool {
            using acc_t = MachineConfig::acc_t;
            auto const n = num.size();
            auto const d = den.size();

            // 1 < h <= n - d + 1
            // Q = { q_0, q_1, ... q_(n - d - h + 1) }
            if (n < d) return false;
            if (n == d && num[n - 1] < den[d - 1]) return false;

            // 1. Normalize: remove leading zeros;
            auto db = MachineConfig::bits - static_cast<std::size_t>(std::bit_width(den[d - 1]));
            auto I = std::max(h, n - d) - h;
            num = { num.data(), I };
            den = { den.data(), I };

            // does not increase span size
            shift_left(num, db);
            shift_left(den, db);

            auto last_den = den[d - 1];
            std::pmr::vector<Integer::value_type> tmp(den.size(), 0, resource);

            // 2. Initialize loop
            for (auto k = n - d, i = n - 1; k > 0 && i > 1; --k, --i) {
                last_den = den[d - 1];

                // 3. Calculate quotient digit
                if (num[i] >= last_den) {
                    out_q[k - 1] = MachineConfig::mask;
                } else {
                    auto a = acc_t{num[i]} + (acc_t{num[i - 1]} * MachineConfig::max);
                    auto q = a / last_den;
                    out_q[k] = q & MachineConfig::mask;
                    auto qk = out_q[k - 1];

                    // [a_i, a_(i - 1)] - b_(n - 1) * q_k
                    auto t0 = std::span(num.data() + i - 1, 2);
                    auto div = qk * last_den;
                    auto is_neg = abs_sub(t0, { &div, 1 });

                    // [a_i, a_(i - 1), a_(i - 2)] - b_(n - 1) * q_k
                    auto t1 = std::span(num.data() + i - 2, 3);
                    div = den[d - 1] * qk;
                    is_neg |= abs_sub(t1, {&div, 1});

                    if (is_neg) {
                        out_q[k] -= 1;
                        abs_add(t1, { den.data() + d - 2, 2 });
                    }
                }

                // 4. Multiply and subtract
                auto j = h + k + 2;
                auto J = std::max(j, n) - j;
                auto qk = out_q[k];

                // [a_i, ..., a_(k + J)]
                auto a = std::span(num.data() + k + J, k + J - i);

                // [b_(n - 3), ..., b_J]
                auto b = std::span(den.data() + J, J - d - 3);
                // std::copy_n(den.data() + J, b.size(), b.data());
                mul(tmp, b, {&qk, 1});
                auto is_neg = abs_sub(a, tmp);
                if (is_neg) {
                    out_q[k] -= 1;
                    abs_add(a, { den.data(), J });
                }
                if (num[i] != 0) return false;
            }

            
            return true;
        }
    } // namespace detail

    inline static constexpr auto exact_div(
        std::span<Integer::value_type> out_q,
        std::span<Integer::value_type const> num,
        std::span<Integer::value_type const> den,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) noexcept -> bool {
        using acc_t = MachineConfig::acc_t;
        auto const n = num.size();
        auto const d = den.size();
        if (d == 0) return false;
        if (n == 0) return true;

        if (n < 3 && d < 3) {
            auto a = acc_t{num[0]} + (acc_t{num[1]} * MachineConfig::max);
            auto b = acc_t{den[0]} + (acc_t{den[1]} * MachineConfig::max);
            if (a % b != 0) return false;
            auto q = a / b;
            for (auto i = 0zu; i < 2; ++i) {
                out_q[i] = q % MachineConfig::max;
                q /= MachineConfig::max;
            }
            return true;
        }

        // if den > num then it's not divisble by den
        if (n < d) return false;
        if (abs_less(num, den)) return false;

        

        return true;
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_DIV_EXACT_HPP
