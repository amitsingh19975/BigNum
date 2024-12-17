#ifndef DARK_UTILS_ALLOCATOR_HPP
#define DARK_UTILS_ALLOCATOR_HPP

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <list>
#include <map>
#include <unordered_map>
#include <cstdlib>
#include <string_view>
#include <stack>
#include <vector>

#ifdef DEBUG_ALLOCATOR
	#include <print>
	#include <cstdio>
	
	static inline void debug_alloc(std::string_view prefix, auto* mem, std::size_t size, bool is_freed) {
		auto address = reinterpret_cast<std::uintptr_t>(mem);
		std::println(stderr, "{}: 0x{:0x}, size: {}, is_freed: {}", prefix, address, size, is_freed);
	}
#else
	#define debug_alloc(...)
#endif

namespace dark::utils {

	struct Allocator {
		using mem_t = void*;
		using size_type = std::size_t;
		using index_t = std::uint32_t;

		struct Info {
			mem_t ptr;
			size_type size;
			bool is_freed;
		};

		virtual ~Allocator() = default;
		virtual auto allocate(size_type bytes, size_type align) noexcept -> mem_t = 0; 
		virtual auto deallocate(mem_t block) noexcept -> void = 0; 
		virtual auto reset() noexcept -> void = 0; 
		virtual auto realloc(mem_t ptr, size_type new_size, size_type align) noexcept -> mem_t = 0;
		virtual constexpr auto name() const noexcept -> std::string_view { return {}; }
		virtual auto get_mem_info(mem_t) -> std::optional<Info> { return {}; }

		virtual auto push_state() -> void {}
		virtual auto pop_state() -> void {}

		inline static Allocator* s_current = nullptr;
	};

	// NOTE: Assumption: malloc/new cannot fail.
	struct BasicBumpAllocator: Allocator {
	public:
		using parent_t = Allocator;
		using mem_t = parent_t::mem_t;
		using size_type = parent_t::size_type;
		using index_t = parent_t::index_t;

		BasicBumpAllocator(std::size_t n, std::string_view name = {}, bool reuse_heap = false) noexcept
			: m_size(n)
			, m_reuse_heap(reuse_heap)
			, m_name(name)
			, m_owned(true)
		{
			constexpr auto align = alignof(std::max_align_t);
			m_buff = static_cast<char*>(malloc(n * sizeof(char) + align));
			
			auto const ptr = reinterpret_cast<std::uintptr_t>(m_buff);
			m_cursor += (ptr % align);
		}

		BasicBumpAllocator(char* buff, std::size_t n, std::string_view name = {}, bool reuse_heap = false) noexcept
			: m_buff(buff)
			, m_size(n)
			, m_reuse_heap(reuse_heap)
			, m_name(name)
		{
			constexpr auto align = alignof(std::max_align_t);
			auto const ptr = reinterpret_cast<std::uintptr_t>(m_buff);
			m_cursor += (ptr % align);
		}
		BasicBumpAllocator(BasicBumpAllocator const&) = delete;
		BasicBumpAllocator& operator=(BasicBumpAllocator const&) = delete;
		BasicBumpAllocator(BasicBumpAllocator&&) noexcept = default;
		BasicBumpAllocator& operator=(BasicBumpAllocator&&) noexcept = default;

		virtual ~BasicBumpAllocator() override {
			reset();
			if (m_owned) free(m_buff);
		}

		virtual auto allocate(size_type bytes, size_type align) noexcept -> mem_t override {
			if (bytes == 0) return nullptr;
			auto new_cursor = find_aligned_space(align);
			auto const padding = new_cursor - m_cursor;
			
			auto const size = bytes;
			auto const total_space_required = size + padding;

			if (total_space_required > free_space()) {
				auto n_size = size + align;
				if (auto [ptr, sz] = get_memory(n_size); ptr) {
					auto aligned_mem = align_ptr(ptr, align);
					m_in_use_blocks[aligned_mem] = {sz, ptr};
					debug_alloc("Reuse: Allocating ", aligned_mem, sz, false);
					return aligned_mem;
				}
				auto* ptr = static_cast<char*>(malloc(n_size));
				auto aligned_mem = align_ptr(ptr, align);
				m_in_use_blocks[aligned_mem] = {n_size, aligned_mem};
				debug_alloc("Heap: Allocating ", ptr, n_size, false);
				return aligned_mem;
			}
			auto* ptr = m_buff + m_cursor;
			auto* aligned_mem = ptr + padding;
			m_cursor += total_space_required;	
			m_in_use_blocks[ptr] = { total_space_required, ptr };
			debug_alloc("Pool: Allocating ", aligned_mem, total_space_required, false);
			return aligned_mem;
		}

