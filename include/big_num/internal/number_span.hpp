#ifndef AMT_BIG_NUM_INTERNAL_NUMBER_SPAN_HPP
#define AMT_BIG_NUM_INTERNAL_NUMBER_SPAN_HPP

#include "base.hpp"
#include <algorithm>
#include <limits>
#include <span>
#include <type_traits>

namespace big_num::internal {

    namespace detail {
        template <typename T>
        struct is_std_span: std::false_type {};

        template <typename T>
        struct is_std_span<std::span<T>>: std::true_type {};

        template <typename T> 
        concept is_convertible_to_span = requires(T const& v) {
            { std::span(v) };
        } && !is_std_span<T>::value;

        inline static constexpr auto calculate_bits_required(
            std::span<MachineConfig::uint_t const> v
        ) noexcept -> std::size_t {
            if (v.empty()) return 0;
            auto sz = v.size();
            auto ptr = v.data();
            auto i = sz;
            for (; i > 0; --i) {
                if (ptr[i - 1]) break;
            }
            if (i == 0) return 0;

            auto blocks = i - 1;
            auto bits = blocks * MachineConfig::bits;
            return bits + static_cast<std::size_t>(std::bit_width(ptr[blocks]));
        }
    } // namespace detail

    template <typename T>
    struct NumberSpan {
        using base_type = std::span<T>;
        using value_type = typename base_type::value_type;
        using size_type = typename base_type::size_type;
        using difference_type = typename base_type::difference_type;
        using reference = typename base_type::reference;
        using const_reference = typename base_type::const_reference;
        using pointer = typename base_type::pointer;
        using const_pointer = typename base_type::const_pointer;
        using iterator = typename base_type::iterator;
        using reverse_iterator = typename base_type::reverse_iterator;

        static constexpr auto npos = std::numeric_limits<size_type>::max();

        constexpr NumberSpan() noexcept = default;
        constexpr NumberSpan(NumberSpan const&) noexcept = default;
        constexpr NumberSpan(NumberSpan &&) noexcept = default;
        constexpr NumberSpan& operator=(NumberSpan const&) noexcept = default;
        constexpr NumberSpan& operator=(NumberSpan &&) noexcept = default;
        constexpr ~NumberSpan() noexcept = default;

        constexpr NumberSpan(T* data, size_type size, bool is_neg = false, size_type bits = npos) noexcept
            : m_base(data, size)
            , m_is_neg(is_neg)
            , m_bits(bits)
        {}

        constexpr NumberSpan(base_type base, bool is_neg = false, size_type bits = npos) noexcept
            : m_base(base)
            , m_is_neg(is_neg)
            , m_bits(bits)
        {}

        // constexpr NumberSpan(NumberSpan<std::add_const_t<T>> const& other) noexcept requires (std::is_const_v<T>)
        //     : m_base(other.m_base)
        //     , m_is_neg(other.is_neg())
        //     , m_bits(other.bits())
        // {}

        constexpr auto span() const noexcept -> std::span<std::add_const_t<value_type>> { return m_base; }
        constexpr auto span() noexcept -> base_type requires (!std::is_const_v<T>) { return m_base; }
        constexpr auto size() const noexcept -> size_type { return m_base.size(); }
        constexpr auto empty() const noexcept -> bool { return m_base.empty(); }
        constexpr auto set_neg(bool n) noexcept -> void { m_is_neg = n; }
        constexpr auto is_neg() const noexcept -> bool { return m_is_neg; }
        template <typename U>
        constexpr auto copy_sign(NumberSpan<U> const& n) noexcept -> void { m_is_neg = n.is_neg(); }
        constexpr operator std::span<std::add_const_t<value_type>>() const noexcept {
            return m_base;
        }
        constexpr operator std::span<value_type>() noexcept requires (!std::is_const_v<T>) {
            return m_base;
        }

        constexpr auto bits() const noexcept -> size_type {
            if (m_bits == npos) return m_bits = detail::calculate_bits_required(m_base);
            return m_bits;
        }

        constexpr auto begin() noexcept -> iterator { return m_base.begin(); }
        constexpr auto end() noexcept -> iterator { return m_base.end(); }
        constexpr auto rbegin() noexcept -> reverse_iterator { return m_base.rbegin(); }
        constexpr auto rend() noexcept -> reverse_iterator { return m_base.rend(); }

        constexpr auto begin() const noexcept -> iterator { return m_base.begin(); }
        constexpr auto end() const noexcept -> iterator { return m_base.end(); }
        constexpr auto rbegin() const noexcept -> reverse_iterator { return m_base.rbegin(); }
        constexpr auto rend() const noexcept -> reverse_iterator { return m_base.rend(); }

        constexpr auto data() noexcept -> pointer { return m_base.data(); }
        constexpr auto data() const noexcept -> const_pointer { return m_base.data(); }

        constexpr auto operator[](size_type k) noexcept -> reference { return m_base[k]; }
        constexpr auto operator[](size_type k) const noexcept -> const_reference { return m_base[k]; }

        constexpr auto trim_trailing_zeros() const noexcept -> NumberSpan {
            auto tmp = *this;
            auto i = size();
            for (; i > 0; --i) {
                if (tmp.m_base[i - 1]) break;
            }
            tmp.m_base = { tmp.data(), i };
            return tmp;
        }

        constexpr auto slice(
            size_type start,
            size_type size = npos
        ) noexcept -> NumberSpan requires (!std::is_const_v<T>) {
            auto sz = std::min(size, this->size());
            sz = std::max(start, sz) - start;
            return { m_base.data() + start, sz };
        }

        constexpr auto slice(
            size_type start,
            size_type size = npos
        ) const noexcept -> NumberSpan<std::add_const_t<value_type>> {
            auto sz = std::min(size, this->size());
            sz = std::max(start, sz) - start;
            return { m_base.data() + start, sz };
        }

        constexpr auto abs() const noexcept -> NumberSpan {
            return { m_base, false, bits() };
        }

        constexpr operator NumberSpan<std::add_const_t<value_type>>() const noexcept {
            return { m_base, m_is_neg, m_bits };
        }
    private:
        base_type m_base{};
        bool m_is_neg{false};
        mutable size_type m_bits{npos};
    };

    template <typename T>
    NumberSpan(std::span<T>, bool, std::size_t) -> NumberSpan<T>;

    template <typename T>
    NumberSpan(T*, std::size_t, bool, std::size_t) -> NumberSpan<T>;

    using const_num_t = NumberSpan<MachineConfig::uint_t const>;
    using num_t = NumberSpan<MachineConfig::uint_t>;
} // namespace big_num::internal

#include <format>

namespace std {
    template <typename T>
    struct std::formatter<big_num::internal::NumberSpan<T>> {
        constexpr auto parse(auto& ctx) {
            auto it = ctx.begin();
            while (it != ctx.end()) {
                if (*it == '}') break;
                ++it;
            }
            return it;
        }

        auto format(big_num::internal::NumberSpan<T> const& n, auto& ctx) const {
            auto out = ctx.out();
            if (n.is_neg()) {
                std::format_to(out, "-");
            }
            std::format_to(out, "[");
            for (auto i = 0zu; i < n.size(); ++i) {
                // std::format_to(out, "0x{:016x}", n[i]);
                std::format_to(out, "{}", n[i]);
                if (i + 1 < n.size()) std::format_to(out, ", ");
            }
            std::format_to(out, "]");
            return out;
        }
    };
} // namespace std

#endif // AMT_BIG_NUM_INTERNAL_NUMBER_SPAN_HPP
