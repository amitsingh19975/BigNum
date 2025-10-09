#ifndef AMT_BIG_NUM_INTERNAL_INTEGER_PARSE_HPP
#define AMT_BIG_NUM_INTERNAL_INTEGER_PARSE_HPP

#include "base.hpp"
#include "number_span.hpp"
#include "utils.hpp"
#include "integer.hpp"
#include <algorithm>
#include <bit>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <expected>
#include <string_view>
#include <type_traits>
#include <vector>

namespace big_num::internal {

    inline static constexpr auto remove_trailing_zeros(
        std::pmr::vector<std::uint8_t>& v
    ) noexcept -> void {
        auto it = std::find_if_not(v.rbegin(), v.rend(), [](auto el) {
            return el == 0;
        });
        auto sz = v.size() - static_cast<std::size_t>(std::distance(v.rbegin(), it));
        v.resize(sz);
    }

    inline static constexpr auto fix_bits_required(Integer& v) noexcept -> void {
        v.resize(detail::calculate_bits_required(v));
    }

    inline static constexpr auto remove_trailing_zeros(
        Integer& v
    ) -> void {
        if (v.empty()) return;
        auto sz = v.size();
        auto ptr = v.data();
        for (auto i = sz; i > 0; --i) {
            auto j = i - 1;
            auto n = ptr[j];
            if (n) {
                auto last_bits = static_cast<std::size_t>(std::bit_width(n));
                auto const bits = j * MachineConfig::bits + last_bits;
                v.resize(bits);
                v.shrink_to_fit();
                return;
            }
        }
        v.resize(0);
        v.shrink_to_fit();
    }

    inline static constexpr auto remove_leading_zeros(
        Integer& v
    ) -> void {
        if (v.empty()) return;
        auto sz = v.size();
        auto ptr = v.data();
        auto i = 0ul;
        for (; i < sz; ++i) {
            if (ptr[i]) break;
        }
        if (i == sz) {
            v.resize(0);
            v.shrink_to_fit();
            return;
        }

        auto new_size = sz - i;
        std::copy_n(ptr + i, new_size, ptr);
        auto last_bits = static_cast<std::size_t>(std::bit_width(ptr[new_size - 1]));
        auto const bits = (new_size - 1) * MachineConfig::bits + last_bits;
        v.resize(bits);
        v.shrink_to_fit();
    }

    namespace detail {
        template <std::size_t Radix>
        inline static constexpr auto parse_integer_to_block_slow(
            std::span<MachineConfig::uint_t> out,
            std::span<std::uint8_t> in
        ) noexcept -> void {
            auto out_size = std::size_t{};
            using acc_t = typename Integer::acc_t;
            for (auto i = 0zu; i < in.size(); ++i) {
                auto c = static_cast<acc_t>(in[i]);
                auto j = 0zu;
                while (j <= out_size || c) {
                    auto o = static_cast<acc_t>(out[j]);
                    auto v = (o * Radix) + c;
                    out[j++] = static_cast<MachineConfig::uint_t>(v & MachineConfig::mask);
                    c = v >> MachineConfig::bits;
                }

                out_size = std::max(out_size, j);
            }
        }

        template <std::size_t Radix>
        inline static constexpr auto parse_integer_to_block(
            std::span<MachineConfig::uint_t> out,
            std::span<std::uint8_t> in,
            std::pmr::memory_resource* resource = std::pmr::get_default_resource()
        ) noexcept -> void {
            (void)resource;
            // auto const size = in.size();
            // if (size <= MachineConfig::parse_naive_threshold) {
                parse_integer_to_block_slow<Radix>(out, in);
            //     return;
            // }

            // auto const mid = size >> 1;
            //
            // auto lhs = std::span(in.data(), mid);
            // auto rhs = std::span(in.data() + mid, size - mid);
            //
            // auto olhs = std::span();
        }

        inline static constexpr auto normalize_string(std::string_view num, std::string& buf) -> std::string_view {
            auto need_cleaning = false;
            for (auto c : num) {
                if (c == '_' || c == ',' || c == ' ') {
                    need_cleaning = true;
                    break;
                }
            }
            if (!need_cleaning) return num;

            buf.reserve(num.size());

            for (auto c : num) {
                if (c == '_' || c == ',' || c == ' ') {
                    continue;
                }
                buf.push_back(c);
            }

            return buf;
        }
    } // namespace detail

