#ifndef DARK_BIG_NUM_MUL_HPP
#define DARK_BIG_NUM_MUL_HPP

#include "allocator.hpp"
#include "basic.hpp"
#include "big_num/ntt.hpp"
#include "type_traits.hpp"
#include "dyn_array.hpp"
#include "block_info.hpp"
#include <algorithm>
#include <bit>
#include <cassert>
#include <type_traits>
#include "operators.hpp"
#include "add_sub.hpp"


namespace dark::internal::integer {
	inline static constexpr auto all_zeros(BlockInfo::blocks_t const& b) noexcept -> bool {
		constexpr auto is_zero = [](auto n) { return n == 0; };
		return std::all_of(b.rbegin(), b.rend(), is_zero);
	}

	inline static constexpr auto naive_mul(
		BlockInfo::blocks_t& out,
		BlockInfo::blocks_t const& lhs,
		BlockInfo::blocks_t const& rhs
	) noexcept -> void {
	using acc_t = BlockInfo::accumulator_t;
	if (all_zeros(lhs) || all_zeros(rhs)) {
		out.clear();
		return;
	}

	out.resize(lhs.size() + rhs.size());

	auto const lhs_size = lhs.size();
	auto const rhs_size = rhs.size();

	for (auto i = 0zu; i < lhs_size; ++i) {
		auto l = acc_t{lhs[i]};

		auto carry = acc_t {};

		for (auto j = 0zu; j < rhs_size; ++j) {
			auto r = acc_t{rhs[j]};

			auto [acc, c] = safe_add_helper(out[i + j], l * r + carry);

			out[i + j] = acc;
			carry = c;
		}
		
		auto temp = out.to_borrowed(rhs_size + i);
		safe_add_helper(temp, carry);
	}
	}

	template <std::size_t NaiveThreshold>
	inline static constexpr auto karatsuba_mul_helper(
		BlockInfo::blocks_t& out,
		BlockInfo::blocks_t const& lhs,
		BlockInfo::blocks_t const& rhs,
		std::size_t size,
		std::size_t depth = 0
	) noexcept -> void {
		using block_t = typename BlockInfo::type;
		using acc_t = typename BlockInfo::accumulator_t;

		if (size <= NaiveThreshold) {
			auto old_size = out.size();
			naive_mul(out, lhs, rhs);
			out.resize(old_size, 0);
			return;
		}

		// lhs * rhs = z2 * (2^b)^2 + z1 * (2^b) + z0
		// z0 = x_l * y_l
		// z1 = z3 - z0 - z2
		// z2 = x_u * y_u
		// z3 = (x_l + x_u) * (y_l + y_u)
		
		auto const half = size >> 1;
		auto const low = half;
		auto const high = size - half;

		auto xl = lhs.to_borrowed(0, half);
		auto xu = lhs.to_borrowed(half);
		auto yl = rhs.to_borrowed(0, half);
		auto yu = rhs.to_borrowed(half);

		assert(!xl.is_owned());
		assert(!xu.is_owned());
		assert(!yl.is_owned());
		assert(!yu.is_owned());

		auto x_sum = DynArray<block_t>(high * 2 + /* carry */ 1, 0);
		auto y_sum = DynArray<block_t>(high * 2 + /* carry */ 1, 0);
		auto const sz = (high << 1) + 1;
		auto z0 = DynArray<block_t>(sz, 0);
		auto z2 = DynArray<block_t>(sz, 0);
		auto z3 = DynArray<block_t>(sz, 0);

		auto l_carry = acc_t{};
		auto r_carry = acc_t{};

		{
			auto i = 0zu;
			for (; i < half; ++i) {
				auto [x, xc] = safe_add_helper(xl[i], xu[i] + l_carry);
				auto [y, yc] = safe_add_helper(yl[i], yu[i] + r_carry);
				l_carry = xc;
				r_carry = yc;

				x_sum[i] = x;
				y_sum[i] = y;
			}
		

			for (; i < high; ++i) {
				auto [x, xc] = safe_add_helper(xu[i], l_carry);
				auto [y, yc] = safe_add_helper(yu[i], r_carry);

				x_sum[i] = x;
				l_carry = xc;

				y_sum[i] = y;
				r_carry = yc;
			}
		}
		
		auto const mid_size = high + static_cast<bool>(l_carry | r_carry);
		if (mid_size > high) {
			x_sum[high] = l_carry & BlockInfo::lower_mask;
			y_sum[high] = r_carry & BlockInfo::lower_mask;
		}

		/*std::println("{}: xl: {}\nxu: {}\nyl: {}\nyu: {}\nxs: {}\nys: {}\n", depth, xl, xu, yl, yu, x_sum, y_sum);*/

		karatsuba_mul_helper<NaiveThreshold>(z0, xl, yl, low, depth + 1);
		karatsuba_mul_helper<NaiveThreshold>(z2, xu, yu, high, depth + 1);
		karatsuba_mul_helper<NaiveThreshold>(z3, x_sum, y_sum, mid_size, depth + 1);

		auto o1 = out.to_borrowed(0, low << 1);
		auto o2 = out.to_borrowed(half, mid_size << 1);
		auto o3 = out.to_borrowed(half << 1, high << 1);
		safe_add_helper(o1, z0);
		safe_add_helper(o2, z3);
		safe_add_helper(o3, z2);

		/*std::println("\n========{}==========", depth);*/
		/*std::println("z0: {}\nz2: {}\nz3: {}\n", z0, z2, z3);*/
		auto z0_t = z0.to_borrowed(0, sz);
		auto z2_t = z2.to_borrowed(0, sz);
		safe_add_helper(z0_t, z2_t);


		auto b = acc_t{};
		for (auto i = 0zu; i < sz; ++i) {
			auto [v, bb] = safe_sub_helper(out[i + half], z0[i], b);
			out[i + half] = v;
			b = bb;
		}
	}

