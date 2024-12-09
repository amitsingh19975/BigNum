#ifndef DARK_BIG_NUM_UTILS_HPP
#define DARK_BIG_NUM_UTILS_HPP

#include "block_info.hpp"
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <array>
#include <type_traits>

namespace dark::utils {

	static constexpr auto radix_mapping = [](){
		std::array<std::int8_t, 256> res{};
		for (auto i = '0'; i <= '9'; ++i) res[static_cast<std::size_t>(i)] = static_cast<std::int8_t>(i - '0');
		for (auto i = 'a'; i <= 'f'; ++i) res[static_cast<std::size_t>(i)] = 10 + static_cast<std::int8_t>(i - 'a');
		for (auto i = 'A'; i <= 'F'; ++i) res[static_cast<std::size_t>(i)] = 10 + static_cast<std::int8_t>(i - 'A');
		return res;
	}();

	static constexpr std::string_view number_to_hex_char_mapping = "0123456789abcdef"; 

	inline static constexpr auto trim_leading_zero(std::string_view num) noexcept {
		auto i = 0zu;
		for (; i < num.size(); ++i) {
			auto c = num[i];
			if (!(c == '0' || c == 0)) break;
		}
		return num.substr(i);
	}	

	inline static constexpr auto trim_trailing_zero(std::string_view num) noexcept {
		auto end = num.size();
		while (end > 0) {
			auto c = num[end - 1];
			if (!(c == '0' || c == 0)) break;
			--end;
		}
		return num.substr(0, end);
	}	

	template <std::size_t To, typename T>
		requires std::is_arithmetic_v<T>
	inline static constexpr auto basic_convert_to_block_radix(
		T* out,
		std::size_t from_radix,
		std::size_t size,
		auto&& get_digit
	) noexcept -> std::size_t {
		auto output_size = std::size_t{};
		using acc_t = internal::BlockInfo::accumulator_t;
		
		for (auto i = 0ul; i < size; ++i){
			auto c = get_digit(i);
			auto carry_over = static_cast<acc_t>(c);

			auto j = 0zu;
			while (j <= output_size || carry_over) {
				auto o = static_cast<acc_t>(out[j]);
				acc_t temp = (o * from_radix) + carry_over;
				out[j++] = static_cast<T>(temp % To);
				carry_over = temp / To;
			}
			output_size = std::max(j, output_size);
		}
		return output_size + 1;
	}

	inline static constexpr auto convert_to_block_radix(
		internal::BlockInfo::type* out,
		std::string_view num,
		std::size_t from_radix
	) noexcept -> std::size_t {
		return basic_convert_to_block_radix<internal::BlockInfo::max_value>(
			out,
			from_radix,
			num.size(),
			[&num](std::size_t i) {
				return static_cast<internal::BlockInfo::accumulator_t>(radix_mapping[static_cast<std::size_t>(num[i])]); 
			});
	}

	inline static constexpr auto convert_to_decimal_from_hex(
		std::string& out,
		std::string_view num
	) noexcept -> std::size_t {
		return basic_convert_to_block_radix<10>(
			out.data(),
			16,
			num.size(),
			[&num](std::size_t i) {
				return static_cast<internal::BlockInfo::accumulator_t>(radix_mapping[static_cast<std::size_t>(num[i])]); 
			});
	}

	template <std::integral T>
	inline static constexpr auto count_nibles(T num) noexcept -> std::size_t {
		std::size_t c{};
	
		while (num) {
			++c;
			num >>= 4;
		}

		return c;
	}
} // namespace dark::utils

#endif // DARK_BIG_NUM_UTILS_HPP
