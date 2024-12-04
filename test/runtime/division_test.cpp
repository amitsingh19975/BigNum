#include <print>
#include <catch2/catch_test_macros.hpp>
#include <big_num.hpp>

using namespace dark;
using namespace dark::internal;

TEST_CASE("Integer division", "[division:integer]") {
	SECTION("Long Division") {
		auto n = BigInteger("827394650391827364598273645982736459827364598273645982736459827364598273645982736459982");
		auto d = BigInteger("0x1234567890abcdef0123456789abdef");
		auto res = n.div(d, DivKind::LongDiv);

		auto [quot, rem] = res;
		REQUIRE(quot == "547086227378285774385535279880313068985597505218194");
		REQUIRE(rem == "908374728037675477649289214902459520");
	}

	SECTION("Newton–Raphson Division") {
		auto n = BigInteger("827394650391827364598273645982736459827364598273645982736459827364598273645982736459982");
		auto d = BigInteger("0x1234567890abcdef0123456789abdef");
		auto res = n.div(d, DivKind::NewtonRaphson);

		auto [quot, rem] = res;
		REQUIRE(quot == "547086227378285774385535279880313068985597505218194");
		REQUIRE(rem == "908374728037675477649289214902459520");
	}
}
