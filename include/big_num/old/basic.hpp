#ifndef DARK_BIG_NUM_BASIC_HPP
#define DARK_BIG_NUM_BASIC_HPP

#include "block_info.hpp"
#include <algorithm>
#include <cstddef>
#include <string_view>

#ifndef BIG_NUM_NAIVE_THRESHOLD
	#define BIG_NUM_NAIVE_THRESHOLD 32
#endif

#ifndef BIG_NUM_KARATSUBA_THRESHOLD
	#define BIG_NUM_KARATSUBA_THRESHOLD 512
#endif

#ifndef BIG_NUM_TOOM_COOK_3_THRESHOLD
	#define BIG_NUM_TOOM_COOK_3_THRESHOLD 1024
#endif

#ifndef BIG_NUM_PARSE_NAIVE_THRESHOLD
	#define BIG_NUM_PARSE_NAIVE_THRESHOLD 2'000
#endif

#ifndef BIG_NUM_PARSE_DIVIDE_CONQUER_THRESHOLD
	#define BIG_NUM_PARSE_DIVIDE_CONQUER_THRESHOLD 100'000
#endif

namespace dark {
	enum class Radix: std::uint8_t {
		None = 0,
		Binary = 2,
		Octal = 8,
		Dec = 10,
		Hex = 16
	};

	constexpr std::string_view to_string(Radix r) noexcept {
		switch (r) {
			case Radix::None: return "unknown";
			case Radix::Binary: return "binary";
			case Radix::Octal: return "octal";
			case Radix::Dec: return "decimal";
			case Radix::Hex: return "hexadecimal";
		}
	}
	
	constexpr std::string_view get_prefix(Radix r) noexcept {
		switch (r) {
			case Radix::None: return "";
			case Radix::Binary: return "0b";
			case Radix::Octal: return "0o";
			case Radix::Dec: return "";
			case Radix::Hex: return "0x";
		}
	}

	enum class MulKind {
		Auto,
		Naive,
		Karatsuba,
		ToomCook3,
		NTT
	};

	enum class DivKind {
		Auto,
		LongDiv,
	};

	struct BigNumFlag {
		using type = std::uint8_t;
		static constexpr type None = 0;
		static constexpr type Neg = 1;
		static constexpr type Overflow = 2;
		static constexpr type Underflow = 4;
		static constexpr type IsSigned = 8;
	};


    namespace internal {
        inline static constexpr auto compute_used_bits(std::span<BlockInfo::type> bs) noexcept -> std::size_t {
            auto const sz = bs.size();
            if (sz == 0) return 0;

            constexpr auto cal = [](auto block, std::size_t left) {
				auto const bw = static_cast<std::size_t>(std::bit_width(block));
				return bw + BlockInfo::block_total_bits * (left);
            };

            constexpr auto block_size = 32zu;

			for (auto i = 0zu; i < sz; i += block_size) {
                auto const ib = std::min(block_size, sz - i);
				auto const idx = sz - 1 - i;
                for (auto j = 0zu; j < ib; ++j) {
                    auto block = bs[idx - j];
                    if (block) return cal(block, idx - j);
                }
			}
			return 0;
        }
    }
}

#endif // DARK_BIG_NUM_BASIC_HPP
