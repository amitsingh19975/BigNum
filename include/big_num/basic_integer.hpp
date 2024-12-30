#ifndef DARK_BIG_NUM_BASIC_INTEGER_HPP
#define DARK_BIG_NUM_BASIC_INTEGER_HPP

#include <algorithm>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <ostream>
#include <cstdint>
#include <format>
#include <print>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include "allocator.hpp"
#include "dyn_array.hpp"
#include <optional>
#include <expected>
#include "bitwise.hpp"
#include "converter.hpp"
#include "division.hpp"
#include "type_traits.hpp"
#include "block_info.hpp"
#include "utils.hpp"
#include "mul.hpp"
#include "basic.hpp"
#include "operators.hpp"

namespace dark::internal {

	class BasicInteger {
	public:
		using block_t = typename BlockInfo::type;
		using block_acc_t = typename BlockInfo::accumulator_t;

		using base_type = BlockInfo::blocks_t;
		using value_type = typename base_type::value_type;
		using reference = typename base_type::reference;
		using const_reference = typename base_type::const_reference;
		using iterator = typename base_type::iterator;
		using const_iterator = typename base_type::const_iterator;
		using reverse_iterator = typename base_type::reverse_iterator;
		using const_reverse_iterator = typename base_type::const_reverse_iterator;
		using pointer = typename base_type::pointer;
		using const_pointer = typename base_type::const_pointer;
		using size_type = typename base_type::size_type;
		using difference_type = typename base_type::difference_type;

        static constexpr auto npos = base_type::npos;

		constexpr BasicInteger() noexcept = default;
		constexpr BasicInteger(BasicInteger const&) = default;
		constexpr BasicInteger& operator=(BasicInteger const&) = default;
		constexpr BasicInteger(BasicInteger &&) noexcept = default;
		constexpr BasicInteger& operator=(BasicInteger &&) noexcept = default;
		constexpr ~BasicInteger() = default;

		constexpr BasicInteger(std::string_view num, Radix radix = Radix::None) {
			auto temp = from(num, radix);
			if (!temp) {
				throw std::runtime_error(temp.error());
			}

			swap(*this, *temp);
		}

		template <std::integral I>
		constexpr BasicInteger(I num) {
			auto temp = from(num);
			if (!temp) {
				throw std::runtime_error(temp.error());
			}

			swap(*this, *temp);
		}

		constexpr auto is_neg() const noexcept -> bool {
			return m_flags & BigNumFlag::Neg;
		}

		constexpr auto set_is_neg(bool is_neg) noexcept -> void {
			toggle_flag(is_neg, BigNumFlag::Neg);
		}
		
		constexpr auto is_overflow() const noexcept -> bool {
			return m_flags & BigNumFlag::Overflow;
		}

		constexpr auto set_overflow(bool f) noexcept -> void {
			toggle_flag(f, BigNumFlag::Overflow);
		}
		
		constexpr auto is_underflow() const noexcept -> bool {
			return m_flags & BigNumFlag::Underflow;
		}

		constexpr auto set_underflow(bool f) noexcept -> void {
			toggle_flag(f, BigNumFlag::Underflow);
		}

		constexpr auto set_zero() noexcept -> void {
			m_data.clear();
			m_bits = 0;
			m_flags = 0;
		}

		constexpr auto reset_ou_flags() noexcept {
			auto neg = is_neg();
			m_flags = 0;
			set_is_neg(neg);
		}

		static auto from(
			std::string_view num,
			Radix radix = Radix::None
		) noexcept -> std::expected<BasicInteger, std::string> {
			auto sign_result = parse_sign(num);
			if (!sign_result) return std::unexpected(std::move(sign_result.error()));
			auto [is_neg, sign_end] = sign_result.value();

			std::string temp;


			num = num.substr(sign_end);

			if (auto it = num.find_first_not_of(" "); it != std::string_view::npos) {
				num = num.substr(it);
			}
			if (auto it = num.find_last_not_of(" "); it != std::string_view::npos) {
				num = num.substr(0, it + 1);
			}

			auto infer_result = infer_radix(num, radix);
			if (!infer_result.has_value()) return std::unexpected(std::move(infer_result.error()));
			auto [infered_radix, num_start] = infer_result.value();
		
			num = utils::trim_leading_zero(num.substr(num_start));
			if (num.contains('_')) {
				temp = sanitize_number(num);
				num = temp;
			}
			auto validate_result = validate_digits(num, infered_radix);
			if (!validate_result.has_value()) return std::unexpected(std::move(validate_result.error()));

			auto res = BasicInteger();
			base_convert(res, num, infered_radix);
			res.set_is_neg(is_neg);
			
			return res;
		}