	template <std::size_t NaiveThreshold>
	inline static constexpr auto karatsuba_mul(
		BlockInfo::blocks_t& out,
		BlockInfo::blocks_t const& lhs,
		BlockInfo::blocks_t const& rhs
	) noexcept -> void {
		auto temp_size = std::max(lhs.size(), rhs.size());
		out.resize(temp_size << 1, 0);

		utils::TempAllocatorScope scope;
		
		auto buff_a = BlockInfo::blocks_t(temp_size + 1, 0);
		auto buff_b = BlockInfo::blocks_t(temp_size + 1, 0);

		std::copy(lhs.begin(), lhs.end(), buff_a.begin());
		std::copy(rhs.begin(), rhs.end(), buff_b.begin());

		auto size =	std::max(lhs.size(), rhs.size());
		size += (size & 1);

		karatsuba_mul_helper<NaiveThreshold + (NaiveThreshold & 1)>(
			out,
			buff_a,
			buff_b,
			size
		);
	}


	template <std::size_t D>
	inline static constexpr auto toom_cook_3_sub_helper(
		BlockInfo::accumulator_t lhs,
		BlockInfo::accumulator_t rhs,
		BlockInfo::accumulator_t prev = 0
	) noexcept -> std::tuple<BlockInfo::type, BlockInfo::type, BlockInfo::type>
	{
		auto b0 = rhs < prev;
		auto t0 = lhs + b0 * BlockInfo::max_value - prev;
		
		auto b1 = lhs < t0;
		auto t1 = lhs + b1 * BlockInfo::max_value - t0;
		
		auto b = static_cast<BlockInfo::type>(b0 | b1);
		assert(t1 % D == 0);
		auto res = t1 / D;
		auto c = (res >> BlockInfo::total_bits) & BlockInfo::lower_mask;
		if (b && c) {
			c -= 1;
			b -= 1;
		}

		return std::make_tuple(res, c, b);
	}

	// lhs < rhs
	inline static constexpr auto is_less(
		BlockInfo::blocks_t const& lhs,
		BlockInfo::blocks_t const& rhs
	) noexcept -> bool {
	
		constexpr auto count_bits = [](BlockInfo::blocks_t const& arr) {
			auto bits = 0zu;
			for (auto i = 0zu; i < arr.size(); ++i) {
				auto b = arr[i];
				if (!b) continue;
				bits = static_cast<std::size_t>(std::bit_width(b));
				return bits + BlockInfo::total_bits * (arr.size() - i);
			}

			return bits;
		};

		auto lb = count_bits(lhs);
		auto rb = count_bits(rhs);

		if (lb > rb) return false;
		if (lb < rb) return true;

		auto sz = std::min(lhs.size(), rhs.size());
		for (auto i = 0zu; i < sz; ++i) {
			if (lhs[i] > rhs[i]) return false;
		}
		return true;
	}