		virtual auto deallocate(mem_t aligned_mem) noexcept -> void override {
			if (aligned_mem == nullptr) return;
			auto it = m_in_use_blocks.find(aligned_mem);
			if (it == m_in_use_blocks.end()) return;

			auto [sz, r_ptr] = it->second;
			m_in_use_blocks.erase(aligned_mem);
			debug_alloc("Deallocating ", aligned_mem, sz, true);

			if (is_heap_ptr(r_ptr)) {
				if (m_reuse_heap) {
					auto& ls = m_free_blocks[sz];
					ls.push_front(r_ptr);
				} else {
					free(r_ptr);
				}
				return;
			}
			if (m_cursor == 0) return;

			if (static_cast<char*>(r_ptr) + sz == m_buff + m_cursor) m_cursor -= sz;
			else {
				auto& ls = m_free_blocks[sz];
				ls.push_front(r_ptr);
			}
		}

		virtual auto reset() noexcept -> void override {
			m_cursor = 0;
			for (auto& [_, val]: m_in_use_blocks) {
				auto [__, ptr] = val;
				if (is_heap_ptr(ptr)) free(ptr);
			}
			m_in_use_blocks.clear();

			for (auto& [_, ls]: m_free_blocks) {
				for (auto ptr: ls) {
					if (is_heap_ptr(ptr)) free(ptr);
				}
				ls.clear();
			}
			m_free_blocks.clear();
		}

		constexpr auto is_heap_ptr(mem_t ptr) const noexcept -> bool {
			return ptr < m_buff || ptr >= (m_buff + size());
		}

		virtual auto realloc(mem_t ptr, size_type new_size, size_type align) noexcept -> mem_t override {
			auto [old_sz, real_ptr] = m_in_use_blocks[ptr];
			if (old_sz >= new_size) return ptr;

			if (!is_heap_ptr(real_ptr) && is_last_block(real_ptr, old_sz)) {
				auto diff = new_size - old_sz;
				if (m_cursor + diff <= size()) {
					m_cursor += diff;
					m_in_use_blocks[ptr] = { new_size, real_ptr };
					return ptr;
				}
			}

			auto mem = allocate(new_size, align);
			std::memcpy(mem, ptr, old_sz);
			deallocate(ptr);
			return mem;
		}

		constexpr auto free_space() const noexcept -> size_type {
			return std::max(m_cursor, size()) - m_cursor;
		}

		virtual constexpr auto name() const noexcept -> std::string_view override { return m_name; }

		constexpr auto size() const noexcept -> size_type { return m_size; }

		auto start() const noexcept -> std::uintptr_t { return reinterpret_cast<uintptr_t>(m_buff); }
		auto end() const noexcept -> std::uintptr_t { return reinterpret_cast<uintptr_t>(m_buff + size()); }
		virtual auto get_mem_info(mem_t ptr) -> std::optional<Info> override { 
			if (auto it = m_in_use_blocks.find(ptr); it != m_in_use_blocks.end()) {
				return Info {
					.ptr = it->second.second,
					.size = it->second.first,
					.is_freed = false
				};
			}
			for (auto const& [sz, ls]: m_free_blocks) {
				for (auto* m: ls) {
					if (m == ptr) {
						return Info {
							.ptr = m,
							.size = sz,
							.is_freed = true
						};
					}
				}
			}

			if (!is_heap_ptr(ptr)) {
				return Info {
					.ptr = ptr,
					.size = 0,
					.is_freed = true
				};
			}

			return {};
		}

		virtual auto push_state() -> void override {
			m_cursor_stack.push(m_cursor);
		}
		virtual auto pop_state() -> void override {
			m_cursor = m_cursor_stack.top();
			m_cursor_stack.pop();

			for (auto& [sz, ls]: m_free_blocks) {
				ls.remove_if([this](auto* ptr) {
					if (is_heap_ptr(ptr)) return false;
					auto diff = static_cast<std::size_t>(static_cast<char*>(ptr) - m_buff);
					return (diff >= m_cursor);
				});
			}
			std::erase_if(m_free_blocks, [](auto const& val) {
				return val.second.empty();
			});
			std::erase_if(m_in_use_blocks, [this](auto const& val) {
				auto ptr = val.first;
				if (is_heap_ptr(ptr)) return false;
				auto diff = static_cast<std::size_t>(static_cast<char*>(ptr) - m_buff);
				return (diff >= m_cursor);
			});
		}

