#ifndef AMT_BIG_NUM_INTERNAL_MUL_TOOM_COOK_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_TOOM_COOK_HPP

#include "../integer.hpp"
#include "../base.hpp"
#include "../div/naive.hpp"
#include "../logical_bitwise.hpp"
#include "big_num/internal/integer_parse.hpp"
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

            // auto helper = [](std::span<uint_t const> t) {
            //     auto tmp = std::vector<uint_t>(t.begin(), t.end());
            //     twos_complement(tmp);
            //     return tmp;
            // };

            BIG_NUM_TRACE(std::println("ll: {}\nlm: {}\nlr: {}", ll, lm, lr));
            BIG_NUM_TRACE(std::println("rl: {}\nrm: {}\nrr: {}", rl, rm, rr));

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
                int_t& p_n2,  // p(-2)
                int_t& p_n1,  // p(-1)
                int_t& p_0,   // p(0)
                int_t& p_1,   // p(1)
                int_t& p_inf, // p(inf)
                bool& sn1,
                bool& sn2
            ) -> bool {
                // 1. pt = m0 + m2;
                auto pt = int_t{std::max(m0.size(), m2.size()) + 1, 0, resource};
                abs_add(pt, m0, m2);

                // 2. p(0) = m0
                p_0.resize(m0.size());
                std::copy(m0.begin(), m0.end(), p_0.begin());

                // 3. p(1) = pt + m1
                p_1.resize(std::max(pt.size(), m1.size()) + 1, 0);
                abs_add(p_1, pt, m1);

                // 4. p(-1) = pt - m1
                p_n1.resize(std::max(pt.size(), m1.size()), 0);
                sn1 = abs_sub(p_n1, pt, m1);
                // if (cn1) twos_complement(p_n1);

                // 5. p(-2) = (p(-1) + m2) * 2 - m0
                p_n2.resize(std::max(p_n1.size(), m1.size()) + 1, 0);
                if (!sn1) abs_add(p_n2, p_n1, m2);
                else sn2 = abs_sub(p_n2, m2, p_n1);
                shift_left<1>(p_n2);
                if (sn2) {
                    abs_add(p_n2, m0);
                } else {
                    pt.resize(p_n2.size());
                    std::copy(p_n2.begin(), p_n2.end(), pt.begin());
                    sn2 = abs_sub(p_n2, pt, m0);
                }

                // 2. p(inf) = m2
                p_inf.resize(m2.size());
                std::copy(m2.begin(), m2.end(), p_inf.begin());
                return false;
            };

            bool sln1 = false;
            bool sln2 = false;
            bool srn1 = false;
            bool srn2 = false;
            eval(ll, lm, lr, l_n2, l_n1, l_0, l_1, l_inf, sln1, sln2);
            eval(rl, rm, rr, r_n2, r_n1, r_0, r_1, r_inf, srn1, srn2);

            BIG_NUM_TRACE(std::println("\n\nln2: {}{}\nln1: {}{}\nl0: {}\nl1: {}\nl_inf: {}\n", sln2 ? '-' : '+', l_n2, sln2 ? '-' : '+', l_n1, l_0, l_1, l_inf));
            BIG_NUM_TRACE(std::println("\n\nrn2: {}{}\nrn1: {}{}\nr0: {}\nr1: {}\nr_inf: {}\n", srn2 ? '-' : '+', r_n2, srn2 ? '-' : '+', r_n1, r_0, r_1, r_inf));

            // out(-2) = l_n2 * r_n2
            bool son2 = static_cast<bool>(sln2 ^ srn2);
            toom_cook_3_helper<NaiveThreshold>(
                o_n2.data(), o_n2.size(),
                l_n2.data(), r_n2.data(),
                lsz
            );

            // out(-1) = l_n1 * r_n1
            bool son1 = static_cast<bool>(sln1 ^ srn1);
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
            BIG_NUM_TRACE(std::println("on2: {}\non1: {}\no0: {}\no1: {}\noinf: {}", o_n2, o_n1, o_0, o_1, o_inf));

            auto add1 = [](
                std::span<uint_t> out,
                bool so,
                std::span<uint_t const> rhs,
                bool sr
            ) {
                if ((so ^ sr) != 0) {
                    so = abs_sub(out, rhs);
                } else {
                    abs_add(out, rhs);
                }
                return so;
            };

            auto sub1 = [](
                std::span<uint_t> out,
                bool so,
                std::span<uint_t const> rhs,
                bool sr
            ) {
                // out = out - rhs
                if (so && sr) {
                    // out = -(out - rhs)
                    auto c = abs_sub(out, rhs);
                    so = c == 0;
                } else if (!so && sr) {
                    // out = out + rhs
                    abs_add(out, rhs);
                } else if (so && !sr) {
                    // out = out - rhs
                    so = abs_sub(out, rhs);
                } else {
                    so = abs_sub(out, rhs);
                }
                return so;
            };

            auto sub2 = [](
                std::span<uint_t> out,
                std::span<uint_t const> lhs,
                bool sl,
                std::span<uint_t const> rhs,
                bool sr
            ) {
                auto so = sl;
                // out = out - rhs
                if (sl && sr) {
                    // out = rhs - lhs
                    so = abs_sub(out, rhs, lhs);
                } else if (!sl && sr) {
                    // out = out + rhs
                    abs_add(out, lhs, rhs);
                } else if (sl && !sr) {
                    // out = out - rhs
                    so = abs_sub(out, lhs, rhs);
                } else {
                    so = abs_sub(out, lhs, rhs);
                }
                return so;
            };

            // 1. o0 = o_0;
            auto o0 = std::span(o_0);
            // 1. o4 = o_inf;
            auto o4 = std::span(o_inf);

            // 3. o3 = (o_n2 - o_1) / 3
            auto o3 = std::span(o_n2);
            auto so3 = sub1(o3, son2, o_1, false);
            naive_div<3>(o3);
            BIG_NUM_TRACE(std::println("3: {}{}", so3 ? '-' : '+', o3));

            // 4. o1 = (o_1 - o_n1) / 2
            auto o1 = std::span(o_1);
            auto so1 = sub1(o1, false, o_n1, son1);
            naive_div<2>(o1);
            BIG_NUM_TRACE(std::println("4: {}{}", so1 ? '-' : '+', o1));

            // 5. o2 = o_n1 - o_0;
            auto o2 = std::span(o_n1);
            auto so2 = sub1(o2, son1, o_0, false);
            BIG_NUM_TRACE(std::println("5: {}", so2 ? '-' : '+', o2));

            // 6. o3 = (o2 - o3)/2 + 2 * o_inf;
            so3 = sub2(o3, o2, so2, o3, so3);
            naive_div<2>(o3);
            so3 = add1(o3, so3, o_inf, false);
            so3 = add1(o3, so3, o_inf, false);
            BIG_NUM_TRACE(std::println("6: {}", so3 ? '-' : '+', o3));

            // 7. o2 = o2 + o1 - o4
            add1(o2, so2, o1, so1);
            so2 = sub1(o2, so2, o4, false);
            BIG_NUM_TRACE(std::println("7: {}", so2 ? '-' : '+', o2));


            // 8. o1 = o1 - o3
            so1 = sub1(o1, so1, o3, so3);
            BIG_NUM_TRACE(std::println("8: {}", so1 ? '-' : '+', o1));

            BIG_NUM_TRACE(std::println("o0: {}\no1: {}\no2: {}\no3: {}\no4: {}", o0, o1, o2, o3, o4));

            // 9. out = o4 * x^4k + o3 * x^3k + o2 * x^2k + o1 * x^k + o0
            add1({ out + 0 * lsz, out_size - 0 * lsz }, false, o0, false);
            add1({ out + 1 * lsz, out_size - 1 * lsz }, false, o1, so1);
            add1({ out + 2 * lsz, out_size - 2 * lsz }, false, o2, so2);
            add1({ out + 3 * lsz, out_size - 3 * lsz }, false, o3, so3);
            add1({ out + 4 * lsz, out_size - 4 * lsz }, false, o4, false);
            BIG_NUM_TRACE(std::println("O: {}", std::span(out, out_size)));
        }
    } // namespace detail

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

        auto res = out;

        auto alloc = std::pmr::polymorphic_allocator<Integer::value_type>{resource};
        auto tmp = std::pmr::vector<uint_t>{resource};
        auto a = std::pmr::vector<uint_t>{sz, 0, resource};
        auto b = std::pmr::vector<uint_t>{sz, 0, resource};

        if (sz * 2 > out.size()) {
            tmp.resize(sz << 1, 0);
            res = tmp;
        }

        std::copy_n(lhs.data(), lhs.size(), a.begin());
        std::copy_n(rhs.data(), rhs.size(), b.begin());

        detail::toom_cook_3_helper<NaiveThreshold + (3 - NaiveThreshold % 3)>(
            res.data(), res.size(),
            a.data(), b.data(),
            sz
        );

        if (out.data() != res.data()) {
            std::copy_n(res.begin(), out.size(), out.begin());
        }
    }

    template <std::size_t NaiveThreshold = MachineConfig::naive_mul_threshold>
    inline static constexpr auto toom_cook_3_square(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> a,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        using uint_t = MachineConfig::uint_t;
        auto sz = a.size();
        sz += 3 - sz % 3; // round up to 3

        auto res = out;

        auto alloc = std::pmr::polymorphic_allocator<Integer::value_type>{resource};
        auto tmp = std::pmr::vector<uint_t>{sz, 0, resource};
        auto tmp0 = std::pmr::vector<uint_t>{resource};

        std::copy_n(a.data(), a.size(), tmp.begin());

        if (sz * 2 > out.size()) {
            tmp0.resize(sz << 1, 0);
            res = tmp0;
        }

        detail::toom_cook_3_helper<NaiveThreshold + (3 - NaiveThreshold % 3)>(
            res.data(), res.size(),
            tmp.data(), tmp.data(),
            sz
        );

        if (out.data() != res.data()) {
            std::copy_n(res.begin(), out.size(), out.begin());
        }
    }

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
        Integer& out,
        Integer const& a,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        auto sz = a.size();
        sz += 3 - sz % 3; // round up to 3
        out.resize((sz << 1) * MachineConfig::bits);

        auto tmp = std::pmr::vector<Integer::value_type>{sz, 0, resource};
        std::copy_n(a.data(), a.size(), tmp.begin());

        detail::toom_cook_3_helper<NaiveThreshold + (3 - NaiveThreshold % 3)>(
            out.data(), out.size(),
            tmp.data(), tmp.data(),
            sz
        );
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_MUL_TOOM_COOK_HPP
