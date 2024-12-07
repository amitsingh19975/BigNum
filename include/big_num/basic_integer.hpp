#ifndef DARK_BIG_NUM_BASIC_INTEGER_HPP
#define DARK_BIG_NUM_BASIC_INTEGER_HPP

#include <algorithm>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <ostream>
#include <cstdint>
#include <format>
#include <print>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <optional>
#include <expected>
#include "big_num/bitwise.hpp"
#include "big_num/division.hpp"
#include "big_num/ntt.hpp"
#include "block_info.hpp"
#include "utils.hpp"
#include "mul.hpp"
#include "basic.hpp"

namespace dark::internal {

	class BasicInteger {
		static constexpr auto naive_threshold = 32zu;
		static constexpr auto karatsuba_threshold = 1024zu;
	public:

		using block_t = typename BlockInfo::type;
		using block_acc_t = typename BlockInfo::accumulator_t;

		using base_type = std::vector<block_t>;
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
			std::fill(m_data.begin(), m_data.end(), 0);
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

			std::vector<block_t> result(num.size(), 0);

			utils::convert_to_block_radix(result.data(), num, static_cast<std::uint8_t>(infered_radix));	

			while (!result.empty() && result.back() == 0) {
				result.pop_back();
			}

			auto res = BasicInteger{};
			res.set_zero();
			auto bits = std::size_t{};

			if (!result.empty()) {
				auto c = static_cast<std::size_t>(std::bit_width(result.back()));
				bits = (result.size() - 1) * BlockInfo::total_bits + c;
			}
			res.m_bits = bits;
			res.m_data.resize(result.size(), block_t{});

			std::copy_n(result.begin(), res.size(), res.begin());
			res.set_is_neg(is_neg);
			
			return res;
		}