	template <std::size_t NaiveT>
	inline static constexpr auto toom_cook_3_helper(
		is_basic_integer auto& out,	
		is_basic_integer auto const& lhs,
		is_basic_integer auto const& rhs,
		std::size_t size
	) noexcept -> void {
		using integer = std::decay_t<decltype(out)>;

		if (size <= NaiveT) {
			naive_mul(out.dyn_arr(), lhs.dyn_arr(), rhs.dyn_arr());
			out.trim_leading_zeros();
			return;
		}

		auto const left_size = size / 3;
		auto const mid_size = left_size * 2;
		
		auto ll = lhs.to_borrowed(0, left_size);
		auto lm = lhs.to_borrowed(left_size, left_size);

		auto lr = lhs.to_borrowed(mid_size);

		auto rl = rhs.to_borrowed(0, left_size);
		auto rm = rhs.to_borrowed(left_size, left_size);
		auto rr = rhs.to_borrowed(mid_size);

		auto l_n2  = integer{}; // lhs(-2)
		auto l_n1  = integer{}; // lhs(-1)
		auto l_0   = integer{}; // lhs(0)
		auto l_1   = integer{}; // lhs(1)
		auto l_inf = integer{}; // lhs(inf)
		
		auto r_n2  = integer{}; // rhs(-2)
		auto r_n1  = integer{}; // rhs(-1)
		auto r_0   = integer{}; // rhs(0)
		auto r_1   = integer{}; // rhs(1)
		auto r_inf = integer{}; // rhs(inf)

		auto o_n2  = integer{}; // out(-2)
		auto o_n1  = integer{}; // out(-1)
		auto o_0   = integer{}; // out(0)
		auto o_1   = integer{}; // out(1)
		auto o_inf = integer{}; // out(inf)

		auto eval = [](
			auto const& m0, // lower
			auto const& m1, // middle
			auto const& m2, // upper
			auto& p_n2, // p(-2)
			auto& p_n1, // p(-1)
			auto& p_0,  // p(0)
			auto& p_1,  // p(1)
			auto& p_inf// p(inf)
		) noexcept -> void {
		
			// 1. pt = m0 + m2;
			auto pt = m0 + m2;

			// 2. p(0) = m0;
			p_0 = m0;

			// 3. p(1) = pt + m1;
			p_1 = pt + m1;
	
			// 4. p(−1) = pt − m1;
			p_n1 = pt - m1;

			// 5. p(-2) = (p(−1) + m2) * 2 − m0;
			p_n2 = (p_n1 + m2) * 2 - m0;

			// 6. p(∞) = m2;
			p_inf = m2;
		};

	
		eval(ll, lm, lr, l_n2, l_n1, l_0, l_1, l_inf);
		eval(rl, rm, rr, r_n2, r_n1, r_0, r_1, r_inf);

		// ==== out(-2) = l_n2 * r_n2
		auto s1 = l_n2.is_neg();
		auto s2 = r_n2.is_neg(); 
		toom_cook_3_helper<NaiveT>(o_n2, l_n2.abs_mut(), r_n2.abs_mut(), left_size);
		o_n2.set_is_neg(s1 != s2);

        // ==== out(-1) = l_n1 * r_n1
		s1 = l_n1.is_neg();
		s2 = r_n1.is_neg(); 
		toom_cook_3_helper<NaiveT>(o_n1, l_n1.abs_mut(), r_n1.abs_mut(), left_size);
		o_n1.set_is_neg(s1 != s2);

        // ==== out(0) = l_0 * r_0
		s1 = l_0.is_neg();
		s2 = r_0.is_neg(); 
		toom_cook_3_helper<NaiveT>(o_0, l_0.abs_mut(), r_0.abs_mut(), left_size);
		o_0.set_is_neg(s1 != s2);

        // ==== out(1) = l_1 * r_1
		s1 = l_1.is_neg();
		s2 = r_1.is_neg(); 
		toom_cook_3_helper<NaiveT>(o_1, l_1.abs_mut(), r_1.abs_mut(), left_size);
		o_1.set_is_neg(s1 != s2);

        // ==== out(inf) = l_inf * r_inf
		s1 = l_inf.is_neg();
		s2 = r_inf.is_neg(); 
		toom_cook_3_helper<NaiveT>(o_inf, l_inf.abs_mut(), r_inf.abs_mut(), left_size);
		o_inf.set_is_neg(s1 != s2);
	//
		// 1. o0 = o_0
		// 2. o4 = o_inf
		auto o0 = o_0;
		auto o4 = o_inf;

		// 3. o3 = (o_n2 - o_1) / 3
		auto o3 = (o_n2 - o_1) / 3;
		
		// 4. o1 = (o_1 - o_n1) / 2
		auto o1 = (o_1 - o_n1) >> 1;
		
		// 5. o2 = o_n1 - o_0;
		auto o2 = o_n1 - o_0;
		
		// 6. o3 = (o2 - o3)/2 + 2 * o_inf;
		auto t = o3;
		o3 = ((o2 - o3) >> 1) + (o_inf * 2);

		// 7. o2 = o2 + o1 - o4
		o2 += o1 - o4;

		// 8. o1 = o1 - o3
		o1 -= o3;

		// 9. out = o4 * x^4k + o3 * x^3k + o2 * x^2k + o1 * x^k + o0
		o4.shift_by_base(4 * left_size);
		o3.shift_by_base(3 * left_size);
		o2.shift_by_base(2 * left_size);
		o1.shift_by_base(1 * left_size);

		auto temp = o4 + o3 + o2 + o1 + o0;
		out.transfer_ownership(temp);
	}