	private:
		// TODO: Could be improved by using binary_search, but need to test if it is really required
		//		 or linear search is better.
		auto get_memory(size_type size) noexcept -> std::pair<mem_t, size_type> {
			for (auto& [sz, ls]: m_free_blocks) {
				if (sz >= size && ls.size() > 0) {
					auto ptr = ls.front();
					ls.pop_front();
					return std::make_pair(ptr, sz);
				}
			}
			return {};
		}

		constexpr auto find_aligned_space(size_type alignment) const noexcept -> size_type {
			if (alignment < 2 || free_space() == 0) return m_cursor;
			assert((alignment & 1) == 0 && "alignment must be a power of 2");
			if (alignment & 1) std::unreachable(); // hint the compiler that alignment is a power of two. 
			auto ptr = reinterpret_cast<std::uintptr_t>(m_buff + m_cursor);
			return m_cursor + (ptr % alignment);
		}

		constexpr auto is_last_block(mem_t ptr, size_type sz) const noexcept -> bool {
			return static_cast<char*>(ptr) + sz == m_buff + m_cursor;
		}

		constexpr auto align_ptr(mem_t ptr, size_type alignment) const noexcept -> mem_t {
			if (alignment < 2) return ptr;
			assert((alignment & 1) == 0 && "alignment must be a power of 2");
			if (alignment & 1) std::unreachable(); // hint the compiler that alignment is a power of two. 
			auto address = reinterpret_cast<std::uintptr_t>(ptr);
			return static_cast<char*>(ptr) + (address % alignment);
		}

	private:
		char *m_buff;
		size_type m_size;
		size_type m_cursor{};
		std::unordered_map<mem_t, std::pair<size_type, mem_t>> m_in_use_blocks{};
		std::map<size_type, std::list<mem_t>> m_free_blocks{};
		std::stack<size_type> m_cursor_stack;
		bool m_reuse_heap{false};
		std::string_view m_name{};
		bool m_owned{false};
	};

	static constexpr std::size_t global_bump_allocator_size = /*32Mib*/ 1024 * 1024 * 16;
	static inline auto get_global_bump_allocator() noexcept -> BasicBumpAllocator* {
		static auto alloc = BasicBumpAllocator(global_bump_allocator_size, "global", false);
		return &alloc;
	}

	static inline auto temp_buffer(std::size_t n = 1024 * 1024 * 2) noexcept {
		return BasicBumpAllocator(n, "temp_buffer", true);
	}

	inline static auto set_current_alloc(Allocator* alloc) noexcept -> void {
		// TODO: add mutex to protect from data-race
		Allocator::s_current = alloc;
	}

	inline static auto get_current_alloc() noexcept -> Allocator* {
		if (Allocator::s_current != nullptr) return Allocator::s_current;
		set_current_alloc(get_global_bump_allocator());
		return Allocator::s_current;
	}

	struct AllocatorScope {
		template <typename T>
			requires std::is_base_of_v<Allocator, std::decay_t<T>>
		constexpr AllocatorScope(T& alloc) noexcept {
			if (get_current_alloc() == &alloc) {
				return;
			}
			m_alloc = get_current_alloc();
			set_current_alloc(&alloc);
		}
		constexpr AllocatorScope(AllocatorScope const&) noexcept = delete;
		constexpr AllocatorScope& operator=(AllocatorScope const&) noexcept = delete;
		constexpr AllocatorScope(AllocatorScope &&) noexcept = delete;
		constexpr AllocatorScope& operator=(AllocatorScope &&) noexcept = delete;
		constexpr ~AllocatorScope() noexcept {
			if (m_alloc == nullptr) return;
			auto* alloc = get_current_alloc();
			alloc->reset();
			set_current_alloc(m_alloc);
		}
	private:
		Allocator* m_alloc{nullptr};
	};
} // namespace dark::uitls

#ifndef DEBUG_ALLOCATOR
	#undef debug_alloc
#endif

#endif // DARK_UTILS_ALLOCATOR_HPP
