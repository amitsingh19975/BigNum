#ifndef AMT_BIG_NUM_INTERNAL_INTEGER_HPP
#define AMT_BIG_NUM_INTERNAL_INTEGER_HPP

#include <algorithm>
#include <climits>
#include <cstddef>
#include <memory_resource>
#include <span>
#include "base.hpp"

namespace big_num::internal {

    struct Integer {
        using size_type = std::size_t;
        using value_type = MachineConfig::uint_t;
        using acc_t = MachineConfig::acc_t;
    private:
        static constexpr size_type small_cap = std::max(4zu, MachineConfig::simd_acc_t::elements);
        static constexpr size_type small_bits = small_cap * MachineConfig::bits;
        static constexpr size_type last_bit_shift = sizeof(size_type) * CHAR_BIT - 1;
        static constexpr size_type last_bit = size_type{1} << last_bit_shift;
        constexpr auto is_small() const noexcept -> bool {
            return _cap_bits <= small_bits;
        }

    public:
        std::pmr::memory_resource* _resouce { std::pmr::get_default_resource() };
        union {
            value_type data[small_cap];
            value_type* ptr;
        } _data{ .data = {0} };

        size_type _bits{};
        size_type _cap_bits{small_bits};

        constexpr auto destroy() -> void {
            if (!is_small()) {
                allocator().deallocate(data(), MachineConfig::size(_cap_bits));
                _data = { .data = {} };
                _cap_bits = small_bits;
                _bits = 0;
            }
        }

        constexpr auto set_neg(bool flag) noexcept -> void {
            auto f = static_cast<size_type>(flag) << last_bit_shift;
            _bits |= f;
        }

        constexpr auto is_neg() const noexcept -> bool {
            return _bits & last_bit;
        }

        constexpr auto size() const noexcept -> size_type {
            return MachineConfig::size(bits());
        }

        constexpr auto bits() const noexcept -> size_type {
            return _bits & ~last_bit;
        }

        constexpr auto set_bits(size_type bits) noexcept -> void {
            _bits = bits | (size_type{is_neg()} << last_bit_shift);
        }


        constexpr auto cap() const noexcept -> size_type {
            return MachineConfig::size(_cap_bits);
        }

        constexpr auto empty() const noexcept -> bool { return bits() == 0; }
        constexpr auto data() const noexcept -> value_type const* {
            if (is_small()) return _data.data;
            return _data.ptr;
        }

        constexpr auto data() noexcept -> value_type* {
            if (is_small()) return _data.data;
            return _data.ptr;
        }

        constexpr auto begin() noexcept -> value_type* {
            return data();
        }

        constexpr auto end() noexcept -> value_type* {
            return data() + size();
        }

        constexpr auto begin() const noexcept -> value_type const* {
            return data();
        }

        constexpr auto end() const noexcept -> value_type const* {
            return data() + size();
        }

        constexpr auto allocator() const noexcept -> std::pmr::polymorphic_allocator<value_type> {
            return { _resouce };
        }

        constexpr auto fill(value_type val) noexcept -> void {
            std::fill_n(data(), size(), val);
        }

        static constexpr auto from(
            std::span<value_type> n,
            size_type bits,
            bool is_neg = false
        ) noexcept -> Integer {
            auto cap = std::max(bits, small_bits);
            auto tmp = Integer{
                ._data = { .ptr = n.data() },
                ._bits = bits | (value_type{is_neg} * last_bit_shift),
                ._cap_bits = cap
            };
            if (tmp.is_small()) {
                tmp._data = { .data = {} };
                std::copy_n(n.data(), MachineConfig::size(bits), tmp._data.data);
            }
            return tmp;
        }

        static constexpr auto from(
            std::span<value_type const> n,
            size_type bits,
            bool is_neg = false
        ) noexcept -> Integer const {
            auto p = const_cast<value_type*>(n.data());
            auto cap = std::max(bits, small_bits);
            auto tmp = Integer{
                ._data = { .ptr = p },
                ._bits = bits | (value_type{is_neg} * last_bit_shift),
                ._cap_bits = cap
            };
            if (tmp.is_small()) {
                tmp._data = { .data = {} };
                std::copy_n(n.data(), MachineConfig::size(bits), tmp._data.data);
            }
            return tmp;
        }

        template <bool Fill = true>
        constexpr auto resize(
            size_type bits,
            value_type def = {}
        ) -> void {
            static constexpr auto N = MachineConfig::simd_uint_t::elements;

            auto const old_chunks = size();
            if (bits <= _cap_bits) {
                set_bits(bits);
                auto const new_chunks = size();
                if constexpr (Fill) {
                    auto sz = std::max(new_chunks, old_chunks) - old_chunks;
                    std::fill_n(data() + old_chunks, sz, def);
                }
                return;
            }

            auto const new_chunks = MachineConfig::align_up<N>(MachineConfig::size(bits));
            auto ptr = allocator().allocate(new_chunks);
            auto old_ptr = data();
            std::copy(old_ptr, old_ptr + old_chunks, ptr);
            if constexpr (Fill) {
                auto sz = std::max(new_chunks, old_chunks) - old_chunks;
                std::fill_n(ptr + old_chunks, sz, def);
            }

            if (!is_small()) {
                allocator().deallocate(old_ptr, MachineConfig::size(_cap_bits));
            }
            _data = { .ptr = ptr };
            _cap_bits = new_chunks * MachineConfig::bits;
            set_bits(bits);
        }

        constexpr auto shrink_to_fit() -> void {
            static constexpr auto N = MachineConfig::simd_uint_t::elements;

            auto bits_ = this->bits();
            if (bits_ == _cap_bits) return;
            if (_cap_bits <= small_bits) return;
            // if we are wasting space less than 9, we avoid shrink.
            if (_cap_bits - bits_ <= 8) return;
            auto const nc = std::max(bits_, small_bits);
            auto const new_chunks = MachineConfig::align_up<N>(MachineConfig::size(nc));

            auto ptr = data();
            if (nc == small_bits) {
                _data = { .data = {} };
                std::copy(ptr, ptr + size(), _data.data);
            } else {
                auto np = allocator().allocate(new_chunks);
                auto sz = MachineConfig::size(bits_);
                std::copy(ptr, ptr + sz, np);
                _data = { .ptr = np };
            }

            if (!is_small()) {
                allocator().deallocate(ptr, MachineConfig::size(_cap_bits));
            }

            _cap_bits = new_chunks * MachineConfig::bits;
        }

        constexpr friend auto swap(Integer& lhs, Integer& rhs) noexcept -> void {
            auto tmp = lhs._data;
            lhs._data = rhs._data;
            rhs._data = tmp;

            std::swap(lhs._bits, rhs._bits);
            std::swap(lhs._cap_bits, rhs._cap_bits);
            std::swap(lhs._resouce, rhs._resouce);
        }

        constexpr auto to_span() noexcept -> std::span<value_type> {
            return { data(), size() };
        }

        constexpr auto to_span() const noexcept -> std::span<value_type const> {
            return { data(), size() };
        }

        constexpr operator std::span<value_type>() noexcept {
            return to_span();
        }

        constexpr operator std::span<const value_type>() const noexcept {
            return to_span();
        }
    };
} // big_num::internal

#endif // AMT_BIG_NUM_INTERNAL_INTEGER_HPP
