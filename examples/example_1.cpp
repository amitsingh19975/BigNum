#include <print>
#include "big_num.hpp"
#include "big_num/basic_integer.hpp"

int main() {

    // mul: 0x1e49629297809a2a504978d7f7834d04839d3a13be7ec1a9625b4b26cd0f81fb109e63c50c11685312c257fa0698c6cd7e22e52
    auto lhs = dark::BigInteger("1512366075009453295483730155710103023");
    auto rhs = dark::BigInteger("256");
    auto res = lhs.div(rhs);
    auto [q, r] = res;

    std::println("{:0_b} / {} = {}, {}", lhs, rhs, q, r);
    return 0;
}