		template <std::integral I>
		static auto from(
			I num
		) noexcept -> std::expected<BasicInteger, std::string> {
			using type = std::decay_t<std::remove_cvref_t<I>>;
			using unsigned_type = std::make_unsigned_t<type>;

			constexpr auto bytes = sizeof(unsigned_type);
			auto res = BasicInteger{};
			res.set_zero();
			constexpr auto bits = bytes * 8;
			res.m_bits = bits;
			
			res.m_data.resize(BlockInfo::calculate_blocks_from_bytes(bytes), block_t{});

			constexpr auto block_bits = BlockInfo::total_bits;
			bool is_neg{false};
			auto new_num = unsigned_type{};
			if constexpr (std::numeric_limits<type>::is_signed) {
				if (num < 0) {
					is_neg = true;
					num = -num;
				}
			}
			
			new_num = static_cast<unsigned_type>(num);

			for (auto i = 0zu, bk = 0zu; i < bits; i += block_bits, ++bk) {
				auto ib = std::min(block_bits, bits - i);
				auto const shift = bits - ib;
				auto mask = (~unsigned_type{}) >> shift;
				auto temp = new_num & mask;
				res.m_data[bk] = temp;
				num >>= shift;
			}

			res.set_is_neg(is_neg);
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

		friend std::ostream& operator<<(std::ostream& os, BasicInteger const& o) {
			return os << o.to_str();	
		}

		constexpr auto add(BasicInteger const& other) const -> BasicInteger {
			auto res = *this;
			res.add_mut(other);
			return res;
		}
		
		constexpr auto add_mut(BasicInteger const& other) noexcept -> BasicInteger& {
			add_impl(this, this, &other);
			return *this;
		}


		constexpr auto sub(BasicInteger const& other) const -> BasicInteger {
			auto res = *this;
			res.sub_mut(other);
			return res;
		}

		constexpr auto sub_mut(BasicInteger const& other) noexcept -> BasicInteger& {
			sub_impl(this, this, &other);
			return *this;	
		}
		
		constexpr auto mul(BasicInteger const& other, MulKind kind = MulKind::Auto) const -> BasicInteger {
			auto res = *this;
			res.mul_mut(other, kind);
			return res;
		}

		constexpr auto mul_mut(BasicInteger const& other, MulKind kind = MulKind::Auto) -> BasicInteger& {
			auto temp = *this;
			m_data.clear();
			mul_impl(this, &temp, &other, kind);
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

		constexpr auto operator+(BasicInteger const& other) const -> BasicInteger {
			return add(other);
		}

		constexpr auto operator-(BasicInteger const& other) const -> BasicInteger {
			return sub(other);
		}

		constexpr auto operator*(BasicInteger const& other) const -> BasicInteger {
			return mul(other);
		}

		constexpr auto operator/(BasicInteger const& other) const -> BasicInteger {
			return div(other).quot;
		}

		constexpr auto operator%(BasicInteger const& other) const -> BasicInteger {
			return div(other).rem;
		}
		
		constexpr auto operator+=(BasicInteger const& other) -> BasicInteger& {
			return add_mut(other);
		}

		constexpr auto operator-=(BasicInteger const& other) -> BasicInteger& {
			return sub_mut(other);
		}

		constexpr auto operator*=(BasicInteger const& other) -> BasicInteger& {
			mul_mut(other);
			return *this;
		}

		constexpr auto operator/=(BasicInteger const& other) -> BasicInteger& {
			auto [q, _] = div(other);
			swap(*this, q);
			return *this;
		}

		constexpr auto operator%(BasicInteger const& other) -> BasicInteger& {
			auto [_, r] = div(other);
			swap(*this, r);
			return *this;
		}

		constexpr auto operator|(BasicInteger const& other) const -> BasicInteger {
			return bitwise_or(other);
		}

		constexpr auto operator|=(BasicInteger const& other) -> BasicInteger& {
			return bitwise_or_mut(other);
		}

		constexpr auto operator&(BasicInteger const& other) const -> BasicInteger {
			return bitwise_and(other);
		}

		constexpr auto operator&=(BasicInteger const& other) -> BasicInteger& {
			return bitwise_and_mut(other);
		}

		constexpr auto operator-() -> BasicInteger {
			auto temp = *this;
			temp.set_is_neg(!temp.is_neg());
			return temp;
		}

		constexpr auto operator+() -> BasicInteger {
			return *this;
		}

		std::string to_str(Radix radix = Radix::Dec, bool with_prefix = false, std::optional<char> separator = {}) const {
			std::string s;

			if (is_zero()) {
				s = get_prefix(radix);
				s += "0";
				return s;
			}
			auto get_digit = [this](auto i) { return m_data[size() - i - 1]; };

			switch (radix) {
				case Radix::Binary: {
					s.resize(bits() + 2);
					utils::basic_convert_to_block_radix<2>(
						s.data(),
						BlockInfo::max_value,
						size(),
						get_digit
					);
				} break;
				case Radix::Octal: {
					s.resize(bits() + 2);
					utils::basic_convert_to_block_radix<8>(
						s.data(),
						BlockInfo::max_value,
						size(),
						get_digit
					);
				} break;
				case Radix::Hex: {
					s.resize(bits() / 4 + 2, 0);
					utils::basic_convert_to_block_radix<16>(
						s.data(),
						BlockInfo::max_value,
						size(),
						get_digit
					);
				} break;
				case Radix::Dec: case Radix::None: {
					s.resize(bits() / 2 + 2, 0);
					utils::basic_convert_to_block_radix<10>(
						s.data(),
						BlockInfo::max_value,
						size(),
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

		constexpr auto operator==(BasicInteger const& other) const noexcept -> bool {
			if (other.is_neg() != is_neg()) return false;
			if (other.bits() != bits()) return false;
			return std::equal(begin(), end(), other.begin());
		}
		
		constexpr auto operator!=(BasicInteger const& other) const noexcept -> bool {
			return !(*this == other);
		}

		friend constexpr auto operator==(BasicInteger const& lhs, std::string_view s) -> bool {
			auto rhs = BasicInteger(s); 
			return lhs == rhs;
		}

		friend constexpr auto operator==(std::string_view s, BasicInteger const& rhs) -> bool {
			return (rhs == s);
		}

		friend constexpr auto operator!=(BasicInteger const& lhs, std::string_view s) -> bool {
			return !(lhs == s);
		}

		friend constexpr auto operator!=(std::string_view s, BasicInteger const& rhs) -> bool {
			return !(rhs == s);
		}

		template <std::integral I>
		friend constexpr auto operator==(BasicInteger const& lhs, I s) -> bool {
			auto rhs = BasicInteger(s); 
			return lhs == rhs;
		}

		template <std::integral I>
		friend constexpr auto operator==(I s, BasicInteger const& rhs) -> bool {
			return (rhs == s);
		}

		template <std::integral I>
		friend constexpr auto operator!=(BasicInteger const& lhs, I s) -> bool {
			return !(lhs == s);
		}

		template <std::integral I>
		friend constexpr auto operator!=(I s, BasicInteger const& rhs) -> bool {
			return !(rhs == s);
		}

		constexpr auto operator<(BasicInteger const& other) const noexcept -> bool {
			if (is_neg() && !other.is_neg()) return true;
			else return abs_less(other);
		}
		
		constexpr auto operator<=(BasicInteger const& other) const noexcept -> bool {
			if (is_neg() && !other.is_neg()) return true;
			else if (bits() < other.bits()) return true;
			else if (bits() > other.bits()) return false;
			return compare(other, std::less_equal<>{});
		}
		
		constexpr auto operator>(BasicInteger const& other) const noexcept -> bool {
			if (!is_neg() && other.is_neg()) return true;
			else if (bits() > other.bits()) return true;
			else if (bits() < other.bits()) return false;
			return compare(other, std::greater<>{});
		}

		constexpr auto operator>=(BasicInteger const& other) const noexcept -> bool {
			if (!is_neg() && other.is_neg()) return true;
			else if (bits() > other.bits()) return true;
			else if (bits() < other.bits()) return false;
			return compare(other, std::greater_equal<>{});
		}

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

		template <std::integral T>
			requires (!std::numeric_limits<T>::is_signed)
		constexpr auto operator<<(T shift) const -> BasicInteger {
			return shift_left(shift);
		}

		template <std::integral T>
			requires (!std::numeric_limits<T>::is_signed)
		constexpr auto operator<<=(T shift) -> BasicInteger& {
			return shift_left_mut(shift);
		}

		template <std::integral T>
			requires (!std::numeric_limits<T>::is_signed)
		constexpr auto operator>>(T shift) const noexcept -> BasicInteger {
			return shift_right(shift);
		}

		template <std::integral T>
			requires (!std::numeric_limits<T>::is_signed)
		constexpr auto operator>>=(T shift) noexcept -> BasicInteger& {
			return shift_right_mut(shift);
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

		constexpr auto set_bit(std::size_t pos, bool flag, bool should_expand = false) -> void {
			auto index = pos / BlockInfo::total_bits;
			if (index >= size()) {
				if (!should_expand) return;
				resize(index + 1, 0);
			}
			pos = pos % BlockInfo::total_bits;
			
			auto& block = m_data[index];
			auto bit = block_t{flag};
			if (flag) block |= (bit << pos);
			else block &= ~(bit << pos);

			trim_zero();
		}

		constexpr auto get_bit(std::size_t pos) const noexcept -> bool {
			auto index = pos / BlockInfo::total_bits;
			if (index >= size()) return false;
			pos = pos % BlockInfo::total_bits;
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
	private:
		static constexpr auto shift_left_helper(
			BasicInteger& out,
			std::size_t shift,
			bool should_extend
		) -> void {
			if (should_extend) {
				auto const diff_bits = shift;
				auto const diff_size = (diff_bits + BlockInfo::total_bits - 1) / BlockInfo::total_bits;
				out.m_data.resize(out.size() + diff_size, 0);
			}
			logical_left_shift(out.data(), out.size(), shift);
			out.trim_zero();
		}

		static constexpr auto shift_right_helper(
			BasicInteger& out,
			std::size_t shift
		) noexcept -> void {
			logical_right_shift(out.data(), out.size(), shift);
			out.trim_zero();
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
			out.trim_zero();
		}
		
		template <typename Fn>
		constexpr auto compare(BasicInteger const& other, Fn fn) const noexcept -> bool {
			assert(bits() == other.bits());
			auto const sz = size();
			auto l = block_t{};
			auto r = block_t{};
			for (auto i = sz; i > 0; --i) {
				l = m_data[i - 1];
				r = other.m_data[i - 1];
				if (l != r) break;;
			}
			return fn(l, r);
		}
		
		constexpr auto abs_less(BasicInteger const& other) const noexcept -> bool {
			if (bits() < other.bits()) return true;
			else if (bits() > other.bits()) return false;
			return compare(other, std::less<>{});
		} 

		static constexpr auto add_impl(
			BasicInteger* res,
			BasicInteger const* a,
			BasicInteger const* b,
			bool check_sign = true
		) noexcept -> void {
			if (check_sign && a->is_neg() != b->is_neg()) return sub_impl(res, a, b, false);

			auto size_required = std::max(a->size(), b->size());
			res->m_data.resize(size_required, 0);
			res->m_bits = std::max(a->bits(), b->bits());

			res->set_is_neg(a->is_neg());
			
			auto carry = block_acc_t{};
			auto* lhs = res->m_data.data();

			if (a->abs_less(*b)) {
				std::swap(a, b);
			}

			auto size = std::min(a->size(), b->size());

			auto i = 0zu;
			auto const* a_data = a->m_data.data();
			auto const* b_data = b->m_data.data(); 

			for (; i < size; ++i) {
				auto [acc, c] = integer::safe_add_helper(a_data[i], b_data[i] + carry);
				lhs[i] = acc;
				carry = c;
			}

			size = a->size();
			for (; i < size; ++i) {
				auto [acc, c] = integer::safe_add_helper(a_data[i], carry);
				lhs[i] = acc;
				carry = c;
			}

			if (carry) {
				res->set_overflow(true);
				res->m_data.push_back(static_cast<block_t>(carry));
			}
			
			res->trim_zero();
		}

		static constexpr auto sub_impl(
			BasicInteger* res,
			BasicInteger const* a,
			BasicInteger const* b,
			bool check_sign = true
		) noexcept -> void {
			if (check_sign && a->is_neg() != b->is_neg()) return add_impl(res, a, b, false);

			auto size_required = std::max(a->size(), b->size());
			res->m_data.resize(size_required, 0);
			res->m_bits = std::max(a->bits(), b->bits());

			auto borrow = block_acc_t{};
			bool is_neg{false};
			
			auto* lhs = res->m_data.data();

			auto ls = a->is_neg();
			auto rs = !b->is_neg();
			if (a->abs_less(*b)) {
				std::swap(a, b);
				std::swap(ls, rs);
			}
			// Case 1: -ve - (-ve)
			//			= -ve + +ve
			//			= -(+ve - +ve)
			// Case 2: +ve - (+ve)
			//			= +ve - +ve
			is_neg = ls;
			/*std::println("{} | {} => {} {}", *a, *b, a->bits(), b->bits());*/

			auto size = std::min(a->size(), b->size());
		
			auto i = 0zu;
			auto const* a_data = a->m_data.data();
			auto const* b_data = b->m_data.data();

			for (; i < size; ++i) {
				auto minuend = static_cast<block_acc_t>(a_data[i]);
				auto subtrahend = static_cast<block_acc_t>(b_data[i]) + borrow;
				auto [acc, br] = integer::safe_sub_helper(minuend, subtrahend);
				lhs[i] = acc;
				borrow = br;
			}

			size = a->size();
			for (; i < size; ++i) {
				auto minuend = static_cast<block_acc_t>(a_data[i]);
				auto subtrahend = borrow;
				auto [acc, br] = integer::safe_sub_helper(minuend, subtrahend);
				lhs[i] = acc;
				borrow = br;
			}
			res->trim_zero();
			
			res->set_is_neg(is_neg);
			if (borrow || is_neg) {
				res->set_underflow(true);
			}
		}

		static constexpr auto mul_ntt_impl(
			BasicInteger* res,
			BasicInteger const* a,
			BasicInteger const* b
		) noexcept -> void {
			auto const size = impl::NTT::calculate_max_operands_size(a->size(), b->size());

			res->resize(size << 1);
			res->m_bits = a->bits() + b->bits();

			impl::NTT::mul(res->data(), size << 1, a->data(), a->size(), b->data(), b->size(), size);
		}

		static constexpr auto mul_power_of_two(
			BasicInteger& res,
			BasicInteger const& a,
			BasicInteger const& pow_2
		) -> void {
			res = a;
			auto b = pow_2.m_data.back();
			auto k = static_cast<std::size_t>(std::bit_width(b)) + (pow_2.size() - 1) * BlockInfo::total_bits;
			res.shift_left_mut(k, true);
		}

		static constexpr auto mul_impl(
			BasicInteger* res,
			BasicInteger const* a,
			BasicInteger const* b,
			MulKind kind
		) -> void {
			if (a->is_zero() || b->is_zero()) {
				res->m_data.clear();
				res->m_bits = 0;
				res->m_flags = 0;
				return;
			}
			
			// fast-path
			if (a->is_power_of_two()) {
				mul_power_of_two(*res, *b, *a); 
				return;
			} else if (b->is_power_of_two()) {
				mul_power_of_two(*res, *a, *b);
				return;
			}

			res->m_bits = a->bits() + b->bits();

			auto const a_size = a->size();
			auto const b_size = b->size();
			auto res_actual_size = a_size + b_size;

			auto const naive_mul_impl = [&] {
				res->resize(res_actual_size, 0);
				integer::naive_mul(
					res->data(),
					res->size(),
					a->data(),
					a->size(),
					b->data(),
					b->size()
				);
			};

			auto const karatsub_mul_impl = [&] {
				auto temp_size = std::max(a_size, b_size);
				res->resize(temp_size << 1, 0);
				integer::karatsuba_mul<karatsuba_threshold, naive_threshold>(
					res->data(),
					res->size(),
					a->data(),
					a->size(),
					b->data(),
					b->size()
				);

				res->resize(res_actual_size);
			};

			auto const ntt_impl = [&] {
				mul_ntt_impl(
					res,
					a,
					b
				);
			};
			
			switch (kind) {
				case MulKind::Auto: {
					if (a_size <= naive_threshold && b_size <= naive_threshold) naive_mul_impl();
					else if (a_size <= karatsuba_threshold && b_size <= karatsuba_threshold) karatsub_mul_impl();
					else ntt_impl();
				} break;
				case MulKind::Naive: naive_mul_impl(); break;
				case MulKind::Karatsuba: karatsub_mul_impl(); break;
				case MulKind::NTT: ntt_impl(); break;
			}


			res->set_is_neg(a->is_neg() || b->is_neg());
			res->trim_zero();
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
			if (den.bits() > num.bits()) {
				rem = num;
				return;
			}

			// NOTE: Fast path
			if (den.is_power_of_two()) {
				auto bits = den.bits() - 1; 
				quot = num;
				quot.shift_right_mut(bits - 1);
				auto blocks = bits / BlockInfo::total_bits;
				auto extra = bits % BlockInfo::total_bits;
				auto has_extra = static_cast<bool>(extra);
				rem.resize(blocks + has_extra);
				
				for (auto i = 0zu; i < blocks; ++i) {
					rem.m_data[i] = num.m_data[i];
				}
				
				if (has_extra) {
					block_t const mask = (1 << extra) - 1;
					rem.m_data[blocks] = num.m_data[blocks] & mask; 
				}
				
				quot.trim_zero();
				rem.trim_zero();

				return;
			}

			quot.resize(num.size(), 0);
			rem.resize(num.size(), 0);
			quot.m_bits = num.bits();
			rem.m_bits = num.bits();

			
			switch(kind) {
				case DivKind::LongDiv: case DivKind::Auto:
					integer::long_div(num, den, quot, rem);
					break;
				default: break;
			}
			quot.trim_zero();
			rem.trim_zero();
		}

		constexpr auto trim_zero() noexcept -> void {
			while (!m_data.empty() && m_data.back() == 0) m_data.pop_back();
			if (m_data.empty()) {
				set_is_neg(false);
				m_bits = 0;
				return;
			}
			auto bits = static_cast<std::size_t>(std::bit_width(m_data.back()));
			m_bits = (size() - 1) * BlockInfo::total_bits + bits;
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
		base_type		m_data;
		size_type		m_bits{};
		std::uint8_t	m_flags{};
	};

} // namespace dark::internal

namespace dark {
	using BigInteger = internal::BasicInteger;
} // namespace dark

#endif // DARK_BIG_NUM_BASIC_INTEGER_HPP