    inline static constexpr auto parse_integer(
        Integer& out,
        std::string_view text,
        std::uint8_t radix_hint = 0, // 0 -> auto detect
        std::pmr::memory_resource* resource = std::pmr::get_default_resource()
    ) -> std::expected<void, std::string_view> {
        text = trim(text);
        auto buf = std::string{};
        text = detail::normalize_string(text, buf);
        // 0x -> hex
        // 0b -> binary
        // 0o -> oct
        // (-|+)?(0(x|b|o))?[a-fA-F0-9]+
        out.set_bits(0);
        out.set_neg(false);

        if (text.empty()) return {};
        switch (text[0]) {
            case '-': {
                out.set_neg(true);
                text = text.substr(1);
            } break;
            case '+': {
                text = text.substr(1);
            } break;
            default: break;
        }

        auto sz = text.size();
        std::pmr::vector<std::uint8_t> tmp(resource);

        if (text[0] == '0') {
            if (text.size() < 2) return {};

            switch (text[1]) {
                case 'x': case 'X': {
                    if (text.size() < 3) return std::unexpected("Missing number after radix prefix: '0x'");
                    if (radix_hint == 0) radix_hint = 16;
                    if (radix_hint != 16) return std::unexpected("Radix mismatch: expected radix to be base-16");
                    text = text.substr(2);
                    if (!validate_hex(text)) return std::unexpected("Invalid hexadecimal number");

                    sz = text.size();
                    tmp.resize(sz, 0);
                    for (auto i = 0zu; i < sz; ++i) {
                        auto c = text[i];
                        tmp[i] = digit_mapping[static_cast<std::size_t>(c)];
                    }
                } break;
                case 'b': case 'B': {
                    if (text.size() < 3) return std::unexpected("Missing number after radix prefix: '0b'");
                    if (radix_hint == 0) radix_hint = 2;
                    if (radix_hint != 2) return std::unexpected("Radix mismatch: expected radix to be base-2");
                    text = text.substr(2);
                    if (!validate_binary(text)) return std::unexpected("Invalid binary number");

                    sz = text.size();
                    tmp.resize(sz, 0);
                    for (auto i = 0zu; i < sz; ++i) {
                        auto c = text[i];
                        tmp[i] = digit_mapping[static_cast<std::size_t>(c)];
                    }
                } break;
                case 'o': case 'O': {
                    if (text.size() < 3) return std::unexpected("Missing number after radix prefix: '0o'");
                    if (radix_hint == 0) radix_hint = 8;
                    if (radix_hint != 8) return std::unexpected("Radix mismatch: expected radix to be base-8");
                    text = text.substr(2);
                    if (!validate_octal(text)) return std::unexpected("Invalid octal number");

                    sz = text.size();
                    tmp.resize(sz, 0);
                    for (auto i = 0zu; i < sz; ++i) {
                        auto c = text[i];
                        tmp[i] = digit_mapping[static_cast<std::size_t>(c)];
                    }
                } break;
                default: {
                    if (radix_hint == 0) radix_hint = 10;
                    if (radix_hint != 10) return std::unexpected("Radix mismatch: expected radix to be base-10");
                    if (!validate_decimal(text)) return std::unexpected("Invalid decimal number");

                    sz = text.size();
                    tmp.resize(sz, 0);
                    for (auto i = 0zu; i < sz; ++i) {
                        auto c = text[i];
                        tmp[i] = digit_mapping[static_cast<std::size_t>(c)];
                    }
                }
            }
        } else {
            if (radix_hint == 0) radix_hint = 10;
            if (radix_hint != 10) return std::unexpected("Radix mismatch: expected radix to be base-10");

            tmp.resize(sz, 0);
            for (auto i = 0zu; i < sz; ++i) {
                auto c = text[i];
                tmp[i] = digit_mapping[static_cast<std::size_t>(c)];
            }
        }

        out.resize(tmp.size() * MachineConfig::bits);
        out.fill(0);

        switch (radix_hint) {
            case 2: {
                detail::parse_integer_to_block<2>(out, tmp);
            } break;
            case 8: {
                detail::parse_integer_to_block<8>(out, tmp);
            } break;
            case 10: {
                detail::parse_integer_to_block<10>(out, tmp);
            } break;
            case 16: {
                detail::parse_integer_to_block<16>(out, tmp);
            } break;
        }

        remove_trailing_zeros(out);
        return {};
    }

    template <std::integral T>
    inline static constexpr auto parse_integer(
        Integer& out,
        T integer
    ) -> void {
        using acc_t = std::conditional_t<
            (sizeof(MachineConfig::acc_t) > sizeof(T)),
            MachineConfig::acc_t,
            std::make_unsigned_t<T>
        >;
        if (integer == 0) {
            out.resize(0);
            out.shrink_to_fit();
            return;
        }
        auto num = acc_t{};
        if constexpr (std::is_signed_v<T>) {
            out.set_neg(integer < 0);
            num = static_cast<acc_t>(std::abs(integer));
        } else {
            num = static_cast<acc_t>(integer);
        }
        auto bits_required = sizeof(T) * CHAR_BIT;
        out.resize(bits_required);
        auto j = 0zu;
        auto ptr = out.data();
        while (num) {
            auto q = static_cast<MachineConfig::uint_t>(num & MachineConfig::mask);
            if constexpr (MachineConfig::bits >= sizeof(T) * CHAR_BIT) {
                num = 0;
            } else {
                num = num >> MachineConfig::bits;
            }
            ptr[j++] = q;
        }

        remove_trailing_zeros(out);
    }

