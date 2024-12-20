#ifndef DARK_UTILS_ALLOCATOR_HPP
#define DARK_UTILS_ALLOCATOR_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <cstdlib>
#include <string_view>
#include <stack>
#include <list>
#include <atomic>
#include <expected>

#define DEBUG_ALLOCATOR

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
	enum class AllocatorError {
		no_space // When there is not space to allocate
	};

	constexpr std::string_view to_string(AllocatorError e) noexcept {
		switch (e) {
			case AllocatorError::no_space: return "not enough space";
		}
		std::unreachable();
	}

	// NOTE: Assumption: allocation can never fail
	struct BumpAllocator {
		using mem_t = void*;
		using size_type = std::size_t;
		using index_t = std::uint32_t;

		static_assert(sizeof(size_type) == 8, "Right now, this allocator supports only 64-bit machine.");
	private:
		struct MemInfo {
			index_t size;
		};
	public:
		struct Ref {
			index_t ref_count{};
			index_t cursor{};
		};

		static constexpr auto meta_info_size = sizeof(MemInfo);


		constexpr BumpAllocator(size_type bytes) noexcept
			: m_buff(new char[bytes])
			, m_size(bytes)
		{
		}

		constexpr BumpAllocator(char* buff, size_type size) noexcept
			: m_buff(buff)
			, m_size(size)
			, m_owned(false)
		{}

		template <size_type N>
		constexpr BumpAllocator(char (&buff)[N]) noexcept
			: BumpAllocator(buff, N)
		{}

		BumpAllocator(BumpAllocator const&) = delete;
		BumpAllocator& operator=(BumpAllocator const&) = delete;

		constexpr BumpAllocator(BumpAllocator&& other) noexcept
			: m_ref_cursor(other.m_ref_cursor.load(std::memory_order::relaxed))
			, m_buff(other.m_buff)
			, m_size(other.m_size)
			, m_owned(other.m_owned)
		{
			other.m_buff = nullptr;
			other.m_owned = false;
			other.m_size = 0;
			other.m_ref_cursor.store(0);
		}

		constexpr BumpAllocator& operator=(BumpAllocator&& other) noexcept {
			if (this == &other) return *this;
			swap(other, *this);
			return *this;
		}

		constexpr ~BumpAllocator() noexcept {
			if (m_owned && m_buff) delete[] m_buff;
		}


		[[nodiscard]] auto allocate(size_type bytes) noexcept -> std::expected<mem_t, AllocatorError> {
			auto const size = bytes + sizeof(MemInfo);
			auto maybe_refs = try_alloc(size);	
			if (!maybe_refs.has_value()) return std::unexpected(maybe_refs.error());

			auto temp = *maybe_refs;

			while (!m_ref_cursor.compare_exchange_weak(temp.first, temp.second, std::memory_order::acq_rel, std::memory_order::acquire)) {
				maybe_refs = try_alloc(size);
				if (!maybe_refs.has_value()) return std::unexpected(maybe_refs.error());

				temp = *maybe_refs;
			}
			
			auto old_ref = from_ref_cursor(temp.first);
			auto base_ptr = m_buff + old_ref.cursor;
			auto mem_info = reinterpret_cast<MemInfo*>(base_ptr);
			mem_info->size = static_cast<index_t>(bytes);
			return static_cast<mem_t>(base_ptr + meta_info_size);
		}

		[[nodiscard]] constexpr auto reallocate(mem_t mem, size_type bytes) noexcept -> std::expected<mem_t, AllocatorError> {
			auto old_size = get_ptr_size(mem);
			if (old_size >= bytes) return mem;
			auto maybe_refs = try_realloc(mem, bytes);
			if (!maybe_refs) return std::unexpected(maybe_refs.error());

			auto temp = *maybe_refs;

			while (!m_ref_cursor.compare_exchange_weak(std::get<0>(temp), std::get<1>(temp), std::memory_order::acq_rel, std::memory_order::acquire)) {
				maybe_refs = try_realloc(mem, bytes);
				if (!maybe_refs.has_value()) return std::unexpected(maybe_refs.error());

				temp = *maybe_refs;
			}

			auto ptr = std::get<2>(temp);	
			auto& mem_info = get_mem_info(ptr);
			mem_info.size = static_cast<index_t>(bytes);

			if (ptr != mem) {
				std::memcpy(ptr, mem, old_size);
			}
			return ptr;
		}

		constexpr auto deallocate(mem_t mem) noexcept -> void {
			if (mem == nullptr || !is_valid_ptr(mem)) return;	
			auto temp = try_dealloc(mem);

			while (!m_ref_cursor.compare_exchange_weak(temp.first, temp.second, std::memory_order::acq_rel, std::memory_order::acquire)) {
				temp = try_dealloc(mem);
			}
		}

		constexpr auto get_ref() const noexcept -> Ref {
			auto temp = m_ref_cursor.load(std::memory_order::relaxed);
			return from_ref_cursor(temp);
		}

		constexpr auto update_ref(Ref r) noexcept {
			m_ref_cursor.store(to_ref_cursor(r));
		}

		friend void swap(BumpAllocator& lhs, BumpAllocator& rhs) noexcept {
			using std::swap;
			auto t = lhs.m_ref_cursor.load(std::memory_order::relaxed);
			lhs.m_ref_cursor.store(rhs.m_ref_cursor.load(std::memory_order::relaxed));
			rhs.m_ref_cursor.store(t);
			swap(lhs.m_buff, rhs.m_buff);
			swap(lhs.m_size, rhs.m_size);
			swap(lhs.m_owned, rhs.m_owned);
		}

		constexpr auto free_space() const noexcept -> size_type {
			return m_size - get_ref().cursor;
		}

		[[nodiscard]] constexpr auto is_valid_ptr(mem_t mem) const noexcept -> bool {
			auto ptr = static_cast<char*>(mem);
			return ptr >= m_buff && ptr < m_buff + m_size; 
		}

		constexpr auto size() const noexcept -> size_type { return m_size; }
		constexpr auto is_owned() const noexcept -> bool { return m_owned; }

		constexpr auto reset() noexcept -> void {
			update_ref({ .ref_count = 0, .cursor = 0 });
		}

		constexpr auto get_ptr_size(mem_t mem) const noexcept -> size_type {
			auto mem_info = get_mem_info(mem);
			return mem_info.size;
		}

		constexpr auto buff_start_ptr() const noexcept -> char const* { return m_buff; }

	private:
		[[nodiscard]] constexpr auto from_ref_cursor(size_type ref_cursor) const noexcept -> Ref {
			return {
				.ref_count = static_cast<index_t>(ref_cursor >> 32),
				.cursor = static_cast<index_t>(ref_cursor)
			};
		}

		[[nodiscard]] constexpr auto to_ref_cursor(Ref r) const noexcept -> size_type {
			return (static_cast<size_type>(r.ref_count) << 32) | r.cursor;
		}

		[[nodiscard]] constexpr auto try_alloc(size_type bytes) noexcept -> std::expected<std::pair<size_type /*old*/, size_type/*new*/>, AllocatorError> {
			auto old_ref = get_ref();
			if (old_ref.cursor + bytes > m_size) {
				std::println(stderr, "Alloc: cursor => {}, bytes => {}, size => {} | {} | {}", old_ref.cursor, bytes, m_size, old_ref.cursor + bytes, free_space());
				return std::unexpected(AllocatorError::no_space);
			}
			
			auto new_ref = Ref{
				.ref_count = old_ref.ref_count + 1,
				.cursor = static_cast<index_t>(old_ref.cursor + bytes)
			};
			
			return std::make_pair(to_ref_cursor(old_ref), to_ref_cursor(new_ref)) ;
		}

		[[nodiscard]] constexpr auto try_realloc(mem_t mem, size_type bytes) noexcept -> std::expected<std::tuple<size_type /*old*/, size_type/*new*/, mem_t>, AllocatorError> {
			auto old_ref = get_ref();
			{
				auto ptr = static_cast<char*>(mem);
				auto mem_info = get_mem_info(mem);

				if (ptr + mem_info.size == m_buff + old_ref.cursor) {
					auto diff = static_cast<index_t>(bytes - mem_info.size);
					if (old_ref.cursor + diff > m_size) return std::unexpected(AllocatorError::no_space);
					auto new_ref = Ref { .ref_count = old_ref.ref_count, .cursor = old_ref.cursor + diff };
					return std::make_tuple(to_ref_cursor(old_ref), to_ref_cursor(new_ref), mem);
				}
			}

			auto const size = bytes + meta_info_size;
			if (old_ref.cursor + size > m_size) return std::unexpected(AllocatorError::no_space);
			auto new_ref = old_ref;
			new_ref.cursor += static_cast<index_t>(size);
			auto base_ptr = m_buff + old_ref.cursor;
			auto ptr = static_cast<mem_t>(base_ptr + meta_info_size);

			return std::make_tuple(to_ref_cursor(old_ref), to_ref_cursor(new_ref), ptr);
		}

		[[nodiscard]] auto try_dealloc(mem_t mem) noexcept -> std::pair<size_type /*old*/, size_type/*new*/> {
			auto old_ref = get_ref();
			auto ptr = static_cast<char*>(mem);
			auto mem_info = get_mem_info(mem);
			auto const actual_size = static_cast<index_t>(mem_info.size + meta_info_size);

			auto new_ref = Ref { .ref_count = old_ref.ref_count - 1, .cursor = old_ref.cursor - actual_size };

			if (ptr + mem_info.size == m_buff + old_ref.cursor) {
				if (new_ref.ref_count == 0) new_ref.cursor = 0;
				return { to_ref_cursor(old_ref), to_ref_cursor(new_ref) };
			}
			
			if (new_ref.ref_count == 0) new_ref.cursor = 0;
			else new_ref.cursor = old_ref.cursor;

			return { to_ref_cursor(old_ref), to_ref_cursor(new_ref) };
		}

		auto get_mem_info(mem_t mem) const noexcept -> MemInfo& {
			auto ptr = static_cast<char*>(mem);
			return *reinterpret_cast<MemInfo*>(ptr - meta_info_size);
		}
	private:
		alignas(alignof(std::max_align_t)) std::atomic<std::size_t> m_ref_cursor{}; // |ref_count(32bit)|cursor(32bit)|
		[[maybe_unused]] char padding[128 - sizeof(m_ref_cursor)]; // to avoid false sharing
		char* m_buff{nullptr};
		size_type m_size{};
		bool m_owned{true};
	};


	// INFO: Assumption: 1. allocation can never fail.
	//					 2. each thread gets its own copy of block allocator.
	//					 3. no exception can be thrown from the allocator
	struct BlockAllocator {
		using mem_t = void*;
		using size_type = std::size_t;
		using index_t = std::uint32_t;

		static constexpr size_type extra_space = 32;

		BlockAllocator(size_type block_size, std::string_view name = {}, bool reuse = false) noexcept
			: m_block_size(block_size + extra_space)
			, m_reuse(reuse)
			, m_name(name)
		{}
		BlockAllocator(char* buff, size_type block_size, std::string_view name = {}, bool reuse = false) noexcept
			: m_block_size(block_size)
			, m_reuse(reuse)
			, m_name(name)
		{
			m_blocks.emplace_front(buff, block_size);
		}
		template <size_type N>
		BlockAllocator(char (&buff)[N], std::string_view name = {}, bool reuse = false) noexcept
			: BlockAllocator(buff, N, name, reuse)
		{
		}

		BlockAllocator(BlockAllocator const&) = delete;
		BlockAllocator& operator=(BlockAllocator const&) = delete;
		BlockAllocator(BlockAllocator&&) noexcept = default;
		BlockAllocator& operator=(BlockAllocator&&) noexcept = default;
		~BlockAllocator() noexcept = default;

		template <typename T>
		auto allocate(size_type size) noexcept -> T* {
			auto actual_size = size * sizeof(T);
			auto& block = ensure_space(actual_size);
			auto ptr_or = block.allocate(actual_size);
			assert(ptr_or.has_value() && "No space left"); // This should never happen since we ensure enough space.
			return reinterpret_cast<T*>(*ptr_or);
		}

		template <typename T>
		auto deallocate(T* mem) noexcept -> void {
			auto* ptr = static_cast<mem_t>(mem);
			for (auto it = m_blocks.begin(); it != m_blocks.end(); ++it) {
				auto& block = *it;
				if (block.is_valid_ptr(ptr)) {
					block.deallocate(ptr);
					return;
				}
			}
		}

		template <typename T>
		auto reallocate(T* mem, size_type size) noexcept -> T* {
			auto* ptr = static_cast<mem_t>(mem);
			auto const actual_size = size * sizeof(T);
			ensure_space(actual_size);

			for (auto it = m_blocks.begin(); it != m_blocks.end(); ++it) {
				auto& block = *it;
				if (block.is_valid_ptr(ptr)) {
					auto ptr_or = block.reallocate(ptr, actual_size);
					auto new_ptr = mem;
					if (!ptr_or) {
						new_ptr = allocate<T>(size);
						auto old_size = block.get_ptr_size(ptr);
						std::memcpy(new_ptr, ptr, old_size);
						block.deallocate(ptr);
					} else {
						new_ptr = reinterpret_cast<T*>(*ptr_or);
					}
					return new_ptr;
				}
			}
			assert(false && "Ensure space did not work properly.");
			std::unreachable();
		}

		auto push_state() noexcept {
			if (empty()) {
				m_state.push({ .ptr = nullptr, .ref = {}, .cap = m_block_size });
			} else {
				auto& temp = front();
				m_state.push({ .ptr = temp.buff_start_ptr(), .ref = temp.get_ref(), .cap = m_block_size });
			}
		}

		auto push_state(size_type cap) noexcept -> void {
			cap += extra_space;
			if (empty()) {
				m_state.push({ .ptr = nullptr, .ref = {}, .cap = cap });
				ensure_space(cap);
				return;
			}
			
			auto& temp = front();
			m_state.push({ .ptr = temp.buff_start_ptr(), .ref = temp.get_ref(), .cap = cap });
			ensure_space(cap);
		}

		auto push_state(char* buff, size_type size) noexcept -> void {
			if (empty()) {
				m_state.push({ .ptr = nullptr, .ref = {}, .cap = m_block_size });
			} else {
				auto& temp = front();
				m_state.push({ .ptr = temp.buff_start_ptr(), .ref = temp.get_ref(), .cap = size });
			}
			m_blocks.emplace_front(buff, size);
		}
	
		template <std::size_t N>
		auto push_state(char (&buff)[N]) noexcept -> void {
			return push_state(buff, N);
		}

		auto pop_state() noexcept -> void {
			assert(!empty() && "Blocks must not be empty.");
			assert(!m_state.empty() && "pop is called on empty state.");
			if (empty() || m_state.empty()) return;
			auto top = m_state.top();
			m_state.pop();

			while (!empty() && top.ptr != m_blocks.front().buff_start_ptr()) {
				auto temp = std::move(m_blocks.front());
				m_blocks.pop_front();
				temp.reset();
				if (!temp.is_owned()) continue;
				m_free_blocks.push_front(std::move(temp));
			}

			if (top.ptr == nullptr || empty()) return;
			auto& temp = m_blocks.front();
			temp.update_ref(top.ref);
		}

		constexpr auto empty() const noexcept -> bool { return m_blocks.empty(); }
		constexpr auto blocks() const noexcept -> size_type { return m_blocks.size(); }
		constexpr auto free_blocks() const noexcept -> size_type { return m_free_blocks.size(); }
		constexpr auto current_block_size() const noexcept -> size_type {
			if (m_state.empty()) return m_block_size;
			return m_state.top().cap;
		}
		constexpr auto front() const noexcept -> BumpAllocator const& {
			assert(!empty());
			return m_blocks.front();
		}

		constexpr auto front() noexcept -> BumpAllocator& {
			assert(!empty());
			return m_blocks.front();
		}

		constexpr auto reset() noexcept -> void {
			if (!m_reuse) {
				m_free_blocks.clear();
				m_blocks.clear();
				return;
			}
			while (m_blocks.size() > 1) {
				auto& temp = m_blocks.front();
				temp.reset();
				m_free_blocks.push_back(std::move(temp));
				m_blocks.pop_front();
			}
			front().reset();
		}

		constexpr auto name() const noexcept -> std::string_view { return m_name; }
		constexpr auto set_name(std::string_view name) noexcept -> void { m_name = name; }

	private:
		auto ensure_space(size_type size) -> BumpAllocator& {
			auto block_size = current_block_size();
			auto sz = size + BumpAllocator::meta_info_size;
		
			if (current_block_size() < sz) {
				// INFO: If exceeded the capacity, then there's a high probility that we're
				// going to be hit with larger value next;
				block_size = static_cast<std::size_t>(static_cast<float>(sz) * 1.6f);
			}

			auto insert_block = [this, block_size] {
				auto it = std::find_if(m_free_blocks.begin(), m_free_blocks.end(), [block_size](auto const& alloc) {
					return alloc.size() >= block_size;
				});	
				if (it == m_free_blocks.end()) {
					m_blocks.emplace_front(block_size);
				} else {
					m_blocks.push_front(std::move(*it));
					m_free_blocks.erase(it);
				}
			};

			for (auto it = m_blocks.begin(); it != m_blocks.end(); ++it) {
				auto& block = *it;
				if (block.free_space() >= size + BumpAllocator::meta_info_size) {
					return block;
				}
			}

			insert_block();
			return front();
		}
		struct CursorState {
			char const*			ptr;
			BumpAllocator::Ref	ref;
			size_type			cap;
		};
	private:
		std::list<BumpAllocator> m_blocks;
		std::list<BumpAllocator> m_free_blocks;
		std::stack<CursorState> m_state;
		size_type m_block_size;
		bool m_reuse{false};
		std::string_view m_name;
	};

	struct AllocatorManager {

		static constexpr std::size_t global_bump_allocator_size = /*32Mib*/ 1024 * 1024 * 16; // 16Mib
		static constexpr std::size_t temp_buff_allocator_size = /*32Mib*/ 1024 * 1024 * 2; // 2Mib
		static constexpr std::string_view global_allocator_name = "global";
		static constexpr std::string_view temp_allocator_name = "temp";

		AllocatorManager(std::size_t global_size = global_bump_allocator_size, std::string_view global_name = global_allocator_name) 
			: m_global(std::make_unique<BlockAllocator>(global_size, global_name))
			, m_current(m_global.get())
		{}
		AllocatorManager(AllocatorManager const&) = delete;
		AllocatorManager& operator=(AllocatorManager const&) = delete;
		AllocatorManager(AllocatorManager &&) noexcept = default;
		AllocatorManager& operator=(AllocatorManager &&) noexcept = default;
		~AllocatorManager() = default;

		static auto instance() noexcept -> AllocatorManager& {
			static auto manager = AllocatorManager();
			return manager;
		}

		constexpr auto global_alloc() const noexcept -> BlockAllocator* { 
			return m_global.get();
		}

		constexpr auto current_alloc() const noexcept -> BlockAllocator* {
			if (m_current) return m_current;
			return m_global.get();
		}

		constexpr auto temp_alloc() const noexcept -> BlockAllocator* {
			return m_temp.get();
		}

		constexpr auto is_temp_alloc_set() const noexcept -> bool {
			return m_current == m_temp.get();
		}

		template <typename... Args>
			requires std::constructible_from<BlockAllocator, Args...>
		auto set_temp_alloc(Args&&... args) -> BlockAllocator* {
			if (m_temp) {
				m_current = m_temp.get();
			} else {
				m_temp = std::make_unique<BlockAllocator>(std::forward<Args>(args)...);
				if (m_temp->name().empty()) m_temp->set_name(temp_allocator_name);
				m_current = m_temp.get();
			}
			return m_current;
		}

		auto remove_temp_alloc() -> void {
			m_current = m_global.get();
			if (!m_temp) return;
			m_temp->reset();
		}

		constexpr auto is_valid_allocator(BlockAllocator const* ptr) const noexcept -> bool {
			return ptr == m_temp.get() || ptr == m_global.get();
		}

	private:
		std::unique_ptr<BlockAllocator> m_global;
		std::unique_ptr<BlockAllocator> m_temp{nullptr};
		BlockAllocator* m_current{};
	};

	static inline auto current_block_allocator() noexcept -> BlockAllocator* {
		return AllocatorManager::instance().current_alloc();
	}

	struct TempAllocatorScope {
		template <typename... Args>
			requires std::constructible_from<BlockAllocator, Args...>
		constexpr TempAllocatorScope(Args&&... args) noexcept
			: TempAllocatorScope(AllocatorManager::instance(), std::forward<Args>(args)...)
		{}
	
		template <typename... Args>
			requires std::constructible_from<BlockAllocator, Args...>
		constexpr TempAllocatorScope(AllocatorManager& manager, Args&&... args) noexcept
			: m_manager(manager) 
		{
			if (m_manager.is_temp_alloc_set()) {
				return;
			}
			m_alloc = m_manager.set_temp_alloc(std::forward<Args>(args)...);
		}
		constexpr TempAllocatorScope() noexcept
			: TempAllocatorScope(AllocatorManager::temp_buff_allocator_size, AllocatorManager::temp_allocator_name, true)
		{}

		constexpr TempAllocatorScope(TempAllocatorScope const&) noexcept = delete;
		constexpr TempAllocatorScope& operator=(TempAllocatorScope const&) noexcept = delete;
		constexpr TempAllocatorScope(TempAllocatorScope &&) noexcept = delete;
		constexpr TempAllocatorScope& operator=(TempAllocatorScope &&) noexcept = delete;
		constexpr ~TempAllocatorScope() noexcept {
			if (m_alloc) {
				m_manager.remove_temp_alloc();
			}
		}
	private:
		AllocatorManager& m_manager;
		BlockAllocator* m_alloc{nullptr};
	};
} // namespace dark::uitls

#ifndef DEBUG_ALLOCATOR
	#undef debug_alloc
#endif

#endif // DARK_UTILS_ALLOCATOR_HPP
