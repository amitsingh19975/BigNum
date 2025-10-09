#ifndef AMT_BIG_NUM_INTERNAL_MUL_TOOM_COOK_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_TOOM_COOK_HPP

#include "../integer.hpp"
#include "../base.hpp"
#include "../div/naive.hpp"
#include "../logical_bitwise.hpp"
#include "../number_span.hpp"
#include "naive.hpp"
#include <algorithm>
#include <memory_resource>
#include <span>
#include <vector>

namespace big_num::internal {
    namespace detail {
        template <std::size_t NaiveThreshold>
        inline static constexpr auto toom_cook_3_helper(
            NumberSpan<Integer::value_type> out,
            NumberSpan<Integer::value_type const> const& lhs,
            NumberSpan<Integer::value_type const> const& rhs,
            std::size_t size,
            std::pmr::memory_resource* resource = std::pmr::get_default_resource()
        ) -> void {
            using uint_t = MachineConfig::uint_t;
            if (size <= NaiveThreshold) {
                auto o = out.slice(0, size << 1).abs();
                auto l = lhs.slice(0, size);
                auto r = rhs.slice(0, size);
                naive_mul(o, l, r);
                return;
            }

            auto const lsz = size / 3;
            auto const mid = lsz * 2;

            auto ll = lhs.slice(0, lsz).abs();
            auto lm = lhs.slice(lsz, mid).abs();
            auto lr = lhs.slice(mid).abs();

            auto rl = lhs.slice(0, lsz).abs();
            auto rm = lhs.slice(lsz, mid).abs();
            auto rr = lhs.slice(mid).abs();

            BIG_NUM_TRACE(std::println("lsz: {}, mid: {}, high: {}", lsz, mid, size - mid));
            BIG_NUM_TRACE(std::println("ll: {}\nlm: {}\nlr: {}", ll, lm, lr));
            BIG_NUM_TRACE(std::println("rl: {}\nrm: {}\nrr: {}", rl, rm, rr));

            using int_t = std::pmr::vector<uint_t>;
            auto l_n2_buf = int_t{resource};
            auto l_n1_buf = int_t{resource};
            auto l_0_buf = int_t{resource};
            auto l_1_buf = int_t{resource};
            auto l_inf_buf = int_t{resource};

            auto l_n2 = NumberSpan(std::span(l_n2_buf));
            auto l_n1 = NumberSpan(std::span(l_n1_buf));
            auto l_0 = NumberSpan(std::span(l_0_buf));
            auto l_1 = NumberSpan(std::span(l_1_buf));
            auto l_inf = NumberSpan(std::span(l_inf_buf));

            auto r_n2_buf = int_t{resource};
            auto r_n1_buf = int_t{resource};
            auto r_0_buf = int_t{resource};
            auto r_1_buf = int_t{resource};
            auto r_inf_buf = int_t{resource};

            auto r_n2 = NumberSpan(std::span(r_n2_buf));
            auto r_n1 = NumberSpan(std::span(r_n1_buf));
            auto r_0 = NumberSpan(std::span(r_0_buf));
            auto r_1 = NumberSpan(std::span(r_1_buf));
            auto r_inf = NumberSpan(std::span(r_inf_buf));

            auto eval = [resource](
                std::span<Integer::value_type const> m0, // lower
                std::span<Integer::value_type const> m1, // middle
                std::span<Integer::value_type const> m2, // upper 
                int_t& p_n2,  // p(-2)
                int_t& p_n1,  // p(-1)
                NumberSpan<Integer::value_type>& sp_n2,
                NumberSpan<Integer::value_type>& sp_n1,
                int_t& p_0,   // p(0)
                int_t& p_1,   // p(1)
                int_t& p_inf  // p(inf)
            ) -> void {
                // 1. pt = m0 + m2;
                auto pt = int_t{std::max(m0.size(), m2.size()) + 1, 0, resource};
                auto spt = NumberSpan(std::span(pt));
                add(spt, m0, m2);
                // auto spt = sign;

                // 2. p(0) = m0
                p_0.resize(m0.size());
                std::copy(m0.begin(), m0.end(), p_0.begin());

                // 3. p(1) = pt + m1
                p_1.resize(std::max(pt.size(), m1.size()) + 1, 0);
                auto sp1 = NumberSpan(std::span(p_1));
                add(sp1, spt, m1);

                // 4. p(-1) = pt - m1
                p_n1.resize(std::max(pt.size(), m1.size()), 0);
                sp_n1 = { p_n1 };
                sub(sp_n1, spt, m1);

                // 5. p(-2) = (p(-1) + m2) * 2 - m0
                p_n2.resize(std::max(p_n1.size(), m1.size()) + 1, 0);
                sp_n2 = { p_n2 };
                add(sp_n2, sp_n1, m2);
                shift_left<1>(sp_n2);
                sub(sp_n2, m0);

                // 2. p(inf) = m2
                p_inf.resize(m2.size());
                std::copy(m2.begin(), m2.end(), p_inf.begin());

            };

            eval(ll, lm, lr, l_n2_buf, l_n1_buf, l_n2, l_n1, l_0_buf, l_1_buf, l_inf_buf);
            eval(rl, rm, rr, r_n2_buf, r_n1_buf, r_n2, r_n1, r_0_buf, r_1_buf, r_inf_buf);

            l_0 = NumberSpan(std::span(l_0_buf));
            l_1 = NumberSpan(std::span(l_1_buf));
            l_inf = NumberSpan(std::span(l_inf_buf));
            r_0 = NumberSpan(std::span(r_0_buf));
            r_1 = NumberSpan(std::span(r_1_buf));
            r_inf = NumberSpan(std::span(r_inf_buf));

            l_n2.trim_trailing_zeros();
            l_n1.trim_trailing_zeros();
            l_0.trim_trailing_zeros();
            l_1.trim_trailing_zeros();
            l_inf.trim_trailing_zeros();

            r_n2.trim_trailing_zeros();
            r_n1.trim_trailing_zeros();
            r_0.trim_trailing_zeros();
            r_1.trim_trailing_zeros();
            r_inf.trim_trailing_zeros();

            auto sn2 = std::max(l_n2.size(), r_n2.size());
            auto sn1 = std::max(l_n1.size(), r_n1.size());
            auto s0 = std::max(l_0.size(), r_0.size());
            auto s1 = std::max(l_1.size(), r_1.size());
            auto sinf = std::max(l_inf.size(), r_inf.size());
            auto o_n2_buf = int_t{sn2 << 1, 0, resource};
            auto o_n1_buf = int_t{sn1 << 1, 0, resource};
            auto o_0_buf = int_t{s0 << 1, 0, resource};
            auto o_1_buf = int_t{s1 << 1, 0, resource};
            auto o_inf_buf = int_t{sinf << 1, 0, resource};

            auto o_n2 =  NumberSpan(std::span(o_n2_buf));
            auto o_n1 =  NumberSpan(std::span(o_n1_buf));
            auto o_0 =   NumberSpan(std::span(o_0_buf));
            auto o_1 =   NumberSpan(std::span(o_1_buf));
            auto o_inf = NumberSpan(std::span(o_inf_buf));


            BIG_NUM_TRACE(std::println("\n\nln2: {}\nln1: {}\nl0: {}\nl1: {}\nl_inf: {}\n", l_n2, l_n1, l_0, l_1, l_inf));
            BIG_NUM_TRACE(std::println("\n\nrn2: {}\nrn1: {}\nr0: {}\nr1: {}\nr_inf: {}\n", r_n2, r_n1, r_0, r_1, r_inf));
            // std::println("sn2: {}, sn1: {}, s0: {}, s1: {}, sinf: {}", sn2, sn1, s0, s1, sinf);

            // out(-2) = l_n2 * r_n2
            toom_cook_3_helper<NaiveThreshold>(
                o_n2,
                l_n2, r_n2,
                sn2
            );
            o_n2.set_neg(l_n2.is_neg() ^ r_n2.is_neg());
            // std::println("ON2: {}", o_n2);
            // exit(0);

            // out(-1) = l_n1 * r_n1
            toom_cook_3_helper<NaiveThreshold>(
                o_n1,
                l_n1, r_n1,
                sn1
            );
            o_n1.set_neg(l_n1.is_neg() ^ r_n1.is_neg());

            // out(0) = l_0 * r_0
            toom_cook_3_helper<NaiveThreshold>(
                o_0,
                l_0, r_0,
                s0
            );
            o_0.set_neg(l_0.is_neg() ^ r_0.is_neg());

            // out(1) = l_1 * r_1
            toom_cook_3_helper<NaiveThreshold>(
                o_1,
                l_1, r_1,
                s1
            );
            o_1.set_neg(l_1.is_neg() ^ r_1.is_neg());

            // out(inf) = l_inf * r_inf
            toom_cook_3_helper<NaiveThreshold>(
                o_inf,
                l_inf, r_inf,
                sinf
            );
            o_inf.set_neg(l_inf.is_neg() ^ r_inf.is_neg());

            BIG_NUM_TRACE(std::println("on2: {}\non1: {}\no0: {}\no1: {}\noinf: {}", o_n2, o_n1, o_0, o_1, o_inf));

            // 1. o0 = o_0;
            auto o0 = o_0;
            // 1. o4 = o_inf;
            auto o4 = o_inf;

            // 3. o3 = (o_n2 - o_1) / 3
            auto o3 = o_n2;
            sub(o3, o_1);
            naive_div<3>(o3);
            BIG_NUM_TRACE(std::println("3: {}", o3));

            // 4. o1 = (o_1 - o_n1) / 2
            auto o1 = o_1;
            sub(o1, o_n1);
            naive_div<2>(o1);
            BIG_NUM_TRACE(std::println("4: {}", o1));

            // 5. o2 = o_n1 - o_0;
            auto o2 = o_n1;
            sub(o2, o_0);
            BIG_NUM_TRACE(std::println("5: {}", o2));

            // 6. o3 = (o2 - o3)/2 + 2 * o_inf;
            sub(o3, o2, o3);
            naive_div<2>(o3);
            add(o3, o_inf);
            add(o3, o_inf);
            BIG_NUM_TRACE(std::println("6: {}", o3));

            // 7. o2 = o2 + o1 - o4
            add(o2, o1);
            sub(o2, o4);
            BIG_NUM_TRACE(std::println("7: {}", o2));


            // 8. o1 = o1 - o3
            sub(o1, o3);
            BIG_NUM_TRACE(std::println("8: {}", o1));

            BIG_NUM_TRACE(std::println("o0: {}\no1: {}\no2: {}\no3: {}\no4: {}", o0, o1, o2, o3, o4));

            // 9. out = o4 * x^4k + o3 * x^3k + o2 * x^2k + o1 * x^k + o0
            auto out0 = out;
            auto out1 = out.slice(1 * lsz);
            auto out2 = out.slice(2 * lsz);
            auto out3 = out.slice(3 * lsz);
            auto out4 = out.slice(4 * lsz);
            add(out0, o0);
            add(out1, o1);
            add(out2, o2);
            add(out3, o3);
            add(out4, o4);
            BIG_NUM_TRACE(std::println("O: {}", out));
        }
    } // namespace detail

