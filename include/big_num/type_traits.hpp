#ifndef DARK_BIG_NUM_TYPE_TRAITS_HPP
#define DARK_BIG_NUM_TYPE_TRAITS_HPP

#include <type_traits>

namespace dark {

	namespace internal {
		template <std::size_t Bits, bool IsSigned, bool AllowOverflow>
		class BasicInteger;

		namespace detail {
			template <typename T>
			struct is_basic_integer: std::false_type{};

			template <std::size_t W, bool S, bool A>
			struct is_basic_integer<BasicInteger<W, S, A>>: std::true_type {};

			template <typename T>
			struct is_signed_basic_integer: std::false_type{};

			template <std::size_t W, bool A>
			struct is_signed_basic_integer<BasicInteger<W, true, A>>: std::true_type {};
		}

		template <typename T>
		concept is_basic_integer = detail::is_basic_integer<std::decay_t<std::remove_cvref_t<T>>>::value;

		template <typename T>
		concept is_signed_basic_integer = detail::is_basic_integer<std::decay_t<std::remove_cvref_t<T>>>::value;
	}

} // namespace dark

#endif // DARK_BIG_NUM_TYPE_TRAITS_HPP
