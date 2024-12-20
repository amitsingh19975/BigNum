#include "big_num/allocator.hpp"
#include <algorithm>
#include <memory>
#include <print>
#include <catch2/catch_test_macros.hpp>
#include <type_traits>

using namespace dark;
using namespace dark::utils;

template <typename... Args>
	requires ((std::is_integral_v<Args> && ...) && sizeof...(Args) > 0)
constexpr auto get_actual_size(Args... args) noexcept -> std::size_t {
	return ((args + BumpAllocator::meta_info_size) + ...);
}

TEST_CASE("Bump Allocator", "[bump_allocator]") {
	SECTION("Construction") {
		{
			auto a = BumpAllocator(20);
			REQUIRE(a.free_space() == 20);
			REQUIRE(a.size() == 20);
			REQUIRE(a.is_owned() == true);
			REQUIRE(a.get_ref().ref_count == 0);
			REQUIRE(a.get_ref().cursor == 0);
		}
		{
			std::unique_ptr<char[]> buff = std::make_unique<char[]>(20);
			auto a = BumpAllocator(buff.get(), 20);
			REQUIRE(a.free_space() == 20);
			REQUIRE(a.size() == 20);
			REQUIRE(a.is_owned() == false);
			REQUIRE(a.get_ref().ref_count == 0);
			REQUIRE(a.get_ref().cursor == 0);
			REQUIRE(a.buff_start_ptr() == buff.get());
		}
		{
			char buff[20];
			auto a = BumpAllocator(buff);
			REQUIRE(a.free_space() == 20);
			REQUIRE(a.size() == 20);
			REQUIRE(a.is_owned() == false);
			REQUIRE(a.get_ref().ref_count == 0);
			REQUIRE(a.get_ref().cursor == 0);
			REQUIRE(a.buff_start_ptr() == buff);
		}
	}

	SECTION("Allocation") {
		auto const size = 20u;
		auto a = BumpAllocator(size);
		auto p1 = a.allocate(10 * sizeof(char));
		REQUIRE(p1.has_value());
		REQUIRE(a.free_space() == size - get_actual_size(10));
		REQUIRE(a.get_ref().ref_count == 1);
		REQUIRE(a.get_ptr_size(*p1) == 10);

		auto p2 = a.allocate(10);
		REQUIRE(!p2.has_value());
		REQUIRE(p2.error() == AllocatorError::no_space);
		REQUIRE(a.free_space() == size - get_actual_size(10));
		REQUIRE(a.get_ref().ref_count == 1);
	}

	SECTION("Reallocation") {
		auto const size = 100u;
		auto a = BumpAllocator(size);

		auto p1 = a.allocate(5);
		REQUIRE(p1.has_value());
		REQUIRE(a.free_space() == size - get_actual_size(5));
		REQUIRE(a.get_ref().ref_count == 1);
		REQUIRE(a.get_ptr_size(*p1) == 5);
		std::memset(*p1, 1, 5);

		auto p2 = a.reallocate(*p1, 10);
		REQUIRE(p1.has_value());
		REQUIRE(p1 == p2);
		REQUIRE(a.free_space() == size - get_actual_size(10));
		REQUIRE(a.get_ref().ref_count == 1);
		REQUIRE(a.get_ptr_size(*p2) == 10);
		REQUIRE(std::memcmp(*p1, *p2, 5) == 0);
		std::memset(*p2, 2, 10);

		auto p3 = a.allocate(5);
		REQUIRE(p3.has_value());
		REQUIRE(a.free_space() == size - get_actual_size(10, 5));
		REQUIRE(a.get_ref().ref_count == 2);
		REQUIRE(a.get_ptr_size(*p3) == 5);

		auto p4 = a.reallocate(*p2, 20);
		REQUIRE(p4.has_value());
		REQUIRE(p2 != p4);
		REQUIRE(a.free_space() == size - get_actual_size(10, 5, 20));
		REQUIRE(a.get_ref().ref_count == 2);
		REQUIRE(a.get_ptr_size(*p4) == 20);
		REQUIRE(std::memcmp(*p2, *p4, 10) == 0);
	}

	SECTION("Deallocation") {
		auto const size = 100u;
		auto a = BumpAllocator(size);

		auto p1 = a.allocate(5);
		REQUIRE(p1.has_value());
		REQUIRE(a.free_space() == size - get_actual_size(5));
		REQUIRE(a.get_ref().ref_count == 1);
		REQUIRE(a.get_ptr_size(*p1) == 5);
		REQUIRE(a.is_valid_ptr(*p1) == true);

		auto p2 = a.allocate(5);
		REQUIRE(p2.has_value());
		REQUIRE(a.free_space() == size - get_actual_size(5, 5));
		REQUIRE(a.get_ref().ref_count == 2);
		REQUIRE(a.get_ptr_size(*p2) == 5);
		REQUIRE(a.is_valid_ptr(*p2) == true);

		a.deallocate(*p2);
		REQUIRE(a.free_space() == size - get_actual_size(5));
		REQUIRE(a.get_ref().ref_count == 1);

		auto p3 = a.allocate(5);
		REQUIRE(p3.has_value());
		REQUIRE(a.free_space() == size - get_actual_size(5, 5));
		REQUIRE(a.get_ref().ref_count == 2);
		REQUIRE(a.get_ptr_size(*p3) == 5);
		REQUIRE(a.is_valid_ptr(*p3) == true);

		a.deallocate(*p1);
		REQUIRE(a.free_space() == size - get_actual_size(5, 5));
		REQUIRE(a.get_ref().ref_count == 1);
		REQUIRE(a.get_ref().cursor != 0);

		a.deallocate(*p3);
		REQUIRE(a.free_space() == size);
		REQUIRE(a.get_ref().ref_count == 0);
		REQUIRE(a.get_ref().cursor == 0);

	}

}

