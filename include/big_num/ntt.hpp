#ifndef AMT_DARK_BIG_NUM_NTT_HPP
#define AMT_DARK_BIG_NUM_NTT_HPP

#include "big_num/block_info.hpp"
#include <cassert>
#include <vector>
#include <bit>
#include <print>

// Ref: https://codeforces.com/blog/entry/129600

namespace dark::internal::impl {


	[[nodiscard]] constexpr auto binary_pow(
		typename BlockInfo::accumulator_t n,
		typename BlockInfo::accumulator_t p
	) noexcept -> typename BlockInfo::accumulator_t {
		using acc_t = typename BlockInfo::accumulator_t;
		constexpr auto mod = BlockInfo::mod;

		acc_t res{1};

		while (p) {
			if (p & 1) res = (res * n) % mod;
			n = (n * n) % mod;
			p >>= 1;
		}

		return res;
	}

	struct Montgomery {
		using size_type = std::size_t;
		using type = typename BlockInfo::type;
		using acc_t = typename BlockInfo::accumulator_t;
		static constexpr auto half_bits = (BlockInfo::total_bits >> 1);
		static constexpr auto half_mask = (type{1} << half_bits) - 1;

		static constexpr auto n = BlockInfo::mod;
		static constexpr auto nr = []{
			acc_t nr = 1;

			// INFO: Use Newton-Raphson iteration to find `nr` (modular multiplicative inverse)
			// total iterations required log2(number_of_bits) = log2(64) = 6	
			for (auto i = 0u; i < 6; ++i) {
				nr *= 2 - n * nr; 
				nr &= BlockInfo::ntt_lower_mask; 
			}
			return nr;
		}();

		[[nodiscard]] constexpr auto reduce(acc_t x) const noexcept -> type {
			type q = (nr * x) & BlockInfo::ntt_lower_mask;
			type m = ((q * acc_t{n}) >> BlockInfo::ntt_total_bits) & BlockInfo::ntt_lower_mask;
			type res = ((x >> BlockInfo::ntt_total_bits) + n - m) & BlockInfo::ntt_lower_mask;
			res -= n * (acc_t{res} >= n);
			return res;
		}


		[[nodiscard]] constexpr auto multply(acc_t x, acc_t y) const noexcept -> type {
			return reduce(x * y);
		}

		[[nodiscard]] constexpr auto transform(acc_t x) const noexcept -> type {
			return ((x << BlockInfo::ntt_total_bits) % n) & BlockInfo::ntt_lower_mask;
		}
	};

	static inline auto bit_rev_permutation(std::vector<std::size_t>& out, std::size_t n) {
		auto const bits = static_cast<std::size_t>(std::countr_zero(n));

		for (auto i = 0zu; i < n; ++i) {
			out[i] = (i & 1) * (1 << (bits - 1)) | (out[i>>1] >> 1);
		}
	}

	struct NTT {
		using type = typename BlockInfo::type;
		using acc_t = typename BlockInfo::accumulator_t;

		NTT(
			Montgomery const& mon,
			std::size_t n
		)
			: m_size(n)
			, m_permutation(m_size, 0)
			, m_W(m_size, mon.transform(1)) // Store both root [0, n / 2) and inverse root [n / 2, n)
			, m_inv_n(mon.transform(binary_pow(static_cast<type>(m_size), BlockInfo::mod - 2)))
			, m_root(mon.transform(binary_pow(BlockInfo::generator, (BlockInfo::mod - 1) >> (std::countr_zero(m_size)))))
			, m_inv_root(mon.transform(binary_pow(mon.reduce(m_root), BlockInfo::mod - 2)))
		{
			
			precomputeW(mon);
			bit_rev_permutation(m_permutation, m_size);
		}

		static constexpr auto calculate_max_operands_size(std::size_t lhs, std::size_t rhs) noexcept -> std::size_t {
			if (lhs == 0 || rhs == 0) return 0;
			std::size_t n{1};
			
			while (n < lhs || n < rhs) {
				n <<= 1;
			}
			
			n <<= 1;

			return n;
		}

