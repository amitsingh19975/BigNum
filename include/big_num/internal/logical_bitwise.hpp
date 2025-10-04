#ifndef AMT_BIG_NUM_INTERNAL_LOGICAL_BITWISE_HPP
#define AMT_BIG_NUM_INTERNAL_LOGICAL_BITWISE_HPP

#include "big_num/internal/base.hpp"
#include "big_num/internal/integer_parse.hpp"
#include "integer.hpp"
#include "ui/arch/arm/shift.hpp"
#include <cstddef>
#include <type_traits>

namespace big_num::internal {

    inline static constexpr auto bitwise_and(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> void {
        auto lsz = lhs.size();
        auto rsz = rhs.size();
        auto osz = std::max(lhs.bits(), rhs.bits());
        out.resize(osz);
        out.fill(0);

        using simd_t = MachineConfig::simd_uint_t; 
        static constexpr auto N = simd_t::elements;

        auto a = lhs.data();
        auto b = rhs.data();
        auto o = out.data();

        auto const nsz = std::min(lsz, rsz);
        auto i = std::size_t{};

        if (!std::is_constant_evaluated()) {
            auto ssz = nsz - N;
            for (; i < ssz; i += N) {
                auto l = simd_t::load(a + i, N);
                auto r = simd_t::load(b + i, N);
                auto t = l & r;
                t.store(o + i, N);
            }
        }

        for (; i < nsz; ++i) {
            o[i] = a[i] & b[i];
        }
    }

    inline static constexpr auto bitwise_or(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> void {
        auto lsz = lhs.size();
        auto rsz = rhs.size();
        auto osz = std::max(lhs.bits(), rhs.bits());
        out.resize(osz);

        using simd_t = MachineConfig::simd_uint_t; 
        static constexpr auto N = simd_t::elements;

        auto a = lhs.data();
        auto b = rhs.data();
        auto o = out.data();

        auto const nsz = std::min(lsz, rsz);
        auto i = std::size_t{};

        if (!std::is_constant_evaluated()) {
            auto ssz = nsz - N;
            for (; i < ssz; i += N) {
                auto l = simd_t::load(a + i, N);
                auto r = simd_t::load(b + i, N);
                auto t = l | r;
                t.store(o + i, N);
            }
        }

        for (; i < nsz; ++i) {
            o[i] = a[i] | b[i];
        }

        for (; i < lsz; ++i) {
            o[i] = a[i];
        }

        for (; i < rsz; ++i) {
            o[i] = b[i];
        }
    }

    inline static constexpr auto bitwise_xor(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) noexcept -> void {
        auto lsz = lhs.size();
        auto rsz = rhs.size();
        auto osz = std::max(lhs.bits(), rhs.bits());
        out.resize(osz);

        using simd_t = MachineConfig::simd_uint_t; 
        static constexpr auto N = simd_t::elements;

        auto a = lhs.data();
        auto b = rhs.data();
        auto o = out.data();

        auto const nsz = std::min(lsz, rsz);
        auto i = std::size_t{};

        if (!std::is_constant_evaluated()) {
            auto ssz = nsz - N;
            for (; i < ssz; i += N) {
                auto l = simd_t::load(a + i, N);
                auto r = simd_t::load(b + i, N);
                auto t = l ^ r;
                t.store(o + i, N);
            }
        }

        for (; i < nsz; ++i) {
            o[i] = a[i] ^ b[i];
        }

        for (; i < lsz; ++i) {
            o[i] = a[i] ^ 0;
        }

        for (; i < rsz; ++i) {
            o[i] = b[i] ^ 0;
        }
    }

    inline static constexpr auto bitwise_not(
        Integer& out,
        Integer const& a
    ) noexcept -> void {
        auto sz = a.size();
        out.resize(a.bits());

        using simd_t = MachineConfig::simd_uint_t; 
        static constexpr auto N = simd_t::elements;

        auto ptr = a.data();
        auto o = out.data();

        auto i = std::size_t{};

        if (!std::is_constant_evaluated()) {
            auto ssz = sz - N;
            for (; i < ssz; i += N) {
                auto l = simd_t::load(ptr + i, N);
                auto t = ~l;
                t.store(o + i, N);
            }
        }

        for (; i < sz; ++i) {
            o[i] = ~ptr[i];
        }
    }

    inline static constexpr auto negate(
        Integer& a
    ) noexcept -> Integer& {
        a.set_neg(!a.is_neg());
        return a;
    }

    namespace detail {
        template <std::size_t Count>
        inline static constexpr auto move_blocks_right(
            std::span<Integer::value_type> out
        ) noexcept -> void {
            if constexpr (Count == 0) return;

            using simd_t = MachineConfig::simd_uint_t; 
            static constexpr auto N = simd_t::elements;
            auto const size = out.size();
            assert(Count <= size);

            auto i = std::size_t{};
            if constexpr (Count >= N) {
                if (!std::is_constant_evaluated()) {
                    auto sz = size - Count;
                    sz = sz - sz % N;
                    auto ptr = out.data();
                    for (; i < sz; i += N) {
                        auto v = simd_t::load(ptr + i + Count, N);
                        v.store(ptr + i, N);
                    }
                }
            }

            for (; i < size - Count; ++i) {
                out[i] = out[i + Count];
            }
        }
    } // namespace detail

    template <std::size_t Count>
    inline static constexpr auto shift_right(
        std::span<Integer::value_type> out
    ) noexcept -> std::span<Integer::value_type> {
        if (out.empty()) return {};

        static constexpr auto blocks = Count / MachineConfig::bits;
        static constexpr auto rem = Count % MachineConfig::bits;
        if (blocks >= out.size()) return {};

        detail::move_blocks_right<blocks>(out);

        out = { out.data(), out.size() - blocks };
        if constexpr (rem == 0) return out;

        static constexpr auto mask = (MachineConfig::uint_t{1} << rem) - 1;

        auto i = std::size_t{};
        if (!std::is_constant_evaluated()) {
            using simd_t = MachineConfig::simd_uint_t; 
            static constexpr auto N = simd_t::elements;

            auto sz = out.size() - 1;
            sz = sz - sz % N;
            auto vmask = simd_t::load(mask);

            for (; i < sz; i += N) {
                auto b = simd_t::load(out.data() + i, N);
                auto nb = simd_t::load(out.data() + i + 1, N);
                auto r = ui::shift_right<rem>(b);
                auto m = ui::shift_left<MachineConfig::bits - rem>(nb & vmask);
                auto q = r | m;
                q.store(out.data() + i, N);
            }
        }

        for (; i < out.size() - 1; ++i) {
            auto c = out[i];
            auto n = out[i + 1];
            auto r = c >> rem;
            auto m = ((n & mask) << (MachineConfig::bits - rem));
            out[i] = r | m;
        }
        out.back() >>= rem;

        return out;
    }

    inline static constexpr auto shift_right(
        std::span<Integer::value_type> out,
        std::size_t count
    ) noexcept -> std::span<Integer::value_type> {
        if (out.empty()) return {};

        // [a, b, c, d] => [b, c, d]
        auto const blocks = count / MachineConfig::bits;
        if (blocks >= out.size()) return {};
        std::copy(out.begin() + static_cast<std::ptrdiff_t>(blocks), out.end(), out.begin());

        auto const rem = count % MachineConfig::bits;
        out = { out.data(), out.size() - blocks };
        if (rem == 0) return out;

        auto const mask = (MachineConfig::uint_t{1} << rem) - 1;

        auto i = std::size_t{};
        if (!std::is_constant_evaluated()) {
            using simd_t = MachineConfig::simd_uint_t; 
            static constexpr auto N = simd_t::elements;

            auto sz = out.size() - 1;
            sz = sz - sz % N;
            auto vmask = simd_t::load(mask);

            for (; i < sz; i += N) {
                auto b = simd_t::load(out.data() + i, N);
                auto nb = simd_t::load(out.data() + i + 1, N);
                auto r = b >> rem;
                auto m = (nb & vmask) << (MachineConfig::bits - rem);
                auto q = r | m;
                q.store(out.data() + i, N);
            }
        }

        for (; i < out.size() - 1; ++i) {
            auto c = out[i];
            auto n = out[i + 1];
            auto r = c >> rem;
            auto m = ((n & mask) << (MachineConfig::bits - rem));
            out[i] = r | m;
        }
        out.back() >>= rem;

        return out;
    }

    template <std::size_t Count, bool DeferTrim = false>
    inline static constexpr auto shift_right(
        Integer& out
    ) noexcept -> void {
        if (out.bits() <= Count) {
            out.resize(0);
            return;
        }
        auto t = shift_right<Count>(out.to_span());
        out.resize(t.size() * MachineConfig::bits);
        if constexpr (!DeferTrim) {
            remove_trailing_zeros(out);
        }
    }

    template <bool DeferTrim = false>
    inline static constexpr auto shift_right(
        Integer& out,
        std::size_t count
    ) noexcept -> void {
        if (out.bits() <= count) {
            out.resize(0);
            return;
        }
        auto t = shift_right(out.to_span(), count);
        out.resize(t.size() * MachineConfig::bits);
        if constexpr (!DeferTrim) {
            remove_trailing_zeros(out);
        }
    }

    template <std::size_t Count>
    inline static constexpr auto shift_left(
        std::span<Integer::value_type> out
    ) noexcept -> void {
        if (out.empty()) return;
        // [a, b, c, d] => [0, a, b, c, d]

        static constexpr auto blocks = Count / MachineConfig::bits;
        if (blocks >= out.size()) return;
        std::copy(out.rbegin() + static_cast<std::ptrdiff_t>(blocks), out.rend(), out.rbegin());
        std::fill_n(out.begin(), blocks, 0);

        static constexpr auto rem = Count % MachineConfig::bits;
        auto i = std::size_t{blocks};
        auto c = MachineConfig::uint_t{};

        if (!std::is_constant_evaluated()) {
            using simd_t = MachineConfig::simd_uint_t; 
            static constexpr auto N = simd_t::elements;

            auto sz = out.size();
            sz = sz - sz % N;

            if (sz != 0) {
                auto const mask = simd_t::load(MachineConfig::mask);
                auto p = ui::shift_right_lane<1>(simd_t::load(out.data() + i, N));

                for (; i < sz; i += N) {
                    auto n = simd_t::load(out.data() + i, N);
                    auto np = simd_t::load(out.data() + i + N - 1, N);

                    auto tp = ui::shift_right<MachineConfig::bits - rem>(p);
                    auto r = (ui::shift_left<rem>(n) | tp) & mask;

                    r.store(out.data() + i, N);
                    p = np;
                }
                c = (p[0] >> (MachineConfig::bits - rem));
            }
        }

        for (; i < out.size(); ++i) {
            auto e = out[i];
            auto r = ((e << rem) | c) & MachineConfig::mask;
            c = (e >> (MachineConfig::bits - rem));
            out[i] = static_cast<MachineConfig::uint_t>(r);
        }
    }

    inline static constexpr auto shift_left(
        std::span<Integer::value_type> out,
        std::size_t count
    ) noexcept -> void {
        if (out.empty()) return;
        // [a, b, c, d] => [0, a, b, c, d]

        auto const blocks = count / MachineConfig::bits;
        if (blocks >= out.size()) return;
        std::copy(out.rbegin() + static_cast<std::ptrdiff_t>(blocks), out.rend(), out.rbegin());
        std::fill_n(out.begin(), blocks, 0);

        auto const rem = count % MachineConfig::bits;
        if (rem == 0) return;

        auto i = std::size_t{blocks};
        auto c = MachineConfig::uint_t{};

        if (!std::is_constant_evaluated()) {
            using simd_t = MachineConfig::simd_uint_t; 
            static constexpr auto N = simd_t::elements;

            auto sz = out.size();
            sz = sz - sz % N;

            if (sz != 0) {
                auto const mask = simd_t::load(MachineConfig::mask);
                auto p = ui::shift_right_lane<1>(simd_t::load(out.data() + i, N));

                for (; i < sz; i += N) {
                    auto n = simd_t::load(out.data() + i, N);
                    auto np = simd_t::load(out.data() + i + N - 1, N);

                    auto tp = p >> (MachineConfig::bits - rem);
                    auto r = ((n << rem) | tp) & mask;

                    r.store(out.data() + i, N);
                    p = np;
                }
                c = (p[0] >> (MachineConfig::bits - rem));
            }
        }
        for (; i < out.size(); ++i) {
            auto e = out[i];
            auto r = ((e << rem) | c) & MachineConfig::mask;
            c = (e >> (MachineConfig::bits - rem));
            out[i] = static_cast<MachineConfig::uint_t>(r);
        }
    }

    inline static constexpr auto shift_left(
        Integer& out,
        std::size_t count
    ) -> void {
        out.resize(out.bits() + count);
        shift_left(out.to_span(), count);
        remove_trailing_zeros(out);
    }

    template <std::size_t Count>
    inline static constexpr auto shift_left(
        Integer& out
    ) -> void {
        out.resize(out.bits() + Count);
        shift_left<Count>(out.to_span());
        remove_trailing_zeros(out);
    }

    inline static constexpr auto set_integer_bit(
        std::span<Integer::value_type> out,
        std::size_t pos,
        bool bit
    ) noexcept -> void {
        auto block = pos / MachineConfig::bits;
        auto index = pos % MachineConfig::bits;
        if (out.size() <= block) return;
        out[block] |= (static_cast<Integer::value_type>(bit) << index);
    }

    inline static constexpr auto clear_integer_bit(
        std::span<Integer::value_type> out,
        std::size_t pos
    ) noexcept -> void {
        auto block = pos / MachineConfig::bits;
        auto index = pos % MachineConfig::bits;
        if (out.size() <= block) return;
        out[block] &= ~(Integer::value_type{1} << index);
    }

    inline static constexpr auto get_integer_bit(
        std::span<Integer::value_type const> out,
        std::size_t pos
    ) noexcept -> bool {
        auto block = pos / MachineConfig::bits;
        auto index = pos % MachineConfig::bits;
        if (out.size() <= block) return {};
        return static_cast<bool>(out[block] & (Integer::value_type{1} << index));
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_LOGICAL_BITWISE_HPP