    namespace detail {
        template <std::size_t To>
            requires (To <= 16)
        inline static constexpr auto convert_to_string(
            std::span<MachineConfig::uint_t const> in,
            std::span<char> out
        ) noexcept -> void {
            using acc_t = MachineConfig::acc_t;
            auto out_size = std::size_t{};
            auto in_size = in.size();
            for (auto i = 0zu; i < in_size; ++i) {
                auto c = static_cast<acc_t>(in[in_size - i - 1]);
                auto j = 0zu;
                while (j <= out_size || c) {
                    auto o = static_cast<acc_t>(out[j]);
                    auto v = (o << MachineConfig::bits) + c;
                    out[j++] = static_cast<char>(v % To);
                    c = v / To;
                }
                out_size = std::max(out_size, j);
            }
        }
    } // namespace detail

    struct IntegerStringConvConfig {
        bool show_prefix            { false };
        bool show_separator         { false };
        std::string_view separator  { "_" };
        std::size_t group_size      { 0 /* auto select group size */ };
    };

    // if radix is 0, we print the underlying representation.
    inline static auto to_string(
        const_num_t const& in,
        std::uint8_t radix = 10,
        IntegerStringConvConfig config = {}
    ) -> std::string {
        std::string res;

        if (in.empty()) { 
            switch (radix) {
                case  2: res += "0b"; break;
                case  8: res += "0o"; break;
                case 10: break;
                case 16: res += "0x"; break;
                default: break;
            }
            res += '0';
            return res;
        }

        auto size = in.size();
        auto data = in.data();

        if (radix == 0) {
            res.reserve(size);
            for (auto i = 0zu; i < size; ++i) {
                res += std::to_string(data[size - i - 1]);
                if (i + 1 < size) {
                    if (config.show_separator) res += config.separator;
                }
            }
            return res; 
        }

        assert((radix == 2 || radix == 8 || radix == 10 || radix == 16) && "radix must be one of 2, 8, 10, or 16");

        auto const total_bits = size * MachineConfig::bits;

        std::size_t prefix_offset = 2zu * static_cast<std::size_t>(config.show_prefix) * static_cast<std::size_t>(radix != 10) + static_cast<std::size_t>(in.is_neg());
        res.resize(total_bits + prefix_offset, 0);
        auto res_ptr = res.data() + prefix_offset;
        auto res_size = res.size() - prefix_offset;
        auto res_span = std::span(res_ptr, res_size);
        auto start_index = 0zu;
        if (in.is_neg()) {
            start_index = 1;
            res[0] = '-';
        }
        switch (radix) {
            case 2: {
                if (config.group_size == 0) config.group_size = 8;
                if (config.show_prefix) { res[start_index] = '0'; res[start_index + 1] = 'b'; }
                detail::convert_to_string<2>(in, res_span);
            } break;
            case 8: {
                if (config.group_size == 0) config.group_size = 3;
                if (config.show_prefix) { res[start_index] = '0'; res[start_index + 1] = 'o'; }
                detail::convert_to_string<8>(in, res_span);
            } break;
            case 10: {
                if (config.group_size == 0) config.group_size = 3;
                detail::convert_to_string<10>(in, res_span);
            } break;
            case 16: {
                if (config.group_size == 0) config.group_size = 4;
                if (config.show_prefix) { res[start_index] = '0'; res[start_index + 1] = 'x'; }
                detail::convert_to_string<16>(in, res_span);
            } break;
            default: break;
        }

        auto res_start = res.begin() + static_cast<int>(prefix_offset);
        std::reverse(res_start, res.end());
        for (auto& c: res_span) {
            c = digit_to_char_mapping[static_cast<std::size_t>(c)];
        }
        auto it = std::find_if_not(res_start, res.end(), [](char c) { return c == '0'; });
        res.erase(res_start, it);

        if (config.show_separator) {
            auto sz = static_cast<std::ptrdiff_t>(std::max(res.size(), config.group_size - 1) - config.group_size + 1);
            for (auto i = sz - 1; i > static_cast<int>(prefix_offset); i -= config.group_size) {
                auto pos = static_cast<std::size_t>(i);
                res.insert(pos, config.separator);
            }
        }

        return res;
    }
} // big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_INTEGER_PARSE_HPP
