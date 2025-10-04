#ifndef AMT_BIG_NUM_INTERNAL_BASE_HPP
#define AMT_BIG_NUM_INTERNAL_BASE_HPP

#include <climits>
#include <cstddef>
#include <cstdint>
#include "ui.hpp"

namespace big_num::internal {

    struct MachineConfig {
        #if INTPTR_MAX == INT32_MAX
            using uint_t = std::uint16_t;
            using int_t = std::int16_t;
            using acc_t = std::uint32_t;
            using iacc_t = std::int32_t;
            using simd_uint_t = ui::native::u16;
        #elif INTPTR_MAX == INT64_MAX
            using uint_t = std::uint32_t;
            using int_t = std::int32_t;
            using acc_t = std::uint64_t;
            using iacc_t = std::int64_t;
            using simd_uint_t = ui::native::u32;
        #else
            #error "Environment not 32 or 64-bit."
        #endif
        using simd_acc_t = ui::Vec<simd_uint_t::elements, acc_t>;
        using mask_t = simd_uint_t;
        static constexpr std::size_t bytes = sizeof(uint_t);
        static constexpr std::size_t total_bits = (bytes * CHAR_BIT);
        static constexpr std::size_t bits = total_bits - 1;

        static constexpr acc_t max = acc_t{1} << bits;
        static constexpr acc_t mask = max - 1;

        static constexpr auto size(std::size_t b) noexcept -> std::size_t {
            return (b + bits - 1) / bits;
        }

        template <std::size_t N>
        static constexpr auto align_up(std::size_t n) noexcept -> std::size_t {
            return (n + N - 1) & ~(N - 1);
        }

        template <std::size_t N>
        static constexpr auto align_down(std::size_t n) noexcept -> std::size_t {
            return n & ~(N - 1);
        }

        static constexpr auto nearest_even_number(std::size_t num) noexcept -> std::size_t {
            return num + (num & 1);
        }


        #ifndef BIG_NUM_NAIVE_MUL_THRESHOLD
        static constexpr std::size_t naive_mul_threshold = 32zu;
        #else
        static constexpr std::size_t naive_mul_threshold = nearest_even_number(BIG_NUM_NAIVE_MUL_THRESHOLD);
        #endif

        #ifndef BIG_NUM_KARATSUBA_THRESHOLD
        static constexpr std::size_t karatsuba_threshold = 512zu;
        #else
        static constexpr std::size_t karatsuba_threshold = nearest_even_number(BIG_NUM_KARATSUBA_THRESHOLD);
        #endif

        #ifndef BIG_NUM_TOOM_COOK_3_THRESHOLD
        static constexpr std::size_t toom_cook_3_threshold = 1024zu;
        #else
        static constexpr std::size_t toom_cook_3_threshold = nearest_even_number(BIG_NUM_TOOM_COOK_3_THRESHOLD);
        #endif

        #ifndef BIG_NUM_PARSE_NAIVE_THRESHOLD
        static constexpr std::size_t parse_naive_threshold = 2'000zu;
        #else
        static constexpr std::size_t parse_naive_threshold = nearest_even_number(BIG_NUM_PARSE_NAIVE_THRESHOLD);
        #endif

        #ifndef BIG_NUM_PARSE_DIVIDE_CONQUER_THRESHOLD
        static constexpr std::size_t parse_dc_threshold = 100'000zu;
        #else
        static constexpr std::size_t parse_dc_threshold = nearest_even_number(BIG_NUM_PARSE_DIVIDE_CONQUER_THRESHOLD);
        #endif
    };

} // big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_BASE_HPP
