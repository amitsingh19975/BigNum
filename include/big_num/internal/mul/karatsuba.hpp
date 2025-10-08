#ifndef AMT_BIG_NUM_INTERNAL_MUL_KARATSUBA_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_KARATSUBA_HPP

#include "../integer.hpp"
#include "../base.hpp"
#include "naive.hpp"
#include <algorithm>
#include <memory_resource>
#include <span>
#include <vector>

namespace big_num::internal {
    namespace detail {
        template <std::size_t NaiveThreshold>
        inline static constexpr auto karatsuba_mul_helper(
            Integer::value_type* out,
            std::size_t out_size,
            Integer::value_type const* lhs,
            Integer::value_type const* rhs,
            std::size_t size,
            std::pmr::memory_resource* resource = std::pmr::get_default_resource()
        ) -> void {
            using uint_t = MachineConfig::uint_t;
            if (size <= NaiveThreshold) {
                auto o = std::span(out, out_size);
                auto l = std::span(lhs, size);
                auto r = std::span(rhs, size);
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

            auto xl = std::span(lhs, low);
            auto xu = std::span(lhs + low, high);
            auto yl = std::span(rhs, low);
            auto yu = std::span(rhs + low, high);

            auto sum_sz = std::max(low, high) + 1;
            auto x_sum = std::pmr::vector<uint_t>{sum_sz, 0, resource};
            auto y_sum = std::pmr::vector<uint_t>{sum_sz, 0, resource};

            auto const sz = size + 1;
            auto z0 = std::pmr::vector<uint_t>{sz, 0, resource};
            auto z2 = std::pmr::vector<uint_t>{sz, 0, resource};
            auto z3 = std::pmr::vector<uint_t>{sz, 0, resource};

            auto xc = abs_add({ x_sum.data(), sum_sz - 1 }, xl, xu);
            auto yc = abs_add({ y_sum.data(), sum_sz - 1 }, yl, yu);
            x_sum[sum_sz - 1] = xc;
            y_sum[sum_sz - 1] = yc;
            if (xc + yc == 0) sum_sz -= 1;

            BIG_NUM_TRACE(std::println("xl: {}\nxu: {}\nyl: {}\nyu: {}\nxs: {}\nys: {}", xl, xu, yl, yu, x_sum, y_sum));
            BIG_NUM_TRACE(std::println("xl_size: {}\nxu_size: {}\nyl_size: {}\nyu_size: {}\nxs_size: {}\nys_size: {}", xl.size(), xu.size(), yl.size(), yu.size(), x_sum.size(), y_sum.size()));

            BIG_NUM_TRACE(std::println("======= Z0 = xl * yl ========="));
            karatsuba_mul_helper<NaiveThreshold>(
                z0.data(), z0.size(),
                xl.data(),
                yl.data(),
                xl.size()
            );

            BIG_NUM_TRACE(std::println("======= Z2 = xu * yu ========="));
            karatsuba_mul_helper<NaiveThreshold>(
                z2.data(), z2.size(),
                xu.data(),
                yu.data(),
                xu.size()
            );

            BIG_NUM_TRACE(std::println("======= Z3 = x_sum * y_sum ========="));
            karatsuba_mul_helper<NaiveThreshold>(
                z3.data(), z3.size(),
                x_sum.data(),
                y_sum.data(),
                sum_sz
            );
            BIG_NUM_TRACE(std::println("=========== End ==========="));

            auto helper = [out_size, out](std::size_t start, std::size_t sz) {
                auto ns = std::max(out_size, start) - start;
                auto s = std::min(ns, sz);
                return std::span(out + start, s);
            };

            BIG_NUM_TRACE(std::println("z0: {}\nz2: {}\nz3: {}", z0, z2, z3));

            auto o1 = helper(0, out_size);
            abs_add(o1, z0);

            auto o3 = helper(low << 1, out_size);
            abs_add(o3, z2);

            auto o2 = helper(low, out_size);
            abs_add(o2, z3);
            abs_add(z0, z2);
            abs_sub(o2, z0);

            BIG_NUM_TRACE(std::println("O: {}", helper(0, out_size)));
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
        auto b = std::pmr::vector<uint_t>{size, 0, resource};

        std::copy_n(lhs.data(), lhs.size(), a.begin());
        std::copy_n(rhs.data(), rhs.size(), b.begin());

        detail::karatsuba_mul_helper<MachineConfig::nearest_even_number(NaiveThreshold)>(
            out.data(), out.size(),
            a.data(), b.data(),
            size
        );

        remove_trailing_zeros(out);
    }

    template <std::size_t NaiveThreshold = MachineConfig::naive_mul_threshold>
    inline static constexpr auto karatsuba_square(
        Integer& out,
        Integer const& a,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        using uint_t = MachineConfig::uint_t;

        auto size = a.size();
        out.resize((size << 1) * MachineConfig::bits);

        auto alloc = std::pmr::polymorphic_allocator<Integer::value_type>{resource};
        auto tmp = std::pmr::vector<uint_t>{a.data(), a.data() + a.size(), resource};

        detail::karatsuba_mul_helper<MachineConfig::nearest_even_number(NaiveThreshold)>(
            out.data(), out.size(),
            tmp.data(), tmp.data(),
            size
        );

        remove_trailing_zeros(out);
    }

    template <std::size_t NaiveThreshold = MachineConfig::naive_mul_threshold>
    inline static constexpr auto karatsuba_mul(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        using uint_t = MachineConfig::uint_t;
        auto size = std::max(lhs.size(), rhs.size());

        auto alloc = std::pmr::polymorphic_allocator<Integer::value_type>{resource};
        auto tmp = std::pmr::vector<uint_t>{resource};
        auto res = out;
        if (size * 2 > out.size()) {
            tmp.resize(size * 2, 0);
            res = tmp;
        }
        auto a = std::pmr::vector<uint_t>{size, 0, resource};
        auto b = std::pmr::vector<uint_t>{size, 0, resource};

        std::copy_n(lhs.data(), lhs.size(), a.begin());
        std::copy_n(rhs.data(), rhs.size(), b.begin());

        detail::karatsuba_mul_helper<MachineConfig::nearest_even_number(NaiveThreshold)>(
            res.data(), res.size(),
            a.data(), b.data(),
            size
        );

        if (out.data() != res.data()) {
            std::copy_n(tmp.begin(), out.size(), out.begin());
        }
    }

    template <std::size_t NaiveThreshold = MachineConfig::naive_mul_threshold>
    inline static constexpr auto karatsuba_square(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> a,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        using uint_t = MachineConfig::uint_t;
        auto size = a.size();
        assert((size << 1) <= out.size());

        auto alloc = std::pmr::polymorphic_allocator<Integer::value_type>{resource};
        auto tmp = std::pmr::vector<uint_t>{size, 0, resource};
        auto tmp0 = std::pmr::vector<uint_t>(resource);

        std::copy_n(a.data(), a.size(), tmp.begin());

        detail::karatsuba_mul_helper<MachineConfig::nearest_even_number(NaiveThreshold)>(
            out.data(), out.size(),
            tmp.data(), tmp.data(),
            size
        );
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_MUL_KARATSUBA_HPP