		static auto mul(
			BlockInfo::blocks_t& out,
			BlockInfo::blocks_t const& a,
			BlockInfo::blocks_t const& b
		) -> void {

			auto n = calculate_max_operands_size(a.size(), b.size());
			out.resize(n << 1);
			auto const na = a.size();
			auto const nb = b.size();
			n = std::max(cal_block_size(na, n), cal_block_size(nb, n));
			
			std::vector<type> va(n, 0);
			std::vector<type> vb(n, 0);
			std::vector<type> res(n, 0);

			Montgomery mon;
			copy_to_half_block(mon, va.data(), a.data(), na);
			copy_to_half_block(mon, vb.data(), b.data(), nb);


			auto const ntt = NTT(mon, n);
			ntt(mon, va.data(), false);
			ntt(mon, vb.data(), false);

			for (auto i = 0zu; i < n; ++i) {
				res[i] = mon.multply(va[i], vb[i]);
			}

			ntt(mon, res.data(), true);

			copy_to_full_block(mon, out.data(), res.data(), res.size());
		}
	private:
		auto operator()(
			Montgomery const& mon,
			type* data,
			bool is_inverse
		) const noexcept -> void {
			
			for (auto i = 0zu; i < m_size; ++i) {
				auto const p = m_permutation[i];
				if (i < p) {
					std::swap(data[i], data[p]);
				}
			}

			auto const bits = static_cast<std::size_t>(std::countr_zero(m_size));
			auto const ws = m_W.data() + (is_inverse ? (m_size >> 1) : 0zu); 
			
			
			for (auto i = 0zu; i < bits; ++i) {
				auto const pos = 1zu << i;
				for (auto j = 0zu; j < m_size; ++j) {
					if (j & pos) continue;
					auto& el = data[j^pos];
					auto w_f = j & (pos - 1);
					auto w_s = m_size >> (i + 1);
					auto w_idx = w_f * w_s;
					auto w = ws[w_idx];

					acc_t const t = mon.multply(el, w);

					auto& ej = data[j];
					el = (ej + BlockInfo::mod * (ej < t) - t) & BlockInfo::ntt_lower_mask;

					auto const temp_sum = ej + t;
					ej = (temp_sum - BlockInfo::mod * (temp_sum > BlockInfo::mod)) & BlockInfo::ntt_lower_mask;
				}
			}

			if (is_inverse) {
				for (auto i = 0zu; i < m_size; ++i) {
					data[i] = mon.multply(data[i], m_inv_n);
				}
			}
		}

		constexpr auto precomputeW(
			Montgomery const& mon
		) -> void {
			auto const half = m_size >> 1;

			// Calculate roots: w^0, w^1, w^2, ....
			for (auto i = 1zu; i < half; ++i) {
				m_W[i] = mon.multply(m_W[i - 1], m_root);
			}

			auto ws = m_W.data() + half;
			// Calculate roots: w^0, w^-1, w^-2, ....
			for (auto i = 1zu; i < half; ++i) {
				ws[i] = mon.multply(ws[i - 1], m_inv_root);
			}
		}

		static constexpr auto cal_block_size(std::size_t n, std::size_t allocated_n) noexcept -> std::size_t {
			auto size = (n * BlockInfo::total_bits + Montgomery::half_bits  - 1) / Montgomery::half_bits;
			if (size <= allocated_n) return allocated_n;
			return neareast_power_of_2(size);
		}

		static constexpr auto copy_to_half_block(Montgomery const& mon, type* out, type const* in, std::size_t n) noexcept -> void {
			auto j = 0zu;
			for (auto i = 0zu; i < n; ++i) {
				auto el = in[i];
				auto low = el & Montgomery::half_mask;
				auto high = el >> Montgomery::half_bits;
				out[j + 0] = mon.transform(low);
				out[j + 1] = mon.transform(high);
				j += 2;
			}
		}

		static constexpr auto copy_to_full_block(Montgomery const& mon, type* out, type* in, std::size_t n) noexcept -> void {
			auto carry = acc_t{};
			for (auto i = 0zu; i < n; ++i) {
				auto r = mon.reduce(in[i]);
				auto temp = r + carry;
				in[i] = temp & Montgomery::half_mask;
				carry = temp >> Montgomery::half_bits;
			}

			auto j = 0zu;
			for (auto i = 0zu; i < n; i += 2) {
				auto low = in[i + 0];
				auto high = in[i + 1];
				auto temp = (high << Montgomery::half_bits) | low;
				out[j++] = temp;
			}
			out[j] = carry & BlockInfo::lower_mask;
		}

	private:
		std::size_t m_size;
		std::vector<std::size_t> m_permutation;
		std::vector<typename BlockInfo::type> m_W;
		typename BlockInfo::type m_inv_n;
		typename BlockInfo::type m_root;
		typename BlockInfo::type m_inv_root;
	};

} // namespace dark::internal::impl

#endif // AMT_DARK_BIG_NUM_NTT_HPP 
