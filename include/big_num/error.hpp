#ifndef DARK_BIG_NUM_ERROR_HPP
#define DARK_BIG_NUM_ERROR_HPP

#include <string_view>

namespace dark {

	enum class BigNumError {
		UnknownRadix,
		RadixPrefixMismatch,
		ExpectingHex,
		ExpectingBinary,
		ExpectingOctal,
		ExpectingDec,
		Overflow,
		Underflow,
		SizeMismatch,
		CannotResize,
		DivideByZero
	};
	
	constexpr std::string_view to_string(BigNumError e) noexcept {
		switch (e) {
			case BigNumError::UnknownRadix: return "UnknownRadix";
			case BigNumError::RadixPrefixMismatch: return "RadixPrefixMismatch";
			case BigNumError::ExpectingHex: return "ExpectingHex";
			case BigNumError::ExpectingBinary: return "ExpectingBinary";
			case BigNumError::ExpectingOctal: return "ExpectingOctal";
			case BigNumError::ExpectingDec: return "ExpectingDec";
			case BigNumError::Overflow: return "Overflow";
			case BigNumError::Underflow: return "Underflow";
			case BigNumError::SizeMismatch: return "SizeMismatch";
			case BigNumError::CannotResize: return "CannotResize";
			case BigNumError::DivideByZero: return "DivideByZero";

		}		
	}


} // namespace dark

#endif // DARK_BIG_NUM_ERROR_HPP
