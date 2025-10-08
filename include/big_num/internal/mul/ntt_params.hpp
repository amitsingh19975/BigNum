#ifndef AMT_BIG_NUM_INTERNAL_MUL_NTT_PARAMS_HPP
#define AMT_BIG_NUM_INTERNAL_MUL_NTT_PARAMS_HPP

#include <array>

namespace big_num::internal::params {
    namespace ntt {
        struct Param {
            unsigned n;
            unsigned k;
        };
    } // namespace ntt
    static constexpr std::array ntt_params = {
        ntt::Param { .n = 1, .k = 2 },
    };
} // namespace big_num::internal::params

#endif // AMT_BIG_NUM_INTERNAL_MUL_NTT_PARAMS_HPP