TEST_CASE("Block Allocator", "[block_allocator]") {
	SECTION("Construction") {
		{
			auto b = BlockAllocator(10, "block");
			REQUIRE(b.blocks() == 0);
			REQUIRE(b.name() == "block");
			REQUIRE(b.empty());
			REQUIRE(b.free_blocks() == 0);
			REQUIRE(b.current_block_size() == 10 + BlockAllocator::extra_space);
		}	
		{
			auto buff = std::make_unique<char[]>(10);
			auto b = BlockAllocator(buff.get(), 10, "block");
			REQUIRE(b.blocks() == 1);
			REQUIRE(b.front().buff_start_ptr() == buff.get());
			REQUIRE(b.name() == "block");
			REQUIRE(b.empty() == false);
			REQUIRE(b.free_blocks() == 0);
			REQUIRE(b.current_block_size() == 10);
		}
		{
			char buff[10];
			auto b = BlockAllocator(buff, "block");
			REQUIRE(b.blocks() == 1);
			REQUIRE(b.front().buff_start_ptr() == buff);
			REQUIRE(b.name() == "block");
			REQUIRE(b.empty() == false);
			REQUIRE(b.free_blocks() == 0);
			REQUIRE(b.current_block_size() == 10);
		}
	}

	SECTION("Allocation") {
		{
			auto b = BlockAllocator(10, "block"); // 10 + extra
			REQUIRE(b.blocks() == 0);
			REQUIRE(b.name() == "block");
			REQUIRE(b.empty());
			REQUIRE(b.free_blocks() == 0);
			REQUIRE(b.current_block_size() == 10 + BlockAllocator::extra_space);

			auto p1 = b.allocate<char>(b.current_block_size() - 5);
			REQUIRE(p1 != nullptr);
			REQUIRE(b.blocks() == 1);
			REQUIRE(b.front().free_space() <= 5);

			auto p2 = b.allocate<char>(5);
			REQUIRE(p2 != nullptr);
			REQUIRE(b.blocks() == 2);
			REQUIRE(b.front().free_space() < b.current_block_size() - 5);
		}
		{
			auto b = BlockAllocator(10, "block"); // 10 + extra
			REQUIRE(b.blocks() == 0);
			REQUIRE(b.name() == "block");
			REQUIRE(b.empty());
			REQUIRE(b.free_blocks() == 0);
			REQUIRE(b.current_block_size() == 10 + BlockAllocator::extra_space);

			auto p1 = b.allocate<char>(b.current_block_size() - 5);
			REQUIRE(p1 != nullptr);
			REQUIRE(b.blocks() == 1);
			REQUIRE(b.front().free_space() <= 5);

			auto p2 = b.allocate<char>(b.current_block_size() - 5);
			REQUIRE(p2 != nullptr);
			REQUIRE(b.blocks() == 2);
			REQUIRE(b.front().free_space() < b.current_block_size() - 5);

			// reuse block 1 to allocate p3
			b.deallocate(p1);
			auto p3 = b.allocate<char>(10);
			REQUIRE(p1 != nullptr);
			REQUIRE(b.blocks() == 2);

		}
	}

	SECTION("Reallocation") {
		auto b = BlockAllocator(10);
		REQUIRE(b.blocks() == 0);
		REQUIRE(b.name().empty());
		REQUIRE(b.empty());
		REQUIRE(b.free_blocks() == 0);
		REQUIRE(b.current_block_size() == 10 + BlockAllocator::extra_space);

		auto p1 = b.allocate<char>(5);
		REQUIRE(p1 != nullptr);
		REQUIRE(b.front().get_ptr_size(p1) == 5);
		REQUIRE(b.blocks() == 1);

		auto p2 = b.reallocate(p1, 10);
		REQUIRE(p1 == p2);
		REQUIRE(b.front().get_ptr_size(p2) == 10);
		REQUIRE(b.blocks() == 1);

		auto p3 = b.allocate<char>(2);
		REQUIRE(p3 != nullptr);
		REQUIRE(b.front().get_ptr_size(p3) == 2);
		REQUIRE(b.blocks() == 1);


		auto p4 = b.reallocate(p2, 11);
		REQUIRE(p2 != p4);
		REQUIRE(b.front().get_ptr_size(p4) == 11);
		REQUIRE(b.blocks() == 1);

		// If the size is greater than the block size, it would allocate a new block of that size.
		auto p5 = b.reallocate(p4, 50);
		REQUIRE(p4 != p5);
		REQUIRE(b.front().get_ptr_size(p5) == 50);
		REQUIRE(b.blocks() == 2);
		REQUIRE(b.front().free_space() == 0);
	}
}

TEST_CASE("Temp Allocator Scope", "[temp_allocator_scope]") {
	AllocatorManager m(10);
	REQUIRE(m.current_alloc()->name() == AllocatorManager::global_allocator_name);
	REQUIRE(m.current_alloc()->blocks() == 0);

	{
		auto scope = TempAllocatorScope(m, 100);
		REQUIRE(m.current_alloc()->name() == AllocatorManager::temp_allocator_name);
		REQUIRE(m.current_alloc()->blocks() == 0);
	}
	
	REQUIRE(m.current_alloc()->name() == AllocatorManager::global_allocator_name);
	REQUIRE(m.current_alloc()->blocks() == 0);
}
