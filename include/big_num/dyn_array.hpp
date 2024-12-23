#ifndef DARK_BIG_NUM_DYN_ARRAY_HPP
#define DARK_BIG_NUM_DYN_ARRAY_HPP

#include "allocator.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <optional>
#include <print>
#include <type_traits>
#include <span>

namespace dark {

	// NOTE: This class does not handle lifetimes properly.
	//	     Use this class for primitive types.
	template <typename T>
		requires std::is_arithmetic_v<T>
	struct DynArray {
		using value_type = T;
		using reference = value_type&;
		using const_reference = value_type const&;
		using pointer = value_type*;
		using const_pointer = value_type const*;
		using iterator = pointer;
		using const_iterator = const_pointer;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;

		static constexpr size_type npos = std::numeric_limits<size_type>::max();

		explicit constexpr DynArray(utils::BlockAllocator* alloc = nullptr) noexcept
			: m_alloc(alloc ? alloc : utils::current_block_allocator())
		{}

		explicit constexpr DynArray(size_type size, std::optional<value_type> def = {}, utils::BlockAllocator* alloc = nullptr)
			: m_size(size)
			, m_alloc(alloc ? alloc : utils::current_block_allocator())
		{
			reserve(size);
			if (def)
				std::fill(begin(), end(), *def);
		}

		explicit constexpr DynArray(pointer data, size_type size) noexcept
			: m_data(data)
			, m_size(size)
			, m_capacity(size)
			, m_owned(false)
			, m_alloc(utils::current_block_allocator())
		{}

		explicit constexpr DynArray(const_pointer data, size_type size) noexcept
			: DynArray(const_cast<pointer>(data), size)
		{}

		constexpr DynArray(std::initializer_list<value_type> li) noexcept
			: DynArray(li.size())
		{
			std::copy(li.begin(), li.end(), begin());
		}

		constexpr DynArray(DynArray const& other)
			: m_size(other.size())
			, m_capacity(other.size())
			, m_alloc(other.allocator())
		{
			if (other.m_data == nullptr) return;
			m_data = reallocate(other.size());
			std::copy(other.begin(), other.end(), begin());
		}

		constexpr DynArray& operator=(DynArray const& other) {
			if (this == &other) return *this;
			auto temp = DynArray(other);
			swap(temp, *this);
			return *this;
		}

		constexpr DynArray(DynArray&& other) noexcept 
			: m_data(other.m_data)
			, m_size(other.m_size)
			, m_capacity(other.m_size)
			, m_owned(other.m_owned)
			, m_alloc(other.m_alloc)
		{
			other.m_data = nullptr;
			other.m_capacity = 0;
			other.m_size = 0;
			other.m_owned = true;
		}
		constexpr DynArray& operator=(DynArray&& other) noexcept {
			if (this == &other) return *this;
			auto temp = DynArray(std::move(other));
			swap(*this, temp);
			return *this;
		}

		constexpr ~DynArray() noexcept {
			if (m_data == nullptr || m_alloc == nullptr) return;
			if (m_owned) {
				allocator()->deallocate(m_data);
			}
		}

		constexpr auto size() const noexcept -> size_type { return m_size; }
		constexpr auto capacity() const noexcept -> size_type { return m_capacity; }
		constexpr auto is_owned() const noexcept -> bool { return m_owned; }
		constexpr auto allocator() const noexcept -> utils::BlockAllocator* { 
			assert(m_alloc != nullptr);
			assert(utils::AllocatorManager::instance().is_valid_allocator(m_alloc) && "Allocator: use after free.");
			return m_alloc;
		}
		constexpr auto data() noexcept -> pointer { return m_data; }
		constexpr auto data() const noexcept -> const_pointer { return m_data; }
		constexpr auto begin() noexcept -> iterator { return data(); }
		constexpr auto end() noexcept -> iterator { return data() + size(); }
		constexpr auto begin() const noexcept -> const_iterator { return data(); }
		constexpr auto end() const noexcept -> const_iterator { return data() + size(); }
		constexpr auto rbegin() noexcept -> reverse_iterator { return std::reverse_iterator(end()); }
		constexpr auto rend() noexcept -> reverse_iterator { return std::reverse_iterator(begin()); }
		constexpr auto rbegin() const noexcept -> const_reverse_iterator { return std::reverse_iterator(end()); }
		constexpr auto rend() const noexcept -> const_reverse_iterator { return std::reverse_iterator(begin()); }

