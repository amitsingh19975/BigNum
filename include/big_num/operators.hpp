#ifndef DARK_BIG_NUM_OPERATORS_HPP
#define DARK_BIG_NUM_OPERATORS_HPP

#include "type_traits.hpp"
#include <cassert>
#include <concepts>
#include <type_traits>
#include <functional>

namespace dark::internal {

	template <typename A>
	concept is_valid_big_integer_arg = (
		std::constructible_from<std::string_view, A> ||
		std::integral<A>
	);

	template <typename L, typename R>
	concept is_valid_big_num_mut_operator_arg = (
		(is_basic_integer<L> && ( is_basic_integer<R> || is_valid_big_integer_arg<R>))
	);

	template <typename L, typename R>
	concept is_valid_big_num_operator_arg = (
		is_valid_big_num_mut_operator_arg<L, R> ||
		is_valid_big_num_mut_operator_arg<R, L>
	);

	constexpr auto compare(
		is_basic_integer auto const& lhs,
		is_basic_integer auto const& rhs,
		auto&& fn
	) noexcept -> bool {
		using lhs_t = std::decay_t<decltype(lhs)>;
		assert(lhs.bits() == rhs.bits());
		auto const sz = std::min(lhs.size(), rhs.size());
		auto l = typename lhs_t::block_t{};
		auto r = typename lhs_t::block_t{};
		auto const& ld = lhs.dyn_arr();
		auto const& rd = rhs.dyn_arr();
		for (auto i = sz; i > 0; --i) {
			l = ld[i - 1];
			r = rd[i - 1];
			if (l != r) break;
		}
		return fn(l, r);
	}


} // namespace dark::internal

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr auto operator==(L const& lhs, R const& rhs) noexcept -> bool {
	using dark::internal::is_basic_integer;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		if (rhs.is_neg() != lhs.is_neg()) return false;
		if (rhs.bits() != lhs.bits()) return false;
		return std::equal(lhs.begin(), lhs.end(), rhs.begin());
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs == r;
	} else {
		auto l = rhs_t(lhs);
		return l == rhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr auto operator!=(L const& lhs, R const& rhs) noexcept -> bool {
	return !(lhs != rhs);
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr auto operator<(L const& lhs, R const& rhs) noexcept -> bool {
	using dark::internal::is_basic_integer;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		if (lhs.is_neg() && !rhs.is_neg()) return true;
		else if (lhs.bits() < rhs.bits()) return true;
		else if (lhs.bits() > rhs.bits()) return false;
		return compare(lhs, rhs, std::less_equal<>{});
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs < r;
	} else {
		auto l = rhs_t(lhs);
		return l < rhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr auto operator<=(L const& lhs, R const& rhs) noexcept -> bool {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		if (lhs.is_neg() && !rhs.is_neg()) return true;
		else if (lhs.bits() < rhs.bits()) return true;
		else if (lhs.bits() > rhs.bits()) return false;
		return compare(lhs, rhs, std::less_equal<>{});
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs <= r;
	} else {
		auto l = rhs_t(lhs);
		return l <= rhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr auto operator>(L const& lhs, R const& rhs) noexcept -> bool {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		if (!lhs.is_neg() && rhs.is_neg()) return true;
		else if (lhs.bits() > rhs.bits()) return true;
		else if (lhs.bits() < rhs.bits()) return false;
		return compare(lhs, rhs, std::greater<>{});
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs > r;
	} else {
		auto l = rhs_t(lhs);
		return l > rhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr auto operator>=(L const& lhs, R const& rhs) noexcept -> bool {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		if (!lhs.is_neg() && rhs.is_neg()) return true;
		else if (lhs.bits() > rhs.bits()) return true;
		else if (lhs.bits() < rhs.bits()) return false;
		return compare(lhs, rhs, std::greater_equal<>{});
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs >= r;
	} else {
		auto l = rhs_t(lhs);
		return l >= rhs;
	}
}

template <typename L, typename R>
	requires (dark::internal::is_basic_integer<L> && std::integral<R>)
constexpr decltype(auto) operator<<(L const& lhs, R rhs) noexcept {
	assert(rhs >= 0);
	auto shift = static_cast<std::size_t>(rhs);
	return lhs.shift_left(shift, true);
}

template <typename L, typename R>
	requires (dark::internal::is_basic_integer<L> && std::integral<R>)
constexpr decltype(auto) operator<<=(L& lhs, R rhs) noexcept {
	assert(rhs >= 0);
	auto shift = static_cast<std::size_t>(rhs);
	return lhs.shift_left_mut(shift, true);
}

template <typename L, typename R>
	requires (dark::internal::is_basic_integer<L> && std::integral<R>)
constexpr decltype(auto) operator>>(L const& lhs, R rhs) noexcept {
	assert(rhs >= 0);
	auto shift = static_cast<std::size_t>(rhs);
	return lhs.shift_right(shift);
}

template <typename L, typename R>
	requires (dark::internal::is_basic_integer<L> && std::integral<R>)
constexpr decltype(auto) operator>>=(L& lhs, R rhs) noexcept {
	assert(rhs >= 0);
	auto shift = static_cast<std::size_t>(rhs);
	return lhs.shift_right_mut(shift);
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr decltype(auto) operator+(L const& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.add(rhs);
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs + r;
	} else {
		auto l = rhs_t(lhs);
		return l + rhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_mut_operator_arg<L, R>
constexpr decltype(auto) operator+=(L& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.add_mut(rhs);
	} else {
		auto r = lhs_t(rhs);
		lhs += r;
		return lhs;
	}
}


template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr decltype(auto) operator-(L const& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.sub(rhs);
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs - r;
	} else {
		auto l = rhs_t(lhs);
		return l - rhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_mut_operator_arg<L, R>
constexpr decltype(auto) operator-=(L& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.sub_mut(rhs);
	} else {
		auto r = lhs_t(rhs);
		lhs -= r;
		return lhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr decltype(auto) operator*(L const& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.mul(rhs);
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs * r;
	} else {
		auto l = rhs_t(lhs);
		return l * rhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_mut_operator_arg<L, R>
constexpr decltype(auto) operator*=(L& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.mul_mut(rhs);
	} else {
		auto r = lhs_t(rhs);
		lhs *= r;
		return lhs;
	}
}


template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr decltype(auto) operator/(L const& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.div(rhs).quot;
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs / r;
	} else {
		auto l = rhs_t(lhs);
		return l / rhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_mut_operator_arg<L, R>
constexpr decltype(auto) operator/=(L& lhs, R const& rhs) {
	auto res = lhs / rhs;
	lhs = std::move(res);
	return res;
}


template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr decltype(auto) operator%(L const& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.div(rhs).rem;
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs % r;
	} else {
		auto l = rhs_t(lhs);
		return l % rhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_mut_operator_arg<L, R>
constexpr decltype(auto) operator%=(L& lhs, R const& rhs) {
	auto res = lhs % rhs;
	lhs = std::move(res);
	return res;
}


template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr decltype(auto) operator|(L const& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.bitwise_or(rhs);
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs | r;
	} else {
		auto l = rhs_t(lhs);
		return l | rhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_mut_operator_arg<L, R>
constexpr decltype(auto) operator|=(L& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.bitwise_or_mut(rhs);
	} else {
		auto r = lhs_t(rhs);
		lhs |= r;
		return lhs;
	}
}


template <typename L, typename R>
	requires dark::internal::is_valid_big_num_operator_arg<L, R>
constexpr decltype(auto) operator&(L const& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;
	using rhs_t = std::decay_t<R>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.bitwise_and(rhs);
	} else if constexpr (is_basic_integer<L> && !is_basic_integer<R>) {
		auto r = lhs_t(rhs);
		return lhs & r;
	} else {
		auto l = rhs_t(lhs);
		return l & rhs;
	}
}

template <typename L, typename R>
	requires dark::internal::is_valid_big_num_mut_operator_arg<L, R>
constexpr decltype(auto) operator&=(L& lhs, R const& rhs) {
	using namespace dark::internal;
	using lhs_t = std::decay_t<L>;

	if constexpr (is_basic_integer<L> && is_basic_integer<R>) {
		return lhs.bitwise_and_mut(rhs);
	} else {
		auto r = lhs_t(rhs);
		lhs &= r;
		return lhs;
	}
}

constexpr auto operator-(dark::internal::is_basic_integer auto& lhs) {
	auto temp = lhs;
	temp.set_is_neg(!temp.is_neg());
	return temp;
}

constexpr auto operator+(dark::internal::is_basic_integer auto& lhs) {
	return lhs;
}


#endif // DARK_BIG_NUM_OPERATORS_HPP
