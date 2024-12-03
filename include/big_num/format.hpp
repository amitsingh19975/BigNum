#ifndef DARK_BIG_NUM_FORMAT_HPP
#define DARK_BIG_NUM_FORMAT_HPP

#include "type_traits.hpp"
#include "basic_integer.hpp"
#include <format>
#include <optional>

template <dark::internal::is_basic_integer T>
struct std::formatter<T> {
	dark::Radix radix{dark::Radix::Dec};
	std::optional<char> sep{};

	constexpr auto parse(auto& ctx) {
		auto it = ctx.begin();
		while (it != ctx.end() && *it != '{') {
			auto c = *it;
			if (c == '_' || c == ',') {
				sep = c;
				++it;
				break;
			}
			if (c != '0') break;

			++it;
			if (it == ctx.end()) {
				break;
			}

			if (*it == '_' || *it == ',') {
				sep = *it;
				++it;
				if (it == ctx.end()) break;
			}

			switch (*it) {
				case 'b': case 'B': {
					radix = dark::Radix::Binary;
					break;
				}
				case 'x': case 'X': {
					radix = dark::Radix::Hex;
					break;
				}
				case 'o': case 'O': {
					radix = dark::Radix::Octal;
					break;
				}
				default: break;
			} 
			++it;
		}

		return it;
	}

	auto format(T const& s, auto& ctx) const {
		return std::format_to(ctx.out(), "{}", s.to_str(radix, true, sep));
	}
};


#endif // DARK_BIG_NUM_FORMAT_HPP
