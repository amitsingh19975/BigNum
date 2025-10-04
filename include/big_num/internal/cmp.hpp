#ifndef AMT_BIG_NUM_INTERNAL_CMP_HPP
#define AMT_BIG_NUM_INTERNAL_CMP_HPP

#include "base.hpp"
#include "integer_parse.hpp"
#include "integer.hpp"
#include "ui.hpp"
#include <type_traits>

namespace big_num::internal {
    namespace detail {
        template <std::size_t K>
            requires (K < 3)
        inline static constexpr auto merge_helper(Integer const& a) noexcept -> MachineConfig::iacc_t {
            using acc_t = MachineConfig::iacc_t; 
            auto ptr = a.data();
            auto neg = a.is_neg();
            if constexpr (K == 1) {
                return acc_t(ptr[0]) * (neg ? acc_t{-1} : acc_t{1});
            } else if constexpr (K == 2) {
                auto l = static_cast<acc_t>(ptr[0]);
                auto r = static_cast<acc_t>(ptr[1]);
                acc_t sign = neg ? -1 : 1;
                return (l + (r << MachineConfig::bits)) * sign;
            } else {
                return 0;
            }
        }

        template <typename T>
        inline static constexpr auto equal_small(
            Integer const& lhs,
            Integer const& rhs,
            T&& op = std::equal_to<>{}
        ) noexcept -> bool {
            auto lsz = lhs.size();
            auto rsz = rhs.size();
            switch (lsz) {
                case 0: {
                    auto a = merge_helper<0>(lhs);
                    switch (rsz) {
                        case 0: {
                            auto b = merge_helper<0>(rhs);
                            return op(a, b);
                        }
                        case 1: {
                            auto b = merge_helper<1>(rhs);
                            return op(a, b);
                        }
                        case 2: {
                            auto b = merge_helper<2>(rhs);
                            return op(a, b);
                        }
                        default: return false;
                    }
                }
                case 1: {
                    auto a = merge_helper<1>(lhs);
                    switch (rsz) {
                        case 0: {
                            auto b = merge_helper<0>(rhs);
                            return op(a, b);
                        }
                        case 1: {
                            auto b = merge_helper<1>(rhs);
                            return op(a, b);
                        }
                        case 2: {
                            auto b = merge_helper<2>(rhs);
                            return op(a, b);
                        }
                        default: return false;
                    }
                }
                case 2: {
                    auto a = merge_helper<2>(lhs);
                    switch (rsz) {
                        case 0: {
                            auto b = merge_helper<0>(rhs);
                            return op(a, b);
                        }
                        case 1: {
                            auto b = merge_helper<1>(rhs);
                            return op(a, b);
                        }
                        case 2: {
                            auto b = merge_helper<2>(rhs);
                            return op(a, b);
                        }
                        default: return false;
                    }
                }
                default: return false;
            }
        }
    } // namespace detail