    template <std::size_t NaiveThreshold = MachineConfig::naive_mul_threshold>
    inline static constexpr auto toom_cook_3(
        NumberSpan<Integer::value_type> out,
        NumberSpan<Integer::value_type const> lhs,
        NumberSpan<Integer::value_type const> rhs,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        using uint_t = MachineConfig::uint_t;
        auto sz = std::max(lhs.size(), rhs.size());
        sz += 3 - sz % 3; // round up to 3

        auto res = out;

        auto alloc = std::pmr::polymorphic_allocator<Integer::value_type>{resource};
        auto tmp = std::pmr::vector<uint_t>{resource};
        auto a = std::pmr::vector<uint_t>{sz, 0, resource};
        auto b = std::pmr::vector<uint_t>{resource};

        if (sz * 2 > out.size()) {
            tmp.resize(sz << 1, 0);
            res = { tmp, out.is_neg() };
        }

        std::copy_n(lhs.data(), lhs.size(), a.begin());

        auto ta = NumberSpan(std::span(a), false);
        auto tb = NumberSpan(std::span(a), false);
        if (!(lhs.data() == rhs.data() && lhs.size() == rhs.size())) {
            b.resize(sz, 0);
            std::copy_n(rhs.data(), rhs.size(), b.begin());
            tb = { b, false };
        }

        detail::toom_cook_3_helper<NaiveThreshold + (3 - NaiveThreshold % 3)>(
            res,
            ta, tb,
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
        auto b = std::pmr::vector<Integer::value_type>{resource};
        std::copy_n(lhs.data(), lhs.size(), a.begin());

        auto ta = NumberSpan(std::span(a), false);
        auto tb = NumberSpan(std::span(a), false);
        if (!(lhs.data() == rhs.data() && lhs.size() == rhs.size())) {
            b.resize(sz, 0);
            std::copy_n(rhs.data(), rhs.size(), b.begin());
            tb = { b, false };
        }

        detail::toom_cook_3_helper<NaiveThreshold + (3 - NaiveThreshold % 3)>(
            out,
            ta, tb,
            sz
        );
        out.set_neg(lhs.is_neg() ^ rhs.is_neg());
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_MUL_TOOM_COOK_HPP
