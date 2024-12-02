#include <print>
#include <catch2/catch_test_macros.hpp>
#include <big_num.hpp>
#include "bitwise_mock.hpp"

using namespace dark;
using namespace dark::internal;

TEST_CASE_METHOD(LeftShiftMock, "Right Shift Operations", "[logical_operations:right_shift]") {
	for (auto i = 0u; auto const& test: mock.tests) {
		INFO("Testing '" << i << "' test");
		REQUIRE(!test.input.empty());
		REQUIRE(!test.output.empty());
		auto num = BigInteger(test.input);

		INFO("Input '" << num.to_hex() << "'");
		
		REQUIRE(num.to_str() == test.input);
		auto shift = test.get_arg<std::size_t>("shift");
		auto res = num.shift_left(shift, /*should_extend=*/ true); // "num << shift" does the same but does not extend the number buffer
		REQUIRE(res.has_value());

		REQUIRE(*res == test.output);
		++i;
	}
}

TEST_CASE_METHOD(RightShiftMock, "Right Shift Operations", "[logical_operations:right_shift]") {
	for (auto i = 0u; auto const& test: mock.tests) {
		INFO("Testing '" << i << "' test");
		REQUIRE(!test.input.empty());
		REQUIRE(!test.output.empty());
		auto num = BigInteger(test.input);

		INFO("Input '" << num.to_hex() << "'");
		
		REQUIRE(num.to_str() == test.input);
		auto shift = test.get_arg<std::size_t>("shift");
		auto res = num.shift_right(shift);
		REQUIRE(res.has_value());

		REQUIRE(*res == test.output);
		++i;
	}
}
