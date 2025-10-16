#ifndef AMT_BIG_NUM_INTERNAL_MUL_NTT_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_NTT_HPP

#include "../integer.hpp"
#include "../base.hpp"
#include "../logical_bitwise.hpp"
#include "../number_span.hpp"
#include "../add_sub.hpp"
#include "ntt_params.hpp"

namespace big_num::internal {

    namespace detail {

        /**
         * GMP: `mpn_fft_mul_2exp_modF`
         * Calculating A * 2^d mod (2^(n * w) + 1).
         * A = [0, n + 1].
         * Assumption:
         *  1. A is semi-normalized: A[n] <= 1
         *  2. d < 2 * n * w
        */
        inline static constexpr auto fft_mul_2exp_modF(
            num_t out,
            const_num_t const& a,
            std::size_t d,
            std::size_t n
        ) noexcept -> void {
            assert(out.size() == n + 1);
            using val_t = num_t::value_type;
            auto sh = d % MachineConfig::bits;
            auto m = d / MachineConfig::bits;
            val_t cc, rd;

            auto tmp = out.slice(0, n);

            if (m >= n) {
                // case 1: n <= m
                // 2^(m * n * w + sh) mod (2^(n * w) + 1)
                // = (-1)^m * 2^sh mod (2^(n * w) + 1)
                m -= n;

                // In:
                // [++++++++++++++][+++++++|
                // |---(n-m-1)----]|-(n-1)-|
                //
                // Out:
                // [-------|[++++++++++++++]
                // O[0, m -1] = -1 /*wrapping*/ * -1^m * (A[n-m, n-1] << sh)
                // O[0, m -1] = -1 /*wrapping*/ * -1^1 * (A[n-m, n-1] << sh)
                // O[0, m -1] = (A[n-m, n-1] << sh)

                // O[m, n-1] = -1^m * (A[0, n-m-1] << sh)
                // O[m, n-1] = -1^1 * (A[0, n-m-1] << sh)
                // O[m, n-1] = -(A[0, n-m-1] << sh)

                // out[0..m-1] = left_shift(a[n - m]..a[n - 1], sh)
                // out[m..n-1] = -left_shift(a[0]..a[n-m-1], sh)

                auto lo = out.slice(0, m + 1);
                auto hi = out.slice(m);

                if (sh != 0) {
                    shift_left(lo, a.slice(n - m, m + 1), sh);
                    rd = out[m];
                    cc = shift_left(hi, a.slice(0, n - m), sh); 
                    ones_complement(hi);
                } else {
                    std::copy_n(a.data() + n - m, m, lo.begin());
                    rd = a[n];
                    std::copy_n(a.data(), n - m, hi.begin());
                    ones_complement(hi);
                    cc = 0;
                }

                // carry * 2^(nw) mod (2^(n * w) + 1)
                // = -1 * carry mod (2^(n * w) + 1)
                // = -carry mod (2^(n * w) + 1)
                // If we didn't wrapped the upper part, we would be adding
                // carry to it. After wrapping A[n-m, n-1] will be -A[n-m, n-1].
                // So, the final result will be `-A[n-m, n-1] - carry`.
                // Similarly, add `rd` to `out[m]` since the residue was generated
                // the lower part `A[n-m, n-1]`.

                out[n] = 0;
                cc += 1 /*1 for 2's complement*/;
                abs_add(tmp, cc);

                rd += 1 /*1 for 2's complement*/;
                // rd might overflow if MAX_VALUE + 1
                cc = (rd == 0) ? val_t{1} : rd;
                abs_add(tmp.slice(m + (rd == 0)), cc);
            } else {
                // O[0, m -1] = -1^m * (A[n-m, n-1] << sh)
                // O[0, m -1] = -1^1 * (A[n-m, n-1] << sh)
                // O[0, m -1] = -(A[n-m, n-1] << sh)

                // O[m, n-1] = -1 /*wrapping*/ * -1^m * (A[0, n-m-1] << sh)
                // O[m, n-1] = -1 /*wrapping*/ * -1^1 * (A[0, n-m-1] << sh)
                // O[m, n-1] = (A[0, n-m-1] << sh)

                auto lo = out.slice(0, m + 1);
                auto hi = out.slice(m);

                if (sh != 0) {
                    shift_left(lo, a.slice(n - m, m + 1), sh);
                    rd = out[m];
                    ones_complement(lo);
                    cc = shift_left(hi, a.slice(0, n - m), sh);
                } else {
                    std::copy_n(a.data() + n - m, m + 1, lo.begin());
                    ones_complement(lo);
                    rd = a[n];
                    std::copy_n(a.begin(), n - m, hi.begin());
                    cc = 0;
                }

                // if m == 0, there is no block shift except for bits `out[0] = a[n] << sh`
                if (m != 0) {
                    if (cc-- == 0) {
                        // 2's complement
                        cc = abs_add(tmp, 1);
                    }
                    cc = abs_sub(tmp.slice(0, m), cc);
                    // add 1 to cc to avoid overflow by add it to rd
                    cc += 1;
                }

                out[n] = -abs_sub(tmp.slice(m, n - m), cc);
                out[n] -= abs_sub(tmp.slice(m, n - m), rd);

                if (out[n] & MachineConfig::high_bit) {
                    out[n] = abs_add(tmp, 1);
                }
            }
        }
    } // namespace detail

} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_MUL_NTT_HPP
