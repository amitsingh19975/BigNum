#include "big_num/allocator.hpp"
#include <cstdint>
#include <print>
#include <catch2/catch_test_macros.hpp>
#include <big_num.hpp>

using namespace dark;
using namespace dark::utils;

using alloc_t = BasicBumpAllocator;

TEST_CASE("Bump Allocator", "[bump_allocator]") {
	alloc_t alloc(10);

	SECTION("Allocation") {
		auto* ptr = alloc.allocate(5, alignof(char));
		REQUIRE(ptr != nullptr);
		REQUIRE(!alloc.is_heap_ptr(ptr));
		REQUIRE(alloc.free_space() == 5);

		alloc.deallocate(ptr);
		REQUIRE(alloc.free_space() == 10); // Should rewind a block back if the last allocated block.

		auto* p1 = alloc.allocate(1, 0);
		auto* p2 = alloc.allocate(1, 0);
		REQUIRE(p1 != nullptr);
		REQUIRE(p2 != nullptr);

		alloc.deallocate(p1);
		REQUIRE(alloc.free_space() == 8); // Should remain the same size since it's not the last block.

		alloc.reset();

		REQUIRE(alloc.free_space() == 10);
	}

	SECTION("Heap allocation") {
		alloc.reset();
		auto p1 = alloc.allocate(5, 0);
		auto p2 = alloc.allocate(5, 0);
		REQUIRE(!alloc.is_heap_ptr(p1));
		REQUIRE(!alloc.is_heap_ptr(p2));
		REQUIRE(alloc.free_space() == 0);

		auto p3 = alloc.allocate(5, 0);
		REQUIRE(alloc.is_heap_ptr(p3));

		alloc.reset();

		auto p4 = alloc.allocate(20, 0);
		REQUIRE(alloc.is_heap_ptr(p4));
	}

	SECTION("Pointer alignment") {
		alloc.reset();
		auto const alignment = alignof(std::size_t);
		auto p1 = alloc.allocate(5, alignment);
		REQUIRE(!alloc.is_heap_ptr(p1));

		auto add = reinterpret_cast<std::uintptr_t>(p1);
		REQUIRE(add % alignment == 0);

		auto p2 = alloc.allocate(20, alignof(std::size_t));
		REQUIRE(alloc.is_heap_ptr(p2));

		add = reinterpret_cast<std::uintptr_t>(p2);
		REQUIRE(add % alignment == 0);
	}

	SECTION("Realloc") {
		{
			alloc.reset();
			auto p1 = alloc.allocate(5, 0);
			REQUIRE(!alloc.is_heap_ptr(p1));

			auto p2 = alloc.realloc(p1, 5, 0);
			REQUIRE(!alloc.is_heap_ptr(p2));
			REQUIRE(p1 == p2);

			auto p3 = alloc.realloc(p1, 9, 0);
			REQUIRE(!alloc.is_heap_ptr(p3));
			REQUIRE(p1 == p3);
		}
		{
			alloc.reset();
			auto p1 = alloc.allocate(5, 0);
			REQUIRE(!alloc.is_heap_ptr(p1));

			auto p2 = alloc.allocate(5, 0);
			REQUIRE(!alloc.is_heap_ptr(p2));

			auto p3 = alloc.realloc(p1, 9, 0);
			REQUIRE(alloc.is_heap_ptr(p3));
			REQUIRE(p1 != p3);
		}
	}
}

TEST_CASE("Allocator Scope", "[allocator_scope]") {
	alloc_t temp(10, "temp");
	{
		REQUIRE(get_current_alloc()->name() == "global");
		AllocatorScope scop(temp);
		REQUIRE(get_current_alloc()->name() == "temp");
	}
		
	REQUIRE(get_current_alloc()->name() == "global");
}
