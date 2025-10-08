#ifndef AMT_BIG_NUM_INTERNAL_MUL_TOOM_COOK_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_TOOM_COOK_HPP

#include "../integer.hpp"
#include "../base.hpp"
#include "../div/naive.hpp"
#include "../logical_bitwise.hpp"
#include "naive.hpp"
#include <algorithm>
#include <memory_resource>
#include <span>
#include <vector>

namespace big_num::internal {
    namespace detail {
        template <std::size_t NaiveThreshold>
        inline static constexpr auto toom_cook_3_helper(
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

            auto const lsz = size / 3;
            auto const mid = lsz * 2;
            auto const rsz = size - mid;

            auto ll = std::span(lhs, lsz);
            auto lm = std::span(lhs + lsz, lsz);
            auto lr = std::span(lhs + mid, rsz);

            auto rl = std::span(rhs, lsz);
            auto rm = std::span(rhs + lsz, lsz);
            auto rr = std::span(rhs + mid, rsz);

            using int_t = std::pmr::vector<uint_t>;
            auto l_n2 = int_t{resource};
            auto l_n1 = int_t{resource};
            auto l_0 = int_t{resource};
            auto l_1 = int_t{resource};
            auto l_inf = int_t{resource};

            auto r_n2 = int_t{resource};
            auto r_n1 = int_t{resource};
            auto r_0 = int_t{resource};
            auto r_1 = int_t{resource};
            auto r_inf = int_t{resource};

            auto o_n2 = int_t{lsz << 1, 0, resource};
            auto o_n1 = int_t{lsz << 1, 0, resource};
            auto o_0 = int_t{lsz << 1, 0, resource};
            auto o_1 = int_t{lsz << 1, 0, resource};
            auto o_inf = int_t{lsz << 1, 0, resource};

            auto eval = [resource](
                std::span<Integer::value_type const> m0, // lower
                std::span<Integer::value_type const> m1, // middle
                std::span<Integer::value_type const> m2, // upper 
                int_t& p_n2, // p(-2)
                int_t& p_n1, // p(-1)
                int_t& p_0,  // p(0)
                int_t& p_1,  // p(1)
                int_t& p_inf // p(inf)
            ) -> void {
                // 1. pt = m0 + m2;
                auto pt = int_t{m0.size() + m1.size() + 1, 0, resource};
                abs_add(pt, m0, m2);

                // 2. p(0) = m0
                p_0.resize(m0.size());
                std::copy(m0.begin(), m0.end(), p_0.begin());

                // 3. p(1) = pt + m1
                p_1.resize(m0.size() + m1.size() + 1, 0);
                abs_add(p_1, pt, m1);

                // 4. p(-1) = pt - m1
                p_n1.resize(std::max(pt.size(), m1.size()), 0);
                abs_sub(p_n1, pt, m1);

                // 5. p(-2) = (p(-1) + m2) * 2 - m0
                p_n2.resize(pt.size() + m1.size() + 1, 0);
                abs_add(p_n2, p_n1, m2);
                shift_left<1>(p_n2);
                abs_sub(p_n2, m0);

                // 2. p(inf) = m2
                p_inf.resize(m2.size());
                std::copy(m2.begin(), m2.end(), p_inf.begin());
            };

            eval(ll, lm, lr, l_n2, l_n1, l_0, l_1, l_inf);
            eval(rl, rm, rr, r_n2, r_n1, r_0, r_1, r_inf);

            // std::println("ll: {}\nlm: {}\nlr: {}\nln2: {}\nln1: {}\nl0: {}\nl1: {}\nlinf: {}", ll, lm, lr, l_n2, l_n1, l_0, l_1, l_inf);
            // std::println("rl: {}\nrm: {}\nrr: {}\nrn2: {}\nrn1: {}\nr0: {}\nr1: {}\nrinf: {}", rl, rm, rr, r_n2, r_n1, r_0, r_1, r_inf);

            // out(-2) = l_n2 * r_n2
            toom_cook_3_helper<NaiveThreshold>(
                o_n2.data(), o_n2.size(),
                l_n2.data(), r_n2.data(),
                lsz
            );

            // out(-1) = l_n1 * r_n1
            toom_cook_3_helper<NaiveThreshold>(
                o_n1.data(), o_n1.size(),
                l_n1.data(), r_n1.data(),
                lsz
            );

            // out(0) = l_0 * r_0
            toom_cook_3_helper<NaiveThreshold>(
                o_0.data(), o_0.size(),
                l_0.data(), r_0.data(),
                lsz
            );

            // out(1) = l_1 * r_1
            toom_cook_3_helper<NaiveThreshold>(
                o_1.data(), o_1.size(),
                l_1.data(), r_1.data(),
                lsz
            );

            // out(inf) = l_inf * r_inf
            toom_cook_3_helper<NaiveThreshold>(
                o_inf.data(), o_inf.size(),
                l_inf.data(), r_inf.data(),
                lsz
            );
            // std::println("on2: {}\non1: {}\no0: {}\no1: {}\noinf: {}", o_n2, o_n1, o_0, o_1, o_inf);

            // 1. o0 = o_0;
            auto o0 = std::span(o_0);
            // 1. o4 = o_inf;
            auto o4 = std::span(o_inf);

            // 3. o3 = (o_n2 - o_1) / 3
            auto o3 = std::span(o_n2);
            abs_sub(o3, o_1);
            naive_div<3>(o3);

            // 4. o1 = (o_1 - o_n1) / 2
            auto o1 = std::span(o_1);
            abs_sub(o1, o_n1);
            naive_div<2>(o1);

            // 5. o2 = o_n1 - o_0;
            auto o2 = std::span(o_n1);
            abs_sub(o2, o_0);

            // 6. o3 = (o2 - o3)/2 + 2 * o_inf;
            abs_sub(o3, o2, o3);
            naive_div<2>(o3);
            abs_add(o3, o_inf);
            abs_add(o3, o_inf);

            // 7. o2 = o2 + o1 - o4
            abs_add(o2, o1);
            abs_sub(o2, o4);


            // 8. o1 = o1 - o3
            abs_sub(o1, o3);

            // 9. out = o4 * x^4k + o3 * x^3k + o2 * x^2k + o1 * x^k + o0
            abs_add({ out + 0 * lsz, out_size - 0 * lsz }, o0);
            abs_add({ out + 1 * lsz, out_size - 1 * lsz }, o1);
            abs_add({ out + 2 * lsz, out_size - 2 * lsz }, o2);
            abs_add({ out + 3 * lsz, out_size - 3 * lsz }, o3);
            abs_add({ out + 4 * lsz, out_size - 4 * lsz }, o4);
        }
    } // namespace detail

