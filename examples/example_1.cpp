#include <iostream>
#include "big_num.hpp"
#include "big_num/basic_integer.hpp"

int main() {

    // mul: 0x1e49629297809a2a504978d7f7834d04839d3a13be7ec1a9625b4b26cd0f81fb109e63c50c11685312c257fa0698c6cd7e22e52
    auto lhs = dark::BigInteger("827394650391827364598273645982736459827364598273645982736459827364598273645982736459982");
    auto rhs = dark::BigInteger("0x1234567890abcdef0123456789abdef");

    std::cout << lhs.to_str(dark::Radix::Dec) << " - " << rhs << "\n = " << (lhs * rhs) << '\n';
    return 0;
}