		template <std::integral I>
		static auto from(
			I num
		) noexcept -> std::expected<BasicInteger, std::string> {
			using type = std::decay_t<std::remove_cvref_t<I>>;
			using unsigned_type = to_unsigned<type>::type;

			constexpr auto bytes = sizeof(unsigned_type);
			auto res = BasicInteger{};

			bool is_neg{false};
			auto new_num = unsigned_type{};
			if constexpr (std::numeric_limits<type>::is_signed) {
				if (num < 0) {
					is_neg = true;
					num = -num;
				}
			}
			
			new_num = static_cast<unsigned_type>(num);

			if (new_num < BlockInfo::block_max_value) {
				if (new_num == 0) return res;
				res.m_data.push_back(new_num & BlockInfo::block_lower_mask);
				res.trim_leading_zeros();
				res.set_is_neg(is_neg);
				return res;
			}


            constexpr auto bytes_needed = BlockInfo::calculate_blocks_from_bytes(bytes) + 1;
			res.resize(bytes_needed, block_t{});

			for (auto i = 0zu; i < bytes_needed; ++i) {
				auto n = new_num % BlockInfo::block_max_value;
				new_num /= BlockInfo::block_max_value;
				res[i] = static_cast<block_t>(n);
			}

			res.set_is_neg(is_neg);
			res.trim_leading_zeros();
			return res;
		}

		constexpr iterator begin() noexcept { return m_data.begin(); }
		constexpr iterator end() noexcept { return m_data.end(); }
		constexpr const_iterator begin() const noexcept { return m_data.begin(); }
		constexpr const_iterator end() const noexcept { return m_data.end(); }
		constexpr reverse_iterator rbegin() noexcept { return m_data.rbegin(); }
		constexpr reverse_iterator rend() noexcept { return m_data.rend(); }
		constexpr const_reverse_iterator rbegin() const noexcept { return m_data.rbegin(); }
		constexpr const_reverse_iterator rend() const noexcept { return m_data.rend(); }

		constexpr size_type size() const noexcept { return m_data.size(); }
		constexpr size_type actual_size() const noexcept {
			auto sz = size();
			for (; sz > 0; --sz) {
				if (m_data[sz - 1] != 0) return sz;
			}
			return 0;
		}
		constexpr size_type bits() const noexcept { return m_bits; }
		constexpr size_type bytes() const noexcept { return bits() / 8; }
		constexpr pointer data() noexcept { return m_data.data(); }
		constexpr const_pointer data() const noexcept { return m_data.data(); }

		constexpr auto dyn_arr() const noexcept -> base_type const& { return m_data; }
		constexpr auto dyn_arr() noexcept -> base_type& { return m_data; }

		friend std::ostream& operator<<(std::ostream& os, BasicInteger const& o) {
			return os << o.to_str();	
		}

		constexpr auto add(BasicInteger const& other) const -> BasicInteger {
			auto res = BasicInteger{};
			add_impl(res, *this, other);
			return res;
		}
		
		constexpr auto add_mut(BasicInteger const& other) noexcept -> BasicInteger& {
			auto temp = *this;
			m_data.clear();
			add_impl(*this, temp, other);
			return *this;
		}

		constexpr auto sub(BasicInteger const& other) const -> BasicInteger {
			auto res = BasicInteger{};
			sub_impl(res, *this, other);
			return res;
		}

		constexpr auto sub_mut(BasicInteger const& other) noexcept -> BasicInteger& {
			auto temp = *this;
			m_data.clear();
			sub_impl(*this, temp, other);
			return *this;	
		}
		
		constexpr auto mul(BasicInteger const& other, MulKind kind = MulKind::Auto) const -> BasicInteger {
			auto res = *this;
			res.m_data.clear();
			integer::mul_impl(res, *this, other, kind);
			return res;
		}

		constexpr auto mul_mut(BasicInteger const& other, MulKind kind = MulKind::Auto) -> BasicInteger& {
			auto temp = *this;
			m_data.clear();
			integer::mul_impl(*this, temp, other, kind);
			return *this;
		}

