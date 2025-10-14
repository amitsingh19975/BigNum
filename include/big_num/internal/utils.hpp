#ifndef AMT_BIG_NUM_INTERNAL_UTILS_HPP
#define AMT_BIG_NUM_INTERNAL_UTILS_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <cctype>
#include <vector>

namespace big_num::internal {

    constexpr auto ltrim(std::string_view sv) noexcept -> std::string_view {
        auto it = std::find_if_not(sv.begin(), sv.end(), [](char ch) {
            return std::isspace(ch);
        });
        return sv.substr(static_cast<size_t>(it - sv.begin()));
    }

    constexpr auto rtrim(std::string_view sv) noexcept -> std::string_view {
        auto it = std::find_if_not(sv.rbegin(), sv.rend(), [](char ch) {
            return std::isspace(ch);
        });
        return sv.substr(0, static_cast<size_t>(sv.rend() - it));
    }

    constexpr auto trim(std::string_view sv) noexcept -> std::string_view {
        return rtrim(ltrim(sv));
    }

    static constexpr auto digit_mapping = []{
        std::array<std::uint8_t, 256> res{};

        for (auto i = '0'; i <= '9'; ++i) {
            res[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i - '0');
        }

        for (auto i = 'a'; i <= 'f'; ++i) {
            res[static_cast<std::size_t>(i)] = 10 + static_cast<std::uint8_t>(i - 'a');
        }

        for (auto i = 'A'; i <= 'F'; ++i) {
            res[static_cast<std::size_t>(i)] = 10 + static_cast<std::uint8_t>(i - 'A');
        }
        return res;
    }();

    static constexpr auto digit_to_char_mapping = []{
        std::array<char, 16> res{};

        for (auto i = 0zu; i < 10zu; ++i) {
            res[i] = static_cast<char>(i + '0');
        }
        for (auto i = 0zu; i < 6zu; ++i) {
            res[10 + i] = static_cast<char>(i + 'a');
        }
        return res;
    }();

    constexpr auto validate_hex(std::string_view s) noexcept -> bool {
        return std::all_of(s.begin(), s.end(), [](char c) {
            return std::isxdigit(c);
        });
    }

    constexpr auto validate_decimal(std::string_view s) noexcept -> bool {
        return std::all_of(s.begin(), s.end(), [](char c) {
            return std::isdigit(c);
        });
    }

    constexpr auto validate_binary(std::string_view s) noexcept -> bool {
        return std::all_of(s.begin(), s.end(), [](char c) {
            return c == '0' || c == '1';
        });
    }

    constexpr auto validate_octal(std::string_view s) noexcept -> bool {
        return std::all_of(s.begin(), s.end(), [](char c) {
            return c >= '0' && c < '8';
        });
    }

    constexpr auto nearest_power_of_2(std::size_t num) noexcept -> std::size_t {
        if ((num & (num - 1)) == 0) return num;
        --num;
        num |= (num >> 1);
        num |= (num >> 2);
        num |= (num >> 4);
        num |= (num >> 8);
        num |= (num >> 16);
        num |= (num >> 32);
        ++num;
        return num;
    }

    inline static constexpr auto remove_trailing_zeros(
        std::pmr::vector<std::uint8_t>& v
    ) noexcept -> void {
        auto it = std::find_if_not(v.rbegin(), v.rend(), [](auto el) {
            return el == 0;
        });
        auto sz = v.size() - static_cast<std::size_t>(std::distance(v.rbegin(), it));
        v.resize(sz);
    }
} // big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_UTILS_HPP
