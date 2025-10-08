#ifndef AMT_BIG_NUM_INTERNAL_MUL_MUL_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_MUL_HPP

#include "../base.hpp"
#include "../integer.hpp"
#include "toom_cook.hpp"
#include "karatsuba.hpp"
#include "naive.hpp"
#include <span>

namespace big_num::internal {

    inline static constexpr auto mul(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        auto const size = std::max(lhs.size(), rhs.size());
        if (lhs.size() < 2 || rhs.size() < 2) {
            naive_mul_scalar(out, lhs, rhs);
            return;
        }

        if (size <= MachineConfig::naive_mul_threshold) {
            naive_mul(out, lhs, rhs);
        } else if (size <= MachineConfig::karatsuba_threshold) {
            karatsuba_mul(out, lhs, rhs, resource);
        } else if (size <= MachineConfig::toom_cook_3_threshold) {
            toom_cook_3(out, lhs, rhs);
        } else {
            toom_cook_3(out, lhs, rhs);
        }
    }

    inline static constexpr auto sqaure(
        Integer& out,
        Integer const& a,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        auto const size = a.size();
        if (size < 2) {
            naive_mul_scalar(out, a, a);
            return;
        }

        if (size <= MachineConfig::naive_mul_threshold) {
            naive_square(out, a);
        } else if (size <= MachineConfig::karatsuba_threshold) {
            karatsuba_square(out, a, resource);
        } else if (size <= MachineConfig::toom_cook_3_threshold) {
            toom_cook_3_square(out, a);
        } else {
            toom_cook_3_square(out, a);
        }
    }

    inline static constexpr auto mul(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        auto const size = std::max(lhs.size(), rhs.size());
        if (lhs.size() < 2 || rhs.size() < 2) {
            naive_mul(out, lhs, rhs);
            return;
        }

        if (size <= MachineConfig::naive_mul_threshold) {
            naive_mul(out, lhs, rhs);
        } else if (size <= MachineConfig::karatsuba_threshold) {
            karatsuba_mul(out, lhs, rhs, resource);
        } else if (size <= MachineConfig::toom_cook_3_threshold) {
            toom_cook_3(out, lhs, rhs, resource);
        } else {
            toom_cook_3(out, lhs, rhs, resource);
        }
    }

    inline static constexpr auto square(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> a,
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> void {
        auto const size = std::max(a.size(), a.size());
        if (a.size() < 2) {
            naive_mul(out, a, a);
            return;
        }

        if (size <= MachineConfig::naive_mul_threshold) {
            naive_square(out, a);
        } else if (size <= MachineConfig::karatsuba_threshold) {
            karatsuba_square(out, a, resource);
        } else if (size <= MachineConfig::toom_cook_3_threshold) {
            toom_cook_3_square(out, a, resource);
        } else {
            toom_cook_3_square(out, a, resource);
        }
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_MUL_MUL_HPP