		// NOTE: it's templated to avoid lazy load type since BasicInteger is not defined
		// completely.
		template <typename I>
		struct Div {
			I quot;
			I rem;
		};

		constexpr auto div(BasicInteger const& other, DivKind kind = DivKind::Auto) const -> Div<BasicInteger> {
			auto result = Div<BasicInteger>{};
			div_impl(result.quot, result.rem, *this, other, kind);
			return result;
		}

        constexpr auto exact_div(BasicInteger const& divisor) const -> BasicInteger {
            auto q = BasicInteger();
            [[maybe_unused]] auto res = integer::exact_div(*this, divisor, q);
            assert(res && "numerator is not divisble by divisor");
            return q;
        }

		constexpr auto bitwise_or(BasicInteger const& other) const -> BasicInteger {
			auto res = *this;
			res.bitwise_or_mut(other);
			return res;
		}

		constexpr auto bitwise_or_mut(BasicInteger const& other) -> BasicInteger& {
			bitwise_op_helper(
				*this,
				other,
				[](auto l, auto r) { return l | r; }
			);
			return *this;
		}

		constexpr auto bitwise_and(BasicInteger const& other) const -> BasicInteger {
			auto res = *this;
			res.bitwise_and_mut(other);
			return res;
		}

		constexpr auto bitwise_and_mut(BasicInteger const& other) -> BasicInteger& {
			bitwise_op_helper(
				*this,
				other,
				[](auto l, auto r) { return l & r; }
			);
			return *this;
		}

		std::string to_str(Radix radix = Radix::Dec, bool with_prefix = false, std::optional<char> separator = {}) const {
			std::string s;

            auto sz = 0zu;
            for (auto i = 0zu; i < size(); ++i) {
                if (m_data[size() - i - 1] != 0) break;
                ++sz;
            }
            sz = size() - sz;

			if (is_zero() || sz == 0) {
				s = get_prefix(radix);
				s += "0";
				return s;
			}

			auto get_digit = [this, sz](auto i) { return m_data[sz - i - 1]; };

            auto bits = compute_used_bits(m_data);

			switch (radix) {
				case Radix::Binary: {
					s.resize(bits + 2);
					utils::basic_convert_to_block_radix<2>(
						s.data(),
						BlockInfo::block_max_value,
						sz,
						get_digit
					);
				} break;
				case Radix::Octal: {
					s.resize(bits + 2);
					utils::basic_convert_to_block_radix<8>(
						s.data(),
						BlockInfo::block_max_value,
						sz,
						get_digit
					);
				} break;
				case Radix::Hex: {
					s.resize(bits / 4 + 2, 0);
					utils::basic_convert_to_block_radix<16>(
						s.data(),
						BlockInfo::block_max_value,
						sz,
						get_digit
					);
				} break;
				case Radix::Dec: case Radix::None: {
					s.resize(bits / 2 + 2, 0);
					utils::basic_convert_to_block_radix<10>(
						s.data(),
						BlockInfo::block_max_value,
						sz,
						get_digit
					);
				} break;
				default: std::unreachable();
			}

			while (!s.empty() && s.back() == 0) s.pop_back();
			if (s.empty()) {
				s = get_prefix(radix);
				s += "0";
				return s;
			};
			for (auto& c: s) {
				c = utils::number_to_hex_char_mapping[static_cast<std::size_t>(c)];
			}

			if (separator) {
				auto temp = std::string();
				auto sep = *separator;
				auto const sep_factor = radix == Radix::Dec ? 3zu : 4zu;
				temp.reserve(s.size() + s.size() / sep_factor);
				for (auto i = 0zu; i < s.size(); i += sep_factor ) {
					temp.append(s.substr(i, sep_factor));
					if (i + sep_factor < s.size()) temp.push_back(sep);
				}
				s = std::move(temp);
			}

			std::reverse(s.begin(), s.end());

			if (with_prefix) {
				auto prefix = get_prefix(radix);
				s.insert(s.begin(), prefix.begin(), prefix.end());
			}
			if (is_neg()) s.insert(s.begin() , '-');

			s.shrink_to_fit();

			return s;
		}

