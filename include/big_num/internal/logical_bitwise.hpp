#ifndef AMT_BIG_NUM_INTERNAL_LOGICAL_BITWISE_HPP
#define AMT_BIG_NUM_INTERNAL_LOGICAL_BITWISE_HPP

#include "base.hpp"
#include "number_span.hpp"
#include "integer.hpp"
#include "ui.hpp"
#include <algorithm>
#include <cstddef>
#include <type_traits>

namespace big_num::internal {

    inline static constexpr auto bitwise_and(
        num_t out,
        const_num_t const& lhs,
        const_num_t const& rhs
    ) noexcept -> void {
        auto lsz = lhs.size();
        auto rsz = rhs.size();
        auto osz = std::max(lsz, rsz);
        assert(out.size() >= osz);

        using simd_t = MachineConfig::simd_uint_t; 
        static constexpr auto N = simd_t::elements;

        auto a = lhs.data();
        auto b = rhs.data();
        auto o = out.data();

        auto i = std::size_t{};

        if (!std::is_constant_evaluated()) {
            auto ssz = osz - N;
            for (; i < ssz; i += N) {
                auto l = simd_t::load(a + i, N);
                auto r = simd_t::load(b + i, N);
                auto t = l & r;
                t.store(o + i, N);
            }
        }

        for (; i < osz; ++i) {
            o[i] = a[i] & b[i];
        }
    }

