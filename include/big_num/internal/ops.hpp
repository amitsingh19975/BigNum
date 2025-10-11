#ifndef AMT_BIG_NUM_INTERNAL_OPS_HPP
#define AMT_BIG_NUM_INTERNAL_OPS_HPP

#include "base.hpp"
#include "mul/mul.hpp"
#include "integer.hpp"
#include <memory_resource>
#include <type_traits>

namespace big_num::internal {
    inline static constexpr auto clone(Integer const& a) -> Integer {
        auto tmp = Integer{};
        tmp.resize(a.bits());
        tmp.set_neg(a.is_neg());
        std::copy_n(a.data(), a.size(), tmp.data());
        return tmp;
    }

    inline static constexpr auto abs(Integer& a) noexcept -> Integer& {
        a.set_neg(false);
        return a;
    }

    namespace detail {
        template <std::integral T>
            requires std::is_unsigned_v<T>
        inline static constexpr auto pow_cal_size(
            std::size_t size,
            T p
        ) noexcept -> std::size_t {
            return size * p + 1;
        }
    } // namespace detail

    template <std::integral T>
        requires std::is_unsigned_v<T>
    inline static constexpr auto pow(
        num_t out,
        const_num_t const& a,
        T p,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        if (p == 0) {
            std::fill(out.begin(), out.end(), 0);
            out[0] = 1;
            return;
        } else if (p == 1) {
            if (out.data() == a.data()) return;
            std::copy(a.begin(), a.end(), out.begin());
            return;
        }

        std::pmr::vector<Integer::value_type> res(out.size(), 0, resource);
        std::pmr::vector<Integer::value_type> self(out.size(), resource);
        std::copy(a.begin(), a.end(), self.begin());
        std::fill(out.begin(), out.end(), 0);

        auto res_size = 1zu;
        auto self_size = a.size();

        res[0] = 1;

        while (p) {
            if (p & 1) {
                auto sz = res_size + self_size;
                auto lhs = NumberSpan(res.data(), res_size);
                auto rhs = NumberSpan(self.data(), self_size);

                mul(out.slice(0, sz), lhs, rhs);
                res_size = sz;
                for (auto i = 0zu; i < sz; ++i) {
                    res[i] = out[i];
                    out[i] = 0;
                }
            }
            auto sz = self_size << 1;
            square(out.slice(0, sz), { self.data(), self_size });
            self_size = sz;
            for (auto i = 0zu; i < self_size; ++i) {
                self[i] = out[i];
                out[i] = 0;
            }
            p >>= 1;
        }

        std::copy(res.begin(), res.end(), out.begin());
    }

    template <std::integral T>
        requires std::is_unsigned_v<T>
    inline static constexpr auto pow(
        Integer& out,
        T p,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        if (p == 0) {
            out.resize(1);
            out.data()[0] = 1;
            return;
        } else if (p == 1) {
            return;
        }
        auto size = out.size();
        auto const bits = detail::pow_cal_size(size * 2, p) * MachineConfig::bits;
        out.resize<false>(bits);
        pow(out.to_span(), { out.data(), size }, p, resource);
        out.remove_trailing_empty_blocks();
    }

    template <std::integral T>
        requires std::is_unsigned_v<T>
    inline static constexpr auto pow(
        Integer& out,
        Integer const& in,
        T p
    ) -> void {
        if (p == 0) {
            out.resize(1);
            out.data()[0] = 1;
            return;
        } else if (p == 1) {
            out.resize(in.bits());
            std::copy_n(in.data(), in.size(), out.data());
            return;
        }
        auto const bits = detail::pow_cal_size(in.size() * 2, p) * MachineConfig::bits;
        out.resize<false>(bits);
        pow(out.to_span(), in.to_span(), p);
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_OPS_HPP