		std::string to_bin(bool prefix = false) const { return to_str(Radix::Binary, prefix); }
		std::string to_oct(bool prefix = false) const { return to_str(Radix::Octal, prefix); }
		std::string to_hex(bool prefix = false) const { return to_str(Radix::Hex, prefix); }

		constexpr auto shift_left(std::size_t shift, bool should_extend = false) const -> BasicInteger {
			auto temp = *this;
			shift_left_helper(temp, shift, should_extend);
			return temp;
		}
		
		constexpr auto shift_left_mut(std::size_t shift, bool should_extend = false) -> BasicInteger& {
			shift_left_helper(*this, shift, should_extend);
			return *this;
		}

		constexpr auto shift_right(std::size_t shift) const -> BasicInteger {
			auto temp = *this;
			shift_right_helper(temp, shift);
			return temp;
		}
		
		constexpr auto shift_right_mut(std::size_t shift) noexcept -> BasicInteger& {
			shift_right_helper(*this, shift);
			return *this;
		}

		constexpr auto abs_mut() noexcept -> BasicInteger& {
			set_is_neg(false);
			return *this;
		}

		constexpr auto abs() const -> BasicInteger {
			auto temp = *this;
			temp.abs_mut();
			return temp;
		}

		friend void swap(BasicInteger& lhs, BasicInteger& rhs) noexcept {
			using std::swap;
			swap(lhs.m_data, rhs.m_data);
			swap(lhs.m_bits, rhs.m_bits);
			swap(lhs.m_flags, rhs.m_flags);
		}
		
		constexpr auto is_zero() const noexcept -> bool {
			if (size() == 0) return true;
			if (size() == 1) return m_data[0] == 0;
			
			return false;
		}

		constexpr auto is_one() const noexcept -> bool {
			if (size() == 1) return m_data[0] == 1;
			return false;
		}

		constexpr auto set_bit(std::size_t pos, bool flag, bool should_expand = false) -> void {
			auto index = pos / BlockInfo::block_total_bits;
			if (index >= size()) {
				if (!should_expand) return;
				resize(index + 1, 0);
			}
			pos = pos % BlockInfo::block_total_bits;
			
			auto& block = m_data[index];
			auto bit = block_t{flag};
			if (flag) block |= (bit << pos);
			else block &= ~(bit << pos);

			trim_leading_zeros();
		}

		constexpr auto get_bit(std::size_t pos) const noexcept -> bool {
			auto index = pos / BlockInfo::block_total_bits;
			if (index >= size()) return false;
			pos = pos % BlockInfo::block_total_bits;
			auto block = m_data[index];
			return (block >> pos) & 1; 
		}

		constexpr auto is_power_of_two() const noexcept -> bool {
			if (is_zero()) return true;
			for (auto i = 0zu; i < size() - 1; ++i) {
				if (m_data[i] != 0) return false;
			}	
			auto b = m_data.back();
			return !(b & (b - 1));
		}

		constexpr auto trim_leading_zeros() noexcept -> std::size_t {
			auto i = 0zu;
			for (; i < size(); ++i) {
				auto idx = size() - i - 1;
				if (m_data[idx]) break;
			}

			resize(size() - i);

			if (m_data.empty()) {
				set_is_neg(false);
				m_bits = 0;
				m_flags = 0;
				return i;
			}
			auto bits = static_cast<std::size_t>(std::bit_width(m_data.back()));
			m_bits = (size() - 1) * BlockInfo::block_total_bits + bits;
            return i;
		}

		constexpr auto trim_trailing_zeros() noexcept -> std::size_t {
			auto i = 0zu;
			for (; i < size(); ++i) {
				if (m_data[i]) break;
			}

            pop_front(i);
            if (m_data.empty()) return i;

			auto bits = static_cast<std::size_t>(std::bit_width(m_data.back()));
			m_bits = (size() - 1) * BlockInfo::block_total_bits + bits;
            return i;
		}

        constexpr auto pop_front(size_type n = 1) noexcept -> void {
            if (n == 0) return;
            if (n == size()) m_data.clear();
            else {
                std::move(begin() + n, end() - n, begin());
                resize(size() - n);
            }
            if (m_data.empty()) {
                m_bits = 0;
                m_flags = 0;
            }
        }

		constexpr auto pow(std::size_t pow) -> BasicInteger {
			auto res = *this;
			res.pow_mut(pow);
			return res;
		}

