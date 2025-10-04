#ifndef AMT_BIG_NUM_INTERNAL_OPS_HPP
#define AMT_BIG_NUM_INTERNAL_OPS_HPP

#include "integer.hpp"

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
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_OPS_HPP
