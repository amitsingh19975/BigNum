#include "big_num/allocator.hpp"
#include <algorithm>
#include <print>
#include <catch2/catch_test_macros.hpp>
#include <big_num.hpp>

using namespace dark;

TEST_CASE("Dynamic Array", "[dyn_array]") {

	SECTION("Construction") {
		{
			auto a = DynArray<int>();
			REQUIRE(a.size() == 0);
			REQUIRE(a.empty());
			REQUIRE(a.capacity() == 0);
			REQUIRE(a.data() == nullptr);
		}

		{
			auto a = DynArray<int>(5);
			REQUIRE(a.size() == 5);
			REQUIRE(!a.empty());
			REQUIRE(a.capacity() == 8); // capacity cannot be drop by 8.
			REQUIRE(a.data() != nullptr);
			for (auto el : a) {
				REQUIRE(el == 0);
			}
		}
		{
			int buff[] = {1, 2, 3, 4, 5};
			constexpr auto size = sizeof(buff) / sizeof(buff[0]);
			auto a = DynArray(buff, size);
			REQUIRE(a.size() == 5);
			REQUIRE(a.is_owned() == false);
			REQUIRE(a.capacity() == 5);

			a.push_back(6);
			REQUIRE(a.size() == 6);
			REQUIRE(a.is_owned() == true);
			REQUIRE(a.capacity() > 5);
		}
		{
			int buff[] = {1, 2, 3, 4, 5};
			constexpr auto size = sizeof(buff) / sizeof(buff[0]);
			auto a = DynArray(buff, size);
			REQUIRE(a.size() == 5);
			REQUIRE(a.is_owned() == false);
			REQUIRE(a.capacity() == 5);

			REQUIRE(a.pop_back() == 5);
			REQUIRE(a.size() == 4);
			REQUIRE(a.is_owned() == true);
			REQUIRE(a.capacity() == 5);
		}
		{
			auto a = DynArray {1, 2, 3, 4, 5};
			REQUIRE(a.size() == 5);
			REQUIRE(a.is_owned() == true);
			REQUIRE(a.capacity() >= 5);

			auto v1 = a.to_borrowed(2);
			REQUIRE(v1.size() == 3);
			REQUIRE(v1.is_owned() == false);
			REQUIRE(std::equal(v1.begin(), v1.end(), a.begin() + 2));

			auto v2 = a.to_borrowed();
			REQUIRE(v2.size() == 5);
			REQUIRE(v2.is_owned() == false);

			auto v3 = a.to_borrowed(1, 2);
			REQUIRE(v3.size() == 2);
			REQUIRE(v3.is_owned() == false);
			REQUIRE(v3[0] == 2);
			REQUIRE(v3[1] == 3);
		}
	}

	SECTION("Size Modification") {
		{
			auto a = DynArray<int>();
			REQUIRE(a.size() == 0);
			REQUIRE(a.empty());
			REQUIRE(a.capacity() == 0);
			REQUIRE(a.data() == nullptr);

			a.resize(10, 5);
			REQUIRE(a.size() == 10);
			REQUIRE(!a.empty());
			REQUIRE(a.capacity() >= 10);
			REQUIRE(a.data() != nullptr);
			REQUIRE(std::all_of(a.begin(), a.end(), [](auto v) { return v == 5; }));

			a.resize(20, 5);
			REQUIRE(a.size() == 20);
			REQUIRE(!a.empty());
			REQUIRE(a.capacity() >= 20);
			REQUIRE(a.data() != nullptr);
			REQUIRE(std::all_of(a.begin(), a.end(), [](auto v) { return v == 5; }));
		}
		{
			auto a = DynArray<int>();
			REQUIRE(a.size() == 0);
			REQUIRE(a.empty());
			REQUIRE(a.capacity() == 0);
			REQUIRE(a.data() == nullptr);

			a.reserve(10);
			REQUIRE(a.size() == 0);
			REQUIRE(a.empty());
			REQUIRE(a.capacity() == 10);
			REQUIRE(a.data() != nullptr);

			a.reserve(20);
			REQUIRE(a.size() == 0);
			REQUIRE(a.empty());
			REQUIRE(a.capacity() == 20);
			REQUIRE(a.data() != nullptr);
		}
		{
			auto a = DynArray<int>();
			REQUIRE(a.size() == 0);
			REQUIRE(a.empty());
			REQUIRE(a.capacity() == 0);
			REQUIRE(a.data() == nullptr);

			a.push_back(1);
			REQUIRE(a.size() == 1);
			REQUIRE(!a.empty());
			REQUIRE(a.capacity() >= 8);
			REQUIRE(a.data() != nullptr);
			REQUIRE(a.back() == 1);

			REQUIRE(a.pop_back() == 1);
			REQUIRE(a.size() == 0);
			REQUIRE(a.empty());
			REQUIRE(a.capacity() >= 8);
			REQUIRE(a.data() != nullptr);
		}
		{
			auto a = DynArray<int>(1);
			REQUIRE(a.size() == 1);
			REQUIRE(!a.empty());
			REQUIRE(a.capacity() >= 0);
			REQUIRE(a.data() != nullptr);

			auto old_ptr = a.data();

			for (auto i = 0zu; i < 10000; ++i) {
				a.push_back(i);
				REQUIRE(a.data() == old_ptr);
				REQUIRE(a.back() == i);
			}
			REQUIRE(a.size() == 10001);

			for (auto i = 0zu; i < 500; ++i) {
				REQUIRE(a.pop_back() == 10000 - i - 1);
				REQUIRE(a.data() == old_ptr);
			}
			REQUIRE(a.size() == 10001 - 500);
		}
	}


	SECTION("Allocator Scope") {
		auto a = DynArray<int>();
		REQUIRE(a.allocator()->current_block_size() == utils::AllocatorManager::global_bump_allocator_size);
		REQUIRE(a.allocator()->name() == utils::AllocatorManager::global_allocator_name);
		{
			utils::TempAllocatorScope scope(10);
			auto b = DynArray<int>();
			REQUIRE(a.allocator()->current_block_size() == 10);
			REQUIRE(a.allocator()->name() == utils::AllocatorManager::temp_allocator_name);
		}

		auto c = DynArray<int>();
		REQUIRE(a.allocator()->current_block_size() == utils::AllocatorManager::global_bump_allocator_size);
		REQUIRE(a.allocator()->name() == utils::AllocatorManager::global_allocator_name);
	}

}