		constexpr auto pow_mut(std::size_t pow) -> BasicInteger& {
			if (pow == 0) {
				m_data.clear();
				m_data.push_back(1);
				m_bits = 1;
				m_flags = 0;
				return *this;
			}

			exponential_pow(pow);
			return *this;
		}
		constexpr auto to_borrowed(size_type start, size_type size = base_type::npos) const noexcept -> BasicInteger {
			auto temp = BasicInteger{};
			temp.m_data = m_data.to_borrowed(start, size);
			temp.m_bits = compute_used_bits(temp.m_data);
			temp.m_flags = m_flags;
			return temp;
		}

		// ranage: [start, end)
		// This is an exculsive range or end is not included in the range.
		constexpr auto to_borrowed_from_range(size_type start, size_type end) const noexcept -> BasicInteger {
			return to_borrowed(start, end - start);
		}
		
		constexpr auto transfer_ownership(BasicInteger& other) -> void {
			if (other.dyn_arr().allocator() != dyn_arr().allocator()) {
				dyn_arr().clone_from(other.dyn_arr());
			} else {
				m_data = std::move(other.m_data);
			}
			m_bits = other.m_bits;
			m_flags = other.m_flags;
		}

		constexpr auto shift_by_base(size_type pow) -> BasicInteger& {
			auto const sz = size();
			resize(sz + pow, 0);
            std::move(rbegin() + static_cast<std::ptrdiff_t>(pow), rend(), rbegin());
            std::fill_n(begin(), pow, 0);
			return *this;
		}

		constexpr auto operator[](size_type k) const noexcept -> const_reference {
			return m_data[k];
		}
		constexpr auto operator[](size_type k) noexcept -> reference {
			return m_data[k];
		}

        constexpr auto replace_range(std::span<block_t> bs, size_type start, size_type end = npos) -> void {
            auto const sz = std::min({ end - start, size(), bs.size() });
            std::copy_n(bs.begin(), sz, begin());
        }

		constexpr auto abs_less(BasicInteger const& other) const noexcept -> bool {
			if (bits() < other.bits()) return true;
			else if (bits() > other.bits()) return false;
			return compare(*this, other, std::less<>{});
		} 

	private:
		static constexpr auto shift_left_helper(
			BasicInteger& out,
			std::size_t shift,
			bool should_extend
		) -> void {
			if (should_extend) {
				auto const diff_bits = shift;
				auto const diff_size = (diff_bits + BlockInfo::block_total_bits - 1) / BlockInfo::block_total_bits;
				out.m_data.resize(out.size() + diff_size, 0);
			}
			logical_left_shift(out.dyn_arr(), shift);
			out.trim_leading_zeros();
		}

		static constexpr auto shift_right_helper(
			BasicInteger& out,
			std::size_t shift
		) noexcept -> void {
			logical_right_shift(out.dyn_arr(), shift);
			out.trim_leading_zeros();
		}

		static constexpr auto bitwise_op_helper(
			BasicInteger& out,
			BasicInteger const& mask,
			auto&& fn
		) -> void {
			auto const size = std::max(out.size(), mask.size());
			out.resize(size);
			for (auto i = 0zu; i < size; ++i) {
				out.m_data[i] = fn(out.m_data[i], mask.m_data[i]);
			}
			out.trim_leading_zeros();
		}
		
		constexpr auto exponential_pow(std::size_t pow) -> void {
			auto res = BasicInteger{};
			res.m_data.reserve(size() * pow);
			res.m_data.push_back(1);
			res.m_bits = 1;
			auto& self = *this;
			while (pow) {
				if (pow & 1) res.mul_mut(self);
				self = self * self;
				pow >>= 1;
			}
			self = std::move(res);
			self.trim_leading_zeros();
			self.reset_ou_flags();
		}

		static constexpr auto add_impl(
			BasicInteger& res,
			BasicInteger const& a,
			BasicInteger const& b,
			bool check_sign = true
		) noexcept -> void {
			if (check_sign && a.is_neg() != b.is_neg()) return sub_impl(res, a, b, false);

			auto size_required = std::max(a.size(), b.size()) + 1;
			res.resize(size_required, 0);
			res.m_bits = std::max(a.bits(), b.bits());

			res.set_is_neg(a.is_neg());
			
			std::copy(a.begin(), a.end(), res.begin());
			
			auto carry = integer::safe_add_helper(res.dyn_arr(), res.dyn_arr(), b.dyn_arr());

			if (carry) {
				res.set_overflow(true);
				res.m_data.push_back(static_cast<block_t>(carry));
			}
			
			res.trim_leading_zeros();
		}

