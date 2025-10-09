#ifndef AMT_BIG_NUM_INTERNAL_MUL_KARATSUBA_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_KARATSUBA_HPP

#include "../integer.hpp"
#include "../base.hpp"
#include "naive.hpp"
#include <algorithm>
#include <memory_resource>
#include <vector>

namespace big_num::internal {
    namespace detail {
        template <std::size_t NaiveThreshold>
        inline static constexpr auto karatsuba_mul_helper(
            num_t out,
            const_num_t const& lhs,
            const_num_t const& rhs,
            std::size_t size,
            std::pmr::memory_resource* resource = std::pmr::get_default_resource()
        ) -> void {
            using uint_t = MachineConfig::uint_t;
            if (size <= NaiveThreshold) {
                auto o = out;
                auto l = lhs.slice(0, size);
                auto r = rhs.slice(0, size);
                naive_mul(o, l, r);
                return;
            }

            // lhs * rhs = z2 * B^2 + z1 * B + z0
            // z0 = x_l * y_l
            // z1 = z3 - z0 - z2
            // z2 = x_u * y_u
            // z3 = (x_l + x_u) * (y_l + y_u)

            auto const half = size >> 1;
            auto const low = half;
            auto const high = size - low;

            BIG_NUM_TRACE(std::println("size: {}, half: {}, low: {}, high: {}", size, half, low, high));

            auto xl = lhs.slice(0, low).abs();
            auto xu = lhs.slice(low).abs();
            auto yl = rhs.slice(0, low).abs();
            auto yu = rhs.slice(low).abs();

            auto sum_sz = std::max(low, high) + 1;
            auto x_sum = std::pmr::vector<uint_t>{sum_sz, 0, resource};
            auto y_sum = std::pmr::vector<uint_t>{sum_sz, 0, resource};

            auto const sz = size;
            auto z0_buff = std::pmr::vector<uint_t>{sz, 0, resource};
            auto z2_buff = std::pmr::vector<uint_t>{sz, 0, resource};
            auto z3_buff = std::pmr::vector<uint_t>{sz, 0, resource};

            auto z0 = NumberSpan(std::span(z0_buff));
            auto z2 = NumberSpan(std::span(z2_buff));
            auto z3 = NumberSpan(std::span(z3_buff));

            auto sx_sum = NumberSpan(std::span{ x_sum.data(), sum_sz - 1 });
            auto sy_sum = NumberSpan(std::span{ y_sum.data(), sum_sz - 1 });
            auto xc = add(sx_sum, xl, xu);
            auto yc = add(sy_sum, yl, yu);
            x_sum[sum_sz - 1] = xc;
            y_sum[sum_sz - 1] = yc;
            if (xc + yc == 0) sum_sz -= 1;

            BIG_NUM_TRACE(std::println("xl: {}\nxu: {}\nyl: {}\nyu: {}\nxs: {}\nys: {}", xl, xu, yl, yu, x_sum, y_sum));
            BIG_NUM_TRACE(std::println("xl_size: {}\nxu_size: {}\nyl_size: {}\nyu_size: {}\nxs_size: {}\nys_size: {}", xl.size(), xu.size(), yl.size(), yu.size(), x_sum.size(), y_sum.size()));

            BIG_NUM_TRACE(std::println("======= Z0 = xl * yl ========="));
            karatsuba_mul_helper<NaiveThreshold>(
                z0,
                xl,
                yl,
                xl.size()
            );

            BIG_NUM_TRACE(std::println("======= Z2 = xu * yu ========="));
            karatsuba_mul_helper<NaiveThreshold>(
                z2,
                xu,
                yu,
                xu.size()
            );

            BIG_NUM_TRACE(std::println("======= Z3 = x_sum * y_sum ========="));
            karatsuba_mul_helper<NaiveThreshold>(
                z3,
                { x_sum.data(), sum_sz },
                { y_sum.data(), sum_sz },
                sum_sz
            );
            BIG_NUM_TRACE(std::println("=========== End ==========="));

            z0.trim_trailing_zeros();
            z2.trim_trailing_zeros();
            z3.trim_trailing_zeros();

            BIG_NUM_TRACE(std::println("z0: {}\nz2: {}\nz3: {}", z0, z2, z3));

            auto o1 = out;
            add(o1, z0);

            auto o3 = out.slice(low * 2);
            add(o3, z2);

            auto o2 = out.slice(low);
            sub(z3, z0);
            sub(z3, z2);
            add(o2, z3);

            BIG_NUM_TRACE(std::println("O: {}", out));
        }
    } // namespace detail

    template <std::size_t NaiveThreshold = MachineConfig::naive_mul_threshold>
    inline static constexpr auto karatsuba_mul(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        using uint_t = MachineConfig::uint_t;

        auto size = std::max(lhs.size(), rhs.size());
        out.resize((size << 1) * MachineConfig::bits);

        auto alloc = std::pmr::polymorphic_allocator<Integer::value_type>{resource};
        auto a = std::pmr::vector<uint_t>{size, 0, resource};
        auto b = std::pmr::vector<uint_t>{resource};

        std::copy_n(lhs.data(), lhs.size(), a.begin());

        auto ta = NumberSpan(std::span(a), false);
        auto tb = NumberSpan(std::span(a), false);
        if (!(lhs.data() == rhs.data() && lhs.size() == rhs.size())) {
            b.resize(size, 0);
            std::copy_n(rhs.data(), rhs.size(), b.begin());
            tb = { b, false };
        }

        detail::karatsuba_mul_helper<MachineConfig::nearest_even_number(NaiveThreshold)>(
            out,
            ta, tb,
            size
        );

        out.set_neg(lhs.is_neg() != rhs.is_neg());
        remove_trailing_zeros(out);
    }

    template <std::size_t NaiveThreshold = MachineConfig::naive_mul_threshold>
    inline static constexpr auto karatsuba_mul(
        num_t& out,
        const_num_t const& lhs,
        const_num_t const& rhs,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        using uint_t = MachineConfig::uint_t;
        auto size = std::max(lhs.size(), rhs.size());

        auto alloc = std::pmr::polymorphic_allocator<Integer::value_type>{resource};
        auto tmp = std::pmr::vector<uint_t>{resource};
        auto res = out;
        if (size * 2 > out.size()) {
            tmp.resize(size * 2, 0);
            res = { tmp };
        }
        auto a = std::pmr::vector<uint_t>{size, 0, resource};
        auto b = std::pmr::vector<uint_t>{resource};

        std::copy_n(lhs.data(), lhs.size(), a.begin());

        auto ta = NumberSpan(std::span(a), false);
        auto tb = NumberSpan(std::span(a), false);
        if (!(lhs.data() == rhs.data() && lhs.size() == rhs.size())) {
            b.resize(size, 0);
            std::copy_n(rhs.data(), rhs.size(), b.begin());
            tb = { b, false };
        }

        detail::karatsuba_mul_helper<MachineConfig::nearest_even_number(NaiveThreshold)>(
            res,
            ta, tb,
            size
        );

        if (out.data() != res.data()) {
            std::copy_n(tmp.begin(), out.size(), out.begin());
        }
        out.set_neg(lhs.is_neg() != rhs.is_neg());
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_MUL_KARATSUBA_HPP