		friend void swap(DynArray& lhs, DynArray& rhs) noexcept {
			using std::swap;
			swap(lhs.m_data, rhs.m_data);
			swap(lhs.m_size, rhs.m_size);
			swap(lhs.m_capacity, rhs.m_capacity);
			swap(lhs.m_owned, rhs.m_owned);
			swap(lhs.m_alloc, rhs.m_alloc);
		}
	
		constexpr auto clear() noexcept -> void {
			to_owned();
			m_size = 0;
		}

		constexpr auto empty() const noexcept -> bool { return size() == 0; } 
		
		void push_back(T val) noexcept {
			to_owned();
			ensure_space_for(size() + 1);
			m_data[m_size++] = std::move(val);
		}

		void resize(size_type size, value_type def = {}) noexcept {
			to_owned();
			if (m_size == size) return;
			ensure_space_for(size);
			for (auto i = m_size; i < size; ++i) m_data[i] = def;
			m_size = size;
		}

		void reserve(size_type cap) noexcept {
			to_owned();
			if (m_capacity >= cap) return;
			cap = std::max(cap, 8zu);
			m_data = reallocate(cap);
			m_capacity = cap;
		}

		auto pop_back() noexcept -> value_type {
			assert(!empty());
			to_owned();
			auto val = m_data[--m_size];
			return val;
		}

		constexpr reference operator[](size_type k) noexcept {
			assert((k < size()) && "out of bound access");
			return m_data[k];
		}

		constexpr const_reference operator[](size_type k) const noexcept {
			assert((k < size()) && "out of bound access");
			return m_data[k];
		}

		constexpr reference back() noexcept {
			assert(!empty());
			return m_data[size() - 1];
		}

		constexpr reference back() const noexcept {
			assert(!empty());
			return m_data[size() - 1];
		}
		constexpr auto to_owned() noexcept -> DynArray& {
			clone_if_borrowed();	
			return *this;
		}

		constexpr auto to_borrowed(size_type start = 0, size_type size = npos) const noexcept -> DynArray {
			auto new_size = std::max(m_size, start) - start;
			auto sz = std::min(new_size, size);
			return DynArray(m_data + start,  sz);
		}

		constexpr auto clone_from(DynArray const& in) noexcept -> void {
			resize(in.size());
			std::copy(in.begin(), in.end(), begin());
		}

		constexpr operator std::span<T>() const noexcept {
			return { m_data, m_size };
		}
	private:

		auto reallocate(size_type size) noexcept -> pointer {
			if (m_data == nullptr) {
				return allocator()->template allocate<value_type>(size);
			}
			return allocator()->reallocate(m_data, size);
		}

		auto ensure_space_for(size_type size) noexcept -> void {
			if (m_capacity >= size) return;
			auto cap = std::max(m_capacity, 8zu);
			while (cap <= size) {
				cap = static_cast<size_type>(static_cast<float>(cap) * 1.5f); 
			}
			
			m_data = reallocate(cap);
			m_capacity = cap;
		}

		constexpr auto clone_if_borrowed() noexcept -> void {
			if (m_owned) return;
			auto ptr = reallocate(m_capacity);
			std::copy_n(m_data, m_size, ptr);
			m_data = ptr;
			m_owned = true;
		}

	private:
		pointer		m_data{nullptr};
		size_type	m_size{};
		size_type	m_capacity{};
		bool		m_owned{true};
		mutable utils::BlockAllocator* m_alloc{};
	};

} // namespace dark

#endif // DARK_BIG_NUM_DYN_ARRAY_HPP