		static constexpr auto sub_impl(
			BasicInteger& res,
			BasicInteger const& a,
			BasicInteger const& b,
			bool check_sign = true
		) noexcept -> void {
			if (check_sign && a.is_neg() != b.is_neg()) return add_impl(res, a, b, false);

			bool is_neg{false};

			auto const* lhs = &a.dyn_arr();
			auto const* rhs = &b.dyn_arr();
			
			auto ls = a.is_neg();
			auto rs = check_sign ? !b.is_neg() : b.is_neg();

			if (a.abs_less(b)) {
				std::swap(lhs, rhs);
				std::swap(ls, rs);
			}
			// Case 1: -ve - (-ve)
			//			= -ve + +ve
			//			= -(+ve - +ve)
			// Case 2: +ve - (+ve)
			//			= +ve - +ve
			is_neg = ls;

			auto size_required = std::max(a.size(), b.size()) + 1;
			res.resize(size_required, 0);
			res.m_bits = std::max(a.bits(), b.bits());
			
			std::copy(lhs->begin(), lhs->end(), res.begin());
			auto borrow = integer::safe_sub_helper(res.m_data, res.m_data, *rhs);

			res.trim_leading_zeros();
			
			res.set_is_neg(is_neg);
			if (borrow || is_neg) {
				res.set_underflow(true);
			}
		}

		static constexpr auto div_impl(
			BasicInteger& quot,
			BasicInteger& rem,
			BasicInteger const& num,
			BasicInteger const& den,
			DivKind kind
		) -> void {
			if (den.is_zero()) return;
			if (num.is_zero()) return;
			auto q_neg = false;
			auto r_neg = false;
			if (num.is_neg() && !den.is_neg()) {
				q_neg = true;
				r_neg = true;
			} else if (!num.is_neg() && den.is_neg()) {
				q_neg = true;
				r_neg = false;
			}
			if (den.bits() > num.bits()) {
				rem = num;
				rem.set_is_neg(r_neg);
				return;
			}

			// NOTE: Fast path
			if (den.is_power_of_two()) {
				auto bits = den.bits() - 1; 
				quot = num;
				quot.shift_right_mut(bits - 1);
				auto blocks = bits / BlockInfo::block_total_bits;
				auto extra = bits % BlockInfo::block_total_bits;
				auto has_extra = static_cast<bool>(extra);
				rem.resize(blocks + has_extra);
				
				for (auto i = 0zu; i < blocks; ++i) {
					rem.m_data[i] = num.m_data[i];
				}
				
				if (has_extra) {
					block_t const mask = (1 << extra) - 1;
					rem.m_data[blocks] = num.m_data[blocks] & mask; 
				}
			} else {
				auto const msz = std::max(3zu, num.size());
				quot.resize(msz, 0);
				rem.resize(msz, 0);
				quot.m_bits = num.bits();
				rem.m_bits = num.bits();

				auto scope = utils::TempAllocatorScope();
				if (!integer::fast_div(num, den, quot, rem)) {
					switch(kind) {
						case DivKind::LongDiv: case DivKind::Auto:
							integer::long_div(num, den, quot, rem);
							break;
						default: break;
					}
				}
			}

			quot.set_is_neg(q_neg);
			rem.set_is_neg(r_neg);
			quot.trim_leading_zeros();
			rem.trim_leading_zeros();
		}



	private:
		constexpr auto resize(std::size_t len, block_t val = {}) -> void {
			m_data.resize(len, val);
		}
		
		static constexpr auto parse_sign(std::string_view num) noexcept -> std::expected<std::pair<bool, std::size_t>, std::string> {
			auto is_neg = false;
			auto i = 0zu;
			auto sign_count = 0zu;
			for (; i < num.size(); ++i) {
				auto c = num[i];
				bool should_break {false};
				switch (c) {
					case ' ': break;
					case '-': is_neg = true;
					case '+':
						++sign_count;
						break;
					default: should_break = true; break; 
				}

				if (should_break) break;
			}

			if (sign_count > 1) {
				return std::unexpected("a number cannot have multiple signs.");
			}
		
			return { { is_neg, i } };
		}
		