    inline static constexpr auto abs_equal(
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> bool {
        if (lhs.bits() < MachineConfig::total_bits * 2 && rhs.bits() < MachineConfig::total_bits * 2) {
            return detail::equal_small(lhs, rhs, std::equal_to<>{});
        }
        if (lhs.bits() != rhs.bits()) return false;
        if (lhs.bits() == 0) return true;
        using simd_t = MachineConfig::simd_uint_t; 
        static constexpr auto N = simd_t::elements;

        auto a = lhs.data();
        auto b = rhs.data();
        auto asz = lhs.size();
        auto sz = MachineConfig::align_up<N>(asz);

        auto i = std::size_t{};

        if (!std::is_constant_evaluated()) {
            auto nsz = sz - sz % N;
            for (; i < nsz; i += N) {
                auto l = simd_t::load(a + i, N);
                auto r = simd_t::load(b + i, N);
                auto mask = ui::IntMask(ui::rcast<std::uint8_t>(l == r));
                if (!mask.all()) return false;
            }
        }

        for (; i < asz; ++i) {
            auto tl = a[i];
            auto tr = b[i];
            if (tl != tr) return false;
        }

        return true;
    }

    inline static constexpr auto abs_equal(
        std::span<Integer::value_type const> lhs,
        std::size_t lb, // left bits
        std::span<Integer::value_type const> rhs,
        std::size_t rb // right bits
    ) noexcept -> bool {
        auto a = Integer::from(lhs, lb);
        auto b = Integer::from(rhs, rb);
        return abs_equal(a, b);
    }

    inline static constexpr auto abs_equal(
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) noexcept -> bool {
        auto lb = calculate_bits_required(lhs);
        auto rb = calculate_bits_required(rhs);
        return abs_equal(lhs, lb, rhs, rb);
    }

    inline static constexpr auto equal(
        std::span<Integer::value_type const> lhs,
        std::size_t lb, // left bits
        std::span<Integer::value_type const> rhs,
        std::size_t rb // right bits
    ) noexcept -> bool {
        return abs_equal(lhs, lb, rhs, rb);
    }

    inline static constexpr auto equal(
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) noexcept -> bool {
        return abs_equal(lhs, rhs);
    }

    inline static constexpr auto equal(
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> bool {
        if (lhs.is_neg() != rhs.is_neg()) return false;
        return equal(lhs, rhs);
    }

    // lhs < rhs
    template <bool IsAbs = true>
    inline static constexpr auto abs_less(
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> bool {
        if (lhs.bits() < MachineConfig::total_bits * 2 && rhs.bits() < MachineConfig::total_bits * 2) {
            return detail::equal_small(lhs, rhs, std::less<>{});
        }
        // => -lhs[32bits] < -rhs[8bits]
        // =>  lhs[32bits] >  rhs[8bits]
        if constexpr (IsAbs) {
            if (lhs.bits() < rhs.bits()) return true;
            else if (lhs.bits() > rhs.bits()) return false;
        } else {
            if (lhs.bits() < rhs.bits()) return !lhs.is_neg();
            else if (lhs.bits() > rhs.bits()) return lhs.is_neg();
        }

        // using uint_t = MachineConfig::uint_t;
        using simd_t = MachineConfig::simd_uint_t; 
        static constexpr auto N = simd_t::elements;

        auto a = lhs.data();
        auto b = rhs.data();
        auto asz = lhs.size();
        auto sz = MachineConfig::align_up<N>(asz);

        // lhs < rhs
        // -lhs < -rhs => lhs > rhs
        auto i = std::size_t{};
        if (!std::is_constant_evaluated()) {
            auto nsz = sz - sz % N;
            for (; i < nsz; i += N) {
                auto l = simd_t::load(a + i, N);
                auto r = simd_t::load(b + i, N);
                auto mask = ui::IntMask(ui::rcast<std::uint8_t>(l == r));
                if (mask.all()) continue;
                for (auto k = 0ul; k < N; ++k) {
                    auto tl = a[i + k];
                    auto tr = b[i + k];
                    if (tl == tr) continue;
                    if (lhs.is_neg()) return tl > tr;
                    else return tl < tr;
                }
            }
        }

        for (; i < asz; ++i) {
            auto tl = a[i];
            auto tr = b[i];
            if (tl == tr) continue;
            if (lhs.is_neg()) return tl > tr;
            else return tl < tr;
        }
        return false;
    }

    inline static constexpr auto abs_less(
        std::span<Integer::value_type const> lhs,
        std::size_t lb, // left bits
        std::span<Integer::value_type const> rhs,
        std::size_t rb // right bits
    ) noexcept -> bool {
        auto a = Integer::from(lhs, lb);
        auto b = Integer::from(rhs, rb);
        return abs_less<true>(a, b);
    }

    inline static constexpr auto abs_less(
       std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) noexcept -> bool {
        auto lb = calculate_bits_required(lhs);
        auto rb = calculate_bits_required(rhs);
        return abs_less(lhs, lb, rhs, rb);
    }


    inline static constexpr auto less(
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> bool {
        // -lhs < rhs => true
        if (lhs.is_neg() && !rhs.is_neg()) return true;
        // lhs < -rhs => false
        if (!lhs.is_neg() && rhs.is_neg()) return false;

        return abs_less<false>(lhs, rhs);
    }

    inline static constexpr auto less(
        std::span<Integer::value_type const> lhs,
        std::size_t lb, // left bits
        std::span<Integer::value_type const> rhs,
        std::size_t rb // right bits
    ) noexcept -> bool {
        return abs_less(lhs, lb, rhs, rb);
    }

    inline static constexpr auto less(
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) noexcept -> bool {
        return abs_less(lhs, rhs);
    }

    // lhs <= rhs
    template <bool IsAbs = true>
    inline static constexpr auto abs_less_equal(
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> bool {
        if (lhs.bits() < MachineConfig::total_bits * 2 && rhs.bits() < MachineConfig::total_bits * 2) {
            return detail::equal_small(lhs, rhs, std::less_equal<>{});
        }

        // => -lhs[32bits] < -rhs[8bits]
        // =>  lhs[32bits] >  rhs[8bits]
        if constexpr (IsAbs) {
            if (lhs.bits() < rhs.bits()) return true;
            else if (lhs.bits() > rhs.bits()) return false;
        } else {
            if (lhs.bits() < rhs.bits()) return !lhs.is_neg();
            else if (lhs.bits() > rhs.bits()) return lhs.is_neg();
        }

        // using uint_t = MachineConfig::uint_t;
        using simd_t = MachineConfig::simd_uint_t; 
        static constexpr auto N = simd_t::elements;

        auto a = lhs.data();
        auto b = rhs.data();
        auto asz = lhs.size();
        auto sz = MachineConfig::align_up<N>(asz);

        auto i = std::size_t{};
        if (!std::is_constant_evaluated()) {
            auto nsz = sz - sz % N;
            for (; i < nsz; i += N) {
                auto l = simd_t::load(a + i, N);
                auto r = simd_t::load(b + i, N);
                auto mask = ui::IntMask(ui::rcast<std::uint8_t>(l == r));
                if (mask.all()) continue;
                for (auto k = 0ul; k < N; ++k) {
                    auto tl = a[i + k];
                    auto tr = b[i + k];
                    if (tl == tr) continue;
                    if (lhs.is_neg()) return tl > tr;
                    else return tl < tr;
                }
            }
        }

        for (; i < asz; ++i) {
            auto tl = a[i];
            auto tr = b[i];
            if (tl == tr) continue;
            if (lhs.is_neg()) return tl > tr;
            else return tl < tr;
        }
        return true;
    }

    inline static constexpr auto abs_less_equal(
        std::span<Integer::value_type const> lhs,
        std::size_t lb, // left bits
        std::span<Integer::value_type const> rhs,
        std::size_t rb // right bits
    ) noexcept -> bool {
        auto a = Integer::from(lhs, lb);
        auto b = Integer::from(rhs, rb);
        return abs_less_equal(a, b);
    }

    inline static constexpr auto abs_less_equal(
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) noexcept -> bool {
        auto lb = calculate_bits_required(lhs);
        auto rb = calculate_bits_required(rhs);
        return abs_less_equal(lhs, lb, rhs, rb);
    }


    inline static constexpr auto less_equal(
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> bool {
        // -lhs < rhs => true
        if (lhs.is_neg() && !rhs.is_neg()) return true;
        // lhs < -rhs => false
        if (!lhs.is_neg() && rhs.is_neg()) return false;

        return abs_less_equal<false>(lhs, rhs);
    }

    inline static constexpr auto less_equal(
        std::span<Integer::value_type const> lhs,
        std::size_t lb, // left bits
        std::span<Integer::value_type const> rhs,
        std::size_t rb // right bits
    ) noexcept -> bool {
        return abs_less_equal(lhs, lb, rhs, rb);
    }

    inline static constexpr auto less_equal(
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) noexcept -> bool {
        return abs_less_equal(lhs, rhs);
    }

    // lhs > rhs
    inline static constexpr auto abs_greater(
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> bool {
        return !abs_less_equal(lhs, rhs);
    }

    inline static constexpr auto abs_greater(
        std::span<Integer::value_type const> lhs,
        std::size_t lb, // left bits
        std::span<Integer::value_type const> rhs,
        std::size_t rb // right bits
    ) noexcept -> bool {
        return !abs_less_equal(lhs, lb, rhs, rb);
    }

    inline static constexpr auto abs_greater(
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) noexcept -> bool {
        return !abs_less_equal(lhs, rhs);
    }

    inline static constexpr auto greater(
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> bool {
        return !less_equal(lhs, rhs);
    }

    inline static constexpr auto greater(
        std::span<Integer::value_type const> lhs,
        std::size_t lb, // left bits
        std::span<Integer::value_type const> rhs,
        std::size_t rb // right bits
    ) noexcept -> bool {
        return !less_equal(lhs, lb, rhs, rb);
    }

    inline static constexpr auto greater(
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) noexcept -> bool {
        return !less_equal(lhs, rhs);
    }

    // lhs > rhs
    inline static constexpr auto abs_greater_equal(
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> bool {
        return !abs_less(lhs, rhs);
    }

    inline static constexpr auto abs_greater_equal(
        std::span<Integer::value_type const> lhs,
        std::size_t lb, // left bits
        std::span<Integer::value_type const> rhs,
        std::size_t rb // right bits
    ) noexcept -> bool {
        return !abs_less(lhs, lb, rhs, rb);
    }

    inline static constexpr auto abs_greater_equal(
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) noexcept -> bool {
        return !abs_less(lhs, rhs);
    }

    inline static constexpr auto greater_equal(
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> bool {
        return !less(lhs, rhs);
    }

    inline static constexpr auto greater_equal(
        std::span<Integer::value_type const> lhs,
        std::size_t lb, // left bits
        std::span<Integer::value_type const> rhs,
        std::size_t rb // right bits
    ) noexcept -> bool {
        return !less(lhs, lb, rhs, rb);
    }

    inline static constexpr auto greater_equal(
        std::span<Integer::value_type const> lhs,
        std::span<Integer::value_type const> rhs
    ) noexcept -> bool {
        return !less(lhs, rhs);
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_CMP_HPP