    template <std::size_t NaiveThreshold = MachineConfig::naive_mul_threshold>
    inline static constexpr auto toom_cook_3(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        auto sz = std::max(lhs.size(), rhs.size());
        sz += 3 - sz % 3; // round up to 3
        out.resize((sz << 1) * MachineConfig::bits);

        auto a = std::pmr::vector<Integer::value_type>{sz, 0, resource};
        auto b = std::pmr::vector<Integer::value_type>{sz, 0, resource};
        std::copy_n(lhs.data(), lhs.size(), a.begin());
        std::copy_n(rhs.data(), rhs.size(), b.begin());

        detail::toom_cook_3_helper<NaiveThreshold + (3 - NaiveThreshold % 3)>(
            out.data(), out.size(),
            a.data(), b.data(),
            sz
        );
    }

    template <std::size_t NaiveThreshold = MachineConfig::naive_mul_threshold>
    inline static constexpr auto toom_cook_3(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        using uint_t = MachineConfig::uint_t;
        auto sz = std::max(lhs.size(), rhs.size());
        sz += 3 - sz % 3; // round up to 3
        assert((sz << 1) <= out.size());

        auto alloc = std::pmr::polymorphic_allocator<Integer::value_type>{resource};
        auto a = std::pmr::vector<uint_t>{sz, 0, resource};
        auto b = std::pmr::vector<uint_t>{sz, 0, resource};

        std::copy_n(lhs.data(), lhs.size(), a.begin());
        std::copy_n(rhs.data(), rhs.size(), b.begin());

        detail::toom_cook_3_helper<NaiveThreshold + (3 - NaiveThreshold % 3)>(
            out.data(), out.size(),
            a.data(), b.data(),
            sz
        );
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_MUL_TOOM_COOK_HPP