		[[nodiscard]] static constexpr auto infer_radix(
			std::string_view num, 
			Radix expecting_radix
		) noexcept -> std::expected<std::pair<Radix, std::size_t>, std::string> {
			if (num.size() < 3) return { { Radix::Dec, 0 } };
			switch (expecting_radix) {
				case Radix::None: {
					auto const f = num[0];
					auto const r = num[1];
					if (f != '0') return { { Radix::Dec, 0 } };
					switch (r) {
						case 'b': return { { Radix::Binary, 2 } };	
						case 'o': return { { Radix::Octal, 2 } };	
						case 'x': return { { Radix::Hex, 2 } };	
						default: return {{ Radix::Dec, 0 }};
					}
				} break;
				case Radix::Dec: { 
					auto res = expect_radix_prefix(num, 0); 
					if (res.has_value()) return { { Radix::Dec, res.value() } };
					return std::unexpected(std::move(res.error()));
				}
				case Radix::Binary: { 
					auto res = expect_radix_prefix(num, 'b'); 
					if (res.has_value()) return { { Radix::Binary, res.value() } };
					return std::unexpected(std::move(res.error()));
				}
				case Radix::Octal: { 
					auto res = expect_radix_prefix(num, 'o'); 
					if (res.has_value()) return { { Radix::Octal, res.value() } };
					return std::unexpected(std::move(res.error()));
				}
				case Radix::Hex: { 
					auto res = expect_radix_prefix(num, 'x'); 
					if (res.has_value()) return { { Radix::Hex, res.value() } };
					return std::unexpected(std::move(res.error()));
				}
			}
		}

		static constexpr auto expect_radix_prefix(std::string_view num, char radix_char) noexcept -> std::expected<std::size_t, std::string> {
			if (!num.starts_with("0")) return 0;
			switch (num[1]) {
				case 'b': case 'o': case 'x': {
					if (radix_char == 0) {
						return std::unexpected(std::format("expected no radix prefix for decimal number, but found '0{}'", radix_char, num[1]));
					}
					if (num[1] != radix_char) {
						return std::unexpected(std::format("expected radix prefix '0{}', but found '0{}'", radix_char, num[1]));
					}
				} break;
			}
			return 2 - (radix_char == 0 ? 2 : 0);
		}

		static constexpr auto validate_digits(std::string_view num, Radix radix) noexcept -> std::expected<void, std::string> {
			constexpr auto is_digit = [](char c) {
				return c >= '0' && c <= '9';
			};

			constexpr auto is_oct = [](char c) {
				return c >= '0' && c <= '7';
			};
			
			constexpr auto is_hex = [is_digit](char c) {
				return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
			};

			constexpr auto is_binary = [](char c) {
				return c == '0' || c == '1';
			};

			auto pos = num.end();

			switch (radix) {
				case Radix::Dec: {
					pos = std::find_if(num.begin(), num.end(), [is_digit](char c) {
						return !is_digit(c);
					});
				} break;
				case Radix::Hex: {
					pos = std::find_if(num.begin(), num.end(), [is_hex](char c) {
						return !is_hex(c);
					});
				} break;
				case Radix::Octal: {
					pos = std::find_if(num.begin(), num.end(), [is_oct](char c) {
						return !is_oct(c);
					});
				} break;
				case Radix::Binary: {
					pos = std::find_if(num.begin(), num.end(), [is_binary](char c) {
						return !is_binary(c);
					});
				} break;

				case Radix::None: std::unreachable();
			}
			
			if (pos == num.end()) return {};
			
			return std::unexpected(std::format("invalid {} digit '{}'", to_string(radix), *pos));
			
		}

		inline static std::string sanitize_number(std::string_view num) {
			auto res = std::string();
			res.reserve(num.size());

			for (auto c: num) {
				if (c == '_') continue;
				res.push_back(c);
			}

			return res;
		}

		constexpr auto toggle_flag(bool s, BigNumFlag::type flag) -> void {
			if (s) m_flags |= flag;
			else m_flags &= ~flag;
		}

	private:
		base_type		m_data{};
		size_type		m_bits{};
		std::uint8_t	m_flags{};
	};

} // namespace dark::internal

namespace dark {
	using BigInteger = internal::BasicInteger;
} // namespace dark

#endif // DARK_BIG_NUM_BASIC_INTEGER_HPP