    inline static constexpr auto bitwise_and(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> void {
        auto osz = std::max(lhs.bits(), rhs.bits());
        out.resize(osz);
        out.fill(0);
        bitwise_and(out.to_span(), lhs.to_span(), rhs.to_span());
    }

    inline static constexpr auto bitwise_or(
        num_t out,
        const_num_t const& lhs,
        const_num_t const& rhs
    ) noexcept -> void {
        auto lsz = lhs.size();
        auto rsz = rhs.size();

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

    inline static constexpr auto bitwise_or(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> void {
        auto osz = std::max(lhs.bits(), rhs.bits());
        out.resize(osz);
        bitwise_or(out.to_span(), lhs.to_span(), rhs.to_span());
    }

    inline static constexpr auto bitwise_xor(
        num_t out,
        const_num_t const& lhs,
        const_num_t const& rhs
    ) noexcept -> void {
        auto lsz = lhs.size();
        auto rsz = rhs.size();

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

    inline static constexpr auto bitwise_xor(
        Integer& out,
        Integer const& lhs,
        Integer const& rhs
    ) -> void {
        auto osz = std::max(lhs.bits(), rhs.bits());
        out.resize(osz);
        bitwise_xor(out.to_span(), lhs.to_span(), rhs.to_span());
    }

    inline static constexpr auto bitwise_not(
        num_t out,
        const_num_t const& a
    ) noexcept -> void {
        auto sz = a.size();

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

    inline static constexpr auto bitwise_not(
        Integer& out,
        Integer const& a
    ) -> void {
        out.resize(a.bits());
        bitwise_not(out.to_span(), a.to_span());
    }

    inline static constexpr auto negate(
        num_t a
    ) noexcept -> void {
        a.set_neg(!a.is_neg());
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

    template <std::size_t Count>
    inline static constexpr auto shift_right(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> in
    ) noexcept -> void {
        if (in.empty()) return;

        static constexpr auto blocks = Count / MachineConfig::bits;
        static constexpr auto rem = Count % MachineConfig::bits;
        if (blocks >= in.size()) return;

        std::copy(in.begin() + static_cast<std::ptrdiff_t>(blocks), in.end(), out.begin());

        if constexpr (rem == 0) return;

        static constexpr auto mask = (MachineConfig::uint_t{1} << rem) - 1;

        for (auto i = 0zu; i < out.size() - 1; ++i) {
            auto c = out[i];
            auto n = out[i + 1];
            auto r = c >> rem;
            auto m = ((n & mask) << (MachineConfig::bits - rem));
            out[i] = r | m;
        }
        out.back() >>= rem;
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

    inline static constexpr auto shift_right(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> in,
        std::size_t count
    ) noexcept -> Integer::value_type {
        if (in.empty()) return {};

        // [a, b, c, d] => [b, c, d]
        auto const blocks = count / MachineConfig::bits;
        auto const size = std::min(out.size(), in.size());

        if (blocks >= size) {
            std::fill(out.begin(), out.end(), 0);
            return {};
        }

        auto const rem = count % MachineConfig::bits;
        if (rem == 0) {
            std::copy_n(
                in.begin() + static_cast<std::ptrdiff_t>(blocks),
                size - blocks,
                out.begin()
            );
            return {};
        }

        auto const mask = (MachineConfig::uint_t{1} << rem) - 1;

        auto const sz = size - blocks;

        for (auto i = 0zu; i < sz - 1; ++i) {
            auto c = in[i];
            auto n = in[i + 1];
            auto r = c >> rem;
            auto m = ((n & mask) << (MachineConfig::bits - rem));
            out[i] = r | m;
        }
        auto const result = (out[sz - 1] & mask);
        out[sz - 1] = in[sz - 1] >> rem;

        return result;
    }

    template <std::size_t Count>
    inline static constexpr auto shift_right(
        Integer& out
    ) noexcept -> void {
        if (out.bits() <= Count) {
            out.resize(0);
            return;
        }
        auto t = shift_right<Count>(out.to_span());
        out.resize(t.size() * MachineConfig::bits);
        out.remove_trailing_empty_blocks();
    }

    template <std::size_t Count>
    inline static constexpr auto shift_right(
        Integer& out,
        Integer const& in
    ) noexcept -> void {
        auto const bits = in.bits();
        if (bits <= Count) {
            out.resize(0);
            return;
        }
        out.resize(bits);
        shift_right<Count>(out.to_span(), in.to_span());
        out.remove_trailing_empty_blocks();
    }

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
        out.remove_trailing_empty_blocks();
    }

    inline static constexpr auto shift_right(
        Integer& out,
        Integer const& in,
        std::size_t count
    ) noexcept -> void {
        auto const bits = in.bits();
        if (bits <= count) {
            out.resize(0);
            return;
        }
        out.resize(bits);
        shift_right(out.to_span(), in.to_span(), count);
        out.remove_trailing_empty_blocks();
    }

    template <std::size_t Count>
    inline static constexpr auto shift_left(
        std::span<Integer::value_type> out
    ) noexcept -> Integer::value_type {
        using val_t = Integer::value_type;
        if (out.empty()) return {};
        // [a, b, c, d] => [0, a, b, c, d]

        static constexpr auto blocks = Count / MachineConfig::bits;
        if (blocks >= out.size()) return {};
        auto result = blocks == 0 ? val_t{} : out[out.size() - blocks];
        std::copy(out.rbegin() + static_cast<std::ptrdiff_t>(blocks), out.rend(), out.rbegin());
        std::fill_n(out.begin(), blocks, 0);

        static constexpr auto rem = Count % MachineConfig::bits;
        if constexpr (rem == 0) return result;

        result = out.back();

        auto i = std::size_t{blocks};
        auto c = val_t{};

        result &= (1 << rem) - 1;

        if (!std::is_constant_evaluated()) {
            using simd_t = MachineConfig::simd_uint_t; 
            static constexpr auto N = simd_t::elements;

            auto sz = out.size();
            sz = sz - sz % N;

            if (sz != 0) {
                auto const mask = simd_t::load(MachineConfig::mask);
                auto p = ui::shift_right_lane<1>(simd_t::load(out.data() + i, N));

                for (; i < sz - N; i += N) {
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

        return result;
    }

    template <std::size_t Count>
    inline static constexpr auto shift_left(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> in
    ) noexcept -> Integer::value_type {
        using val_t = Integer::value_type;

        if (in.empty()) return {};
        // [a, b, c, d] => [0, a, b, c, d]

        static constexpr auto blocks = Count / MachineConfig::bits;
        auto const size = std::min(out.size(), in.size());

        if (blocks >= in.size()) {
            std::fill(out.begin(), out.end(), 0);
            return {};
        }

        std::fill_n(out.begin(), blocks, 0);
        auto result = blocks == 0 ? val_t{} : in[in.size() - blocks];

        static constexpr auto rem = Count % MachineConfig::bits;
        if constexpr (rem == 0) {
            std::copy_n(in.begin(), size - blocks, out.begin() + blocks);
            return result;
        }

        result = in[in.size() - blocks - 1];
        result &= (1 << rem) - 1;

        auto c = MachineConfig::uint_t{};

        for (auto i = blocks; i < size; ++i) {
            auto e = in[i];
            auto r = ((e << rem) | c) & MachineConfig::mask;
            c = (e >> (MachineConfig::bits - rem));
            out[i] = static_cast<MachineConfig::uint_t>(r);
        }

        return result;
    }

    inline static constexpr auto shift_left(
        std::span<Integer::value_type> out,
        std::size_t count
    ) noexcept -> Integer::value_type {
        using val_t = Integer::value_type;
        if (out.empty()) return {};
        // [a, b, c, d] => [0, a, b, c, d]

        auto const blocks = count / MachineConfig::bits;
        if (blocks >= out.size()) return {};
        auto result = blocks == 0 ? val_t{} : out[out.size() - blocks];

        std::copy(out.rbegin() + static_cast<std::ptrdiff_t>(blocks), out.rend(), out.rbegin());
        std::fill_n(out.begin(), blocks, 0);

        auto const rem = count % MachineConfig::bits;

        if (rem == 0) return result;

        auto i = std::size_t{blocks};
        auto c = MachineConfig::uint_t{};

        result = out.back();
        result &= (1 << rem) - 1;

        if (!std::is_constant_evaluated()) {
            using simd_t = MachineConfig::simd_uint_t; 
            static constexpr auto N = simd_t::elements;

            auto sz = out.size();
            sz = sz - sz % N;

            if (sz != 0) {
                auto const mask = simd_t::load(MachineConfig::mask);
                auto p = ui::shift_right_lane<1>(simd_t::load(out.data() + i, N));

                for (; i < sz - N; i += N) {
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

        return result;
    }

    inline static constexpr auto shift_left(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> in,
        std::size_t count
    ) noexcept -> Integer::value_type {
        using val_t = Integer::value_type;
        if (in.empty()) return {};
        // [a, b, c, d] => [0, a, b, c, d]

        auto const size = std::min(out.size(), in.size());

        auto const blocks = count / MachineConfig::bits;
        if (blocks >= in.size()) return {};

        std::fill_n(out.begin(), blocks, 0);

        auto const rem = count % MachineConfig::bits;
        auto result = blocks == 0 ? val_t{} : in[in.size() - blocks];

        if (rem == 0) {
            std::copy_n(
                in.begin(),
                size - blocks,
                out.begin() + static_cast<std::ptrdiff_t>(blocks)
            );
            return {};
        }

        result = out.back();
        result &= (1 << rem) - 1;

        auto c = Integer::value_type{};
        for (auto i = blocks; i < size; ++i) {
            auto e = in[i];
            auto r = ((e << rem) | c) & MachineConfig::mask;
            c = (e >> (MachineConfig::bits - rem));
            out[i] = static_cast<MachineConfig::uint_t>(r);
        }

        return result;
    }

    template <bool FixedSize = false>
    inline static constexpr auto shift_left(
        Integer& out,
        std::size_t count
    ) -> Integer::value_type {
        if constexpr (!FixedSize) {
            out.resize(out.bits() + count);
        }
        auto mbits = shift_left(out.to_span(), count);
        out.remove_trailing_empty_blocks();
        return mbits;
    }

    inline static constexpr auto shift_left(
        Integer& out,
        Integer const& in,
        std::size_t count
    ) -> Integer::value_type {
        out.resize(in.bits() + count);
        auto mbits = shift_left(out.to_span(), in.to_span(), count);
        out.remove_trailing_empty_blocks();
        return mbits;
    }

    template <std::size_t Count, bool FixedSize = false>
    inline static constexpr auto shift_left(
        Integer& out
    ) -> Integer::value_type {
        if constexpr (!FixedSize) {
            out.resize(out.bits() + Count);
        }
        auto mbits = shift_left<Count>(out.to_span());
        out.remove_trailing_empty_blocks();
        return mbits;
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

    inline static constexpr auto ones_complement(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> in
    ) noexcept -> void {
        if (in.empty()) return;

        auto i = std::size_t{};

        assert(out.size() == in.size());
        auto size = in.size();

        if (!std::is_constant_evaluated()) {
            using simd_t = MachineConfig::simd_uint_t; 
            static constexpr auto N = simd_t::elements;
            auto mask = simd_t::load(MachineConfig::mask);

            auto sz = size - size % N;

            for (; i < sz; i += N) {
                auto v = simd_t::load(in.data() + i, N);
                v = ui::bitwise_not(v) & mask;
                v.store(out.data() + i, N);
            }
        }

        for (; i < size; ++i) {
            auto v = in[i];
            out[i] = (~v) & MachineConfig::mask;
        }
    }

    inline static constexpr auto ones_complement(
        std::span<Integer::value_type> out
    ) noexcept -> void {
        ones_complement(out, out);
    }

    inline static constexpr auto twos_complement(
        std::span<Integer::value_type> out,
        std::span<Integer::value_type const> in
    ) noexcept -> void {
        if (in.empty()) return;

        auto size = in.size();

        auto c = Integer::acc_t{1};

        for (auto i = 0zu; i < size; ++i) {
            auto r = (~in[i]) & MachineConfig::mask;
            auto s = r + c;
            out[i] = s & MachineConfig::mask;
            c = (s >> MachineConfig::bits);
        }

        // auto n = in[size - 1];
        // auto const mask = (Integer::value_type{1} << std::bit_width(n)) - 1;
        // auto r = (~n);
        // auto s = r + c;
        // out[size - 1] = s & mask;
    }

    inline static constexpr auto twos_complement(
        std::span<Integer::value_type> out
    ) noexcept -> void {
        twos_complement(out, out);
    }

    inline static constexpr auto ones_complement(
        Integer& out
    ) noexcept -> void {
        ones_complement(out.to_span());
        out.remove_trailing_empty_blocks();
    }

    inline static constexpr auto ones_complement(
        Integer& out,
        Integer const& in
    ) noexcept -> void {
        out.resize(in.bits());
        ones_complement(out.to_span(), in.to_span());
        out.remove_trailing_empty_blocks();
    }

    inline static constexpr auto twos_complement(
        Integer& out
    ) noexcept -> void {
        twos_complement(out.to_span());
        out.remove_trailing_empty_blocks();
    }

    inline static constexpr auto twos_complement(
        Integer& out,
        Integer const& in
    ) noexcept -> void {
        out.resize(in.bits());
        twos_complement(out.to_span(), in.to_span());
        out.remove_trailing_empty_blocks();
    }
} // namespace big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_LOGICAL_BITWISE_HPP
