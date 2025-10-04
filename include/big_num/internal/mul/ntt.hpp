#ifndef AMT_BIG_NUM_INTERNAL_MUL_NTT_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_NTT_HPP

#include "../integer.hpp"
#include "../base.hpp"

namespace big_num::internal {

    namespace detail {
        template <MachineConfig::uint_t Mod>
        inline static constexpr auto binary_pow(
            MachineConfig::uint_t num,
            MachineConfig::uint_t pow
        ) noexcept -> MachineConfig::acc_t {
            using acc_t = MachineConfig::acc_t;

            acc_t res{1};
            while (pow) {
                if (pow & 1) res = (res * num) % Mod;
                num = (num * num) % Mod;
                pow >>= 1;
            }

            return res;
        }
    } // namespace detail

} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_MUL_NTT_HPP