	template <std::size_t NaiveT>
	inline static constexpr auto toom_cook_3(
		is_basic_integer auto& out,	
		is_basic_integer auto const& lhs,	
		is_basic_integer auto const& rhs	
	) noexcept -> void {
		auto temp_size = std::max(lhs.size(), rhs.size());
		utils::TempAllocatorScope scope;

		temp_size += (3 - temp_size % 3);
		
		auto a = lhs;
		auto b = rhs;
		a.dyn_arr().resize(temp_size, 0);
		b.dyn_arr().resize(temp_size, 0);

		toom_cook_3_helper<NaiveT + (3 - NaiveT % 3)>(out, a, b, temp_size);
	}
	

	inline static constexpr auto mul_power_of_two(
		is_basic_integer auto& res,
		is_basic_integer auto const& a,
		is_basic_integer auto const& pow_2
	) -> void {
		res = a;
		res.shift_left_mut(pow_2.bits() - 1, true);
	}

	
	static constexpr auto mul_impl(
		is_basic_integer auto& res,
		is_basic_integer auto const& a,
		is_basic_integer auto const& b,
		MulKind kind
	) -> void {
		if (a.is_zero() || b.is_zero()) {
			res.set_zero();
			return;
		}

		if (a.is_one()) {
			res = b;
			res.reset_ou_flags();
			return;
		}

		if (b.is_one()) {
			res = a;
			res.reset_ou_flags();
			return;
		}
		
		// fast-path
		if (a.is_power_of_two()) {
			mul_power_of_two(res, b, a); 
			return;
		} else if (b.is_power_of_two()) {
			mul_power_of_two(res, a, b);
			return;
		}

		auto scope = utils::TempAllocatorScope();

		auto const a_size = a.size();
		auto const b_size = b.size();

		auto const naive_mul_impl = [&] {
			integer::naive_mul(
				res.dyn_arr(),
				a.dyn_arr(),
				b.dyn_arr()
			);
		};

		auto const karatsub_mul_impl = [&] {
			integer::karatsuba_mul<BIG_NUM_NAIVE_THRESHOLD>(
				res.dyn_arr(),
				a.dyn_arr(),
				b.dyn_arr()
			);
		};

		auto const ntt_impl = [&] {
			impl::NTT::mul(
				res.dyn_arr(),
				a.dyn_arr(),
				b.dyn_arr()
			);
		};

		auto const toom_cook_3_impl = [&] {
			toom_cook_3<BIG_NUM_NAIVE_THRESHOLD>(res, a, b);
		};

		switch (kind) {
			case MulKind::Auto: {
				if (a_size <= BIG_NUM_NAIVE_THRESHOLD && b_size <= BIG_NUM_NAIVE_THRESHOLD) naive_mul_impl();
				else if (a_size <= BIG_NUM_KARATSUBA_THRESHOLD && b_size <= BIG_NUM_KARATSUBA_THRESHOLD) karatsub_mul_impl();
				else if (a_size <= BIG_NUM_TOOM_COOK_3_THRESHOLD && b_size <= BIG_NUM_TOOM_COOK_3_THRESHOLD) toom_cook_3_impl();
				else ntt_impl();
			} break;
			case MulKind::Naive: naive_mul_impl(); break;
			case MulKind::Karatsuba: karatsub_mul_impl(); break;
			case MulKind::ToomCook3: toom_cook_3_impl(); break;
			case MulKind::NTT: ntt_impl(); break;
		}


		res.set_is_neg(a.is_neg() || b.is_neg());
		res.trim_leading_zeros();
	}


} // namespace dark::internal::integer

#endif // DARK_BIG_NUM_MUL_HPP
