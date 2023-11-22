// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !defined(TRACE_UE_COMPAT_LAYER)
#	if defined(__UNREAL__) && __UNREAL__
#		define TRACE_UE_COMPAT_LAYER	0
#	else
#		define TRACE_UE_COMPAT_LAYER	1
#	endif
#endif

#if TRACE_UE_COMPAT_LAYER

// platform defines
#define PLATFORM_ANDROID	0
#define PLATFORM_APPLE		0
#define PLATFORM_HOLOLENS	0
#define PLATFORM_MAC		0
#define PLATFORM_UNIX		0
#define PLATFORM_WINDOWS	0

#ifdef _WIN32
#	undef PLATFORM_WINDOWS
#	define PLATFORM_WINDOWS			1
#elif defined(__linux__)
#	undef  PLATFORM_UNIX
#	define PLATFORM_UNIX			1
#elif defined(__APPLE__)
#	undef  PLATFORM_MAC
#	define PLATFORM_MAC				1
#	undef  PLATFORM_APPLE
#	define PLATFORM_APPLE			1
#endif

// arch defines
#if defined(__amd64__) || defined(_M_X64)
#	define PLATFORM_CPU_X86_FAMILY	1
#	define PLATFORM_CPU_ARM_FAMILY	0
#	define PLATFORM_64BITS			1
#	define PLATFORM_CACHE_LINE_SIZE	64
#elif defined(__arm64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
#	define PLATFORM_CPU_X86_FAMILY	0
#	define PLATFORM_CPU_ARM_FAMILY	1
#	define PLATFORM_64BITS			1
#	define PLATFORM_CACHE_LINE_SIZE	64
#else
#	error Unknown architecture
#endif

// external includes
#if defined(_MSC_VER)
#	define _ENABLE_EXTENDED_ALIGNED_STORAGE
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <memory>

#if PLATFORM_WINDOWS
#	if !defined(WIN32_LEAN_AND_MEAN)
#		define WIN32_LEAN_AND_MEAN
#	endif
#	if !defined(NOGDI)
#		define NOGDI
#	endif
#	if !defined(NOMINMAX)
#		define NOMINMAX
#	endif
#	include <Windows.h>
#	undef min
#	undef max
#	undef GetEnvironmentVariable
#	undef SendMessage
#	undef InterlockedIncrement
#	undef InterlockedDecrement
#	undef InterlockedAdd
#	undef InterlockedExchange
#	undef InterlockedExchangeAdd
#	undef InterlockedCompareExchange
#	undef InterlockedCompareExchangePointer
#	undef InterlockedExchange64
#	undef InterlockedExchangeAdd64
#	undef InterlockedCompareExchange64
#	undef InterlockedIncrement64
#	undef InterlockedDecrement64
#	undef InterlockedAnd
#	undef InterlockedOr
#	undef InterlockedXor
#endif

// types
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using UPTRINT = uintptr_t;
using PTRINT = intptr_t;

using SIZE_T = size_t;

#if PLATFORM_WINDOWS
#	undef TEXT
#endif
#define TEXT(x)	x
#define TCHAR	ANSICHAR
using ANSICHAR = char;
using WIDECHAR = char16_t;

// keywords
#if defined(_MSC_VER)
#	define FORCENOINLINE	__declspec(noinline)
#	define FORCEINLINE		__forceinline
#else
#	define FORCENOINLINE	inline __attribute__((noinline))
#	define FORCEINLINE		inline __attribute__((always_inline))
#endif

#if defined(_MSC_VER)
#	define LIKELY(x)		x
#	define UNLIKELY(x)		x
#else
#	define LIKELY(x)		__builtin_expect(!!(x), 1)
#	define UNLIKELY(x)		__builtin_expect(!!(x), 0)
#endif

#define UE_ARRAY_COUNT(x)	(sizeof(x) / sizeof(x[0]))

#if !defined(IS_MONOLITHIC)
#	define IS_MONOLITHIC				0
#endif

// misc defines
#define TSAN_SAFE
#define THIRD_PARTY_INCLUDES_END
#define THIRD_PARTY_INCLUDES_START
#define TRACE_ENABLED					1
#define TRACE_PRIVATE_CONTROL_ENABLED	0
#define TRACE_PRIVATE_EXTERNAL_LZ4		1
#define UE_LAUNDER(x)					(x)
#define UE_DEPRECATED(x, ...)
#define UE_TRACE_ENABLED				TRACE_ENABLED
#define PREPROCESSOR_JOIN(a,b)			PREPROCESSOR_JOIN_IMPL(a,b)
#define PREPROCESSOR_JOIN_IMPL(a,b)		a##b
#define DEFINE_LOG_CATEGORY_STATIC(...)
#define UE_LOG(...)
#define check(x)						do { if (!(x)) __debugbreak(); } while (0)
#define checkf(x, ...)					check(x)

#if !defined(UE_BUILD_SHIPPING)
#	define UE_BUILD_SHIPPING			1
#endif
#define UE_BUILD_TEST					0

#if defined(__has_builtin)
#	if __has_builtin(__builtin_trap)
#		define PLATFORM_BREAK()			do { __builtin_trap(); } while (0)
#	endif
#elif defined(_MSC_VER)
#	define PLATFORM_BREAK()				do { __debugbreak(); } while (0)
#endif

#if !defined(PLATFORM_BREAK)
#	define PLATFORM_BREAK()				do { *(int*)0x493 = 0x493; } while (0)
#endif

// api
template <typename T> inline auto Forward(T t) { return std::forward<T>(t); }
template <typename T> inline auto MoveTemp(T&& t) { return std::move<T>(t); }
template <typename T> inline void Swap(T& x, T& y) { return std::swap(x, y); }



template <typename T, typename ALLOCATOR = void>
struct TArray
	: public std::vector<T>
{
	using	 Super = std::vector<T>;
	using	 Super::vector;
	using	 Super::back;
	using	 Super::begin;
	using	 Super::clear;
	using	 Super::data;
	using	 Super::emplace_back;
	using	 Super::empty;
	using	 Super::end;
	using	 Super::erase;
	using	 Super::front;
	using	 Super::insert;
	using	 Super::pop_back;
	using	 Super::push_back;
	using	 Super::reserve;
	using	 Super::resize;
	using	 Super::size;
	void	 Add(const T& t) { push_back(t); }
	T& Add_GetRef(const T& t) { push_back(t); return back(); }
	size_t	 AddUninitialized(size_t n) { resize(size() + n); return size() - n; }
	void	 Emplace() { emplace_back(); }
	T& Emplace_GetRef() { emplace_back(); return back(); }
	void	 Insert(T const& t, size_t i) { insert(begin() + i, t); }
	void	 Push(const T& t) { push_back(t); }
	T		 Pop(bool Eh = true) { T t = back(); pop_back(); return t; }
	size_t	 Num() const { return size(); }
	void	 SetNum(size_t num) { resize(num); }
	void	 SetNumZeroed(size_t num) { resize(num, 0); }
	void	 SetNumUninitialized(size_t num) { resize(num); }
	void	 Reserve(size_t num) { reserve(num); }
	T& Last() { return back(); }
	T* GetData() { return data(); }
	T const* GetData() const { return data(); }
	bool	 IsEmpty() const { return empty(); }
	void	 Empty() { clear(); }
	void	 Reset() { clear(); }
	void	 RemoveAll(bool (*p)(T const&)) { erase(remove_if(begin(), end(), p), end()); }
	void	 RemoveAllSwap(bool (*p)(T const&)) { RemoveAll(p); }

	struct NotLess {
		bool operator () (T const& Lhs, T const& Rhs) const {
			return bool(Lhs < Rhs) == false;
		}
	};
	void	 Heapify() { std::make_heap(begin(), end(), NotLess()); }
	void	 HeapPush(T const& t) { Add(t); std::push_heap(begin(), end(), NotLess()); }
	void	 HeapPopDiscard() { std::pop_heap(begin(), end(), NotLess()); pop_back(); }
	T const& HeapTop() const { return front(); }
};

template <size_t S> struct TInlineAllocator {};

template <typename T>
struct TArrayView
{
	TArrayView() = default;
	TArrayView(T* data, size_t size) : _data(data), _size(size) {}
	T* GetData() { return _data; }
	T const* GetData() const { return _data; }
	size_t		Num() const { return _size; }
	T* _data = nullptr;
	size_t		_size = 0;
	using		value_type = T;
};


template <typename T>
struct TStringViewAdapter
	: public std::basic_string_view<T>
{
	using		Super = std::basic_string_view<T>;
	using		Super::basic_string_view;
	using		Super::size;
	using		Super::data;
	size_t		Len() const { return size(); }
	const char* GetData() const { return data(); }
};

struct FStringProxy
{
	FStringProxy() = default;
	template <typename T>	FStringProxy(size_t len, const T* ptr);
	template <typename T>	FStringProxy(const T& r) : FStringProxy(r.size(), r.data()) {}
	std::string				_str;
	const void* _ptr;
	uint32_t				_len;
	uint32_t				_char_size;
	std::string_view		ToView();
};

template <typename T>
inline FStringProxy::FStringProxy(size_t len, const T* ptr)
	: _ptr(ptr)
	, _len(uint32_t(len))
	, _char_size(sizeof(T))
{
}

inline std::string_view FStringProxy::ToView()
{
	if (_char_size != 1)
	{
		_char_size = 1;
		_str.reserve(_len);
		char* __restrict cursor = _str.data();
		for (uint32_t i = 0, n = _len; i < n; ++i)
			cursor[i] = char(((const char16_t*)_ptr)[i]);
		_ptr = cursor;
	}

	return std::string_view((const char*)_ptr, _len);
}

using FAnsiStringView = TStringViewAdapter<char>;
using FWideStringView = TStringViewAdapter<char16_t>;
using FUtf8StringView = TStringViewAdapter<char>;
using FString = struct FStringProxy;
using ANSITEXTVIEW = FAnsiStringView;

template<typename T>
class TSharedRef : public std::shared_ptr<T>
{
public:
	using Super = std::shared_ptr<T>;
	using Super::operator->;
	using Super::operator bool;

	TSharedRef(TSharedRef<T>&& other)
		: std::shared_ptr<T>(other)
	{ }

	TSharedRef(std::shared_ptr<T> ptr)
		: std::shared_ptr<T>(std::move(ptr))
	{ }

	T& Get() { return *Super::get(); }
	T* Release() { return Super::release(); }
};

template<typename T>
class TSharedPtr : std::shared_ptr<T>
{
public:
	using Super = std::shared_ptr<T>;
	using Super::operator->;
	using Super::operator bool;

	TSharedPtr(TSharedPtr&& other)
		: std::shared_ptr<T>(other)
	{ }

	TSharedPtr(std::shared_ptr<T> ptr)
		: std::shared_ptr<T>(std::move(ptr))
	{ }

	T* Get() { return Super::get(); }
	T* Release() { return Super::release(); }
};

template <typename InObjectType, typename... InArgTypes>
[[nodiscard]] FORCEINLINE TSharedRef<InObjectType> MakeShared(InArgTypes&&... Args)
{
	return TSharedRef<InObjectType>(std::make_shared<InObjectType>(std::forward<InArgTypes>(Args)...));
}

template<typename T>
class TUniquePtr : std::unique_ptr<T>
{
public:
	using Super = std::unique_ptr<T>;
	using Super::operator->;
	using Super::operator bool;

	TUniquePtr(T* ptr)
		: std::unique_ptr<T>(ptr)
	{ }

	TUniquePtr(std::unique_ptr<T> ptr)
		: std::unique_ptr<T>(std::move(ptr))
	{ }

	T* Get() { return Super::get(); }
	T* Release() { return Super::release(); }
};

template <typename InObjectType, typename... InArgTypes>
[[nodiscard]] FORCEINLINE TUniquePtr<InObjectType> MakeUnique(InArgTypes&&... Args)
{
	return TUniquePtr<InObjectType>(std::make_unique<InObjectType>(std::forward<InArgTypes>(Args)...));
}

struct FPlatformAtomics
{
	static long long AtomicRead(const volatile int64* Ptr)
	{
#if PLATFORM_WINDOWS
		return _InterlockedCompareExchange64(const_cast<volatile long long*>(Ptr), 0, 0);
#else
		return std::atomic_load((std::atomic<long long>*)Ptr);
#endif
	}

	static void AtomicStore(volatile int64* Ptr, int64 Value)
	{
#if PLATFORM_WINDOWS
		_InterlockedExchange64(Ptr, Value);
#else
		return std::atomic_store((std::atomic<long long>*)Ptr, Value);
#endif
	}

	static int32 InterlockedIncrement(volatile int32* Ptr)
	{
#if PLATFORM_WINDOWS
		return _InterlockedIncrement((long*)Ptr);
#else
		return std::atomic_fetch_add((std::atomic<long>*)Ptr, 1) + 1;
#endif
	}

	static long InterlockedDecrement(volatile int32* Ptr)
	{
#if PLATFORM_WINDOWS
		return _InterlockedDecrement((long*)Ptr);
#else
		return std::atomic_fetch_add((std::atomic<long>*)Ptr, -1) - 1;
#endif
	}

	static long long InterlockedAdd(volatile int64* Ptr, int64 Value)
	{
#if PLATFORM_WINDOWS
		return _InterlockedExchangeAdd64(Ptr, Value);
#else
		return std::atomic_fetch_add((std::atomic<long long>*)Ptr, Value) + Value;
#endif
	}

	static long InterlockedAnd(volatile int32* Ptr, int32 Value)
	{
#if PLATFORM_WINDOWS
		return _InterlockedAnd((long*)Ptr, Value);
#else
		return std::atomic_fetch_and((std::atomic<long>*)Ptr, Value);
#endif
	}

	static long long InterlockedAnd(volatile int64* Ptr, int64 Value)
	{
#if PLATFORM_WINDOWS
		return InterlockedAnd64(Ptr, Value);
#else
		return std::atomic_fetch_and((std::atomic<long long>*)Ptr, Value);
#endif
	}

	static long long InterlockedOr(volatile int64* Ptr, int64 Value)
	{
#if PLATFORM_WINDOWS
		return InterlockedOr64(Ptr, Value);
#else
		return std::atomic_fetch_or((std::atomic<long long>*)Ptr, Value);
#endif
	}

	static long long InterlockedIncrement(volatile int64* Ptr)
	{
#if PLATFORM_WINDOWS
		return _InterlockedIncrement64(Ptr);
#else
		return std::atomic_fetch_add((std::atomic<long long>*)Ptr, 1) + 1;
#endif
	}

	static long long InterlockedExchange(volatile int64* Ptr, int64 Exchange)
	{
#if PLATFORM_WINDOWS
		return _InterlockedExchange64(Ptr, Exchange);
#else
		return std::atomic_exchange((std::atomic<long long>*)Ptr, Exchange);
#endif
	}

	static long InterlockedCompareExchange(volatile int32* Ptr, int32 Exchange, int32 Comperand)
	{
#if PLATFORM_WINDOWS
		return _InterlockedCompareExchange((long*)Ptr, Exchange, Comperand);
#else
		return std::atomic_compare_exchange_strong((std::atomic<long>*)Ptr, &Comperand, Exchange)? Exchange : Comperand;
#endif
	}

	static long long InterlockedCompareExchange(volatile int64* Ptr, int64 Exchange, int64 Comperand)
	{
#if PLATFORM_WINDOWS
		return _InterlockedCompareExchange64(Ptr, Exchange, Comperand);
#else
		return std::atomic_compare_exchange_strong((std::atomic<long long>*)Ptr, &Comperand, Exchange)? Exchange : Comperand;
#endif
	}
};

namespace UECompat
{

	template <typename T>
	inline T		Max(T const& a, T const& b) { return std::max(a, b); }
	inline size_t	Strlen(const char* str) { return ::strlen(str); }
	inline void* Malloc(size_t size) { return ::malloc(size); }
	inline void* Realloc(void* ptr, size_t size) { return ::realloc(ptr, size); }
	inline void		Free(void* ptr) { return ::free(ptr); }
	inline void		Memcpy(void* d, const void* s, size_t n) { ::memcpy(d, s, n); }

	template <typename T, typename COMPARE = std::less<typename T::value_type>>
	inline void StableSort(T& container, COMPARE&& compare = COMPARE())
	{
		std::stable_sort(container.GetData(), container.GetData() + container.Num(),
			std::forward<COMPARE>(compare));
	}

	template <typename T, typename COMPARE = std::less<typename T::value_type>>
	inline void Sort(T& container, COMPARE&& compare = COMPARE())
	{
		std::sort(container.GetData(), container.GetData() + container.Num(),
			std::forward<COMPARE>(compare));
	}

	template <typename T, typename P>
	inline void SortBy(T& container, P const& p)
	{
		Sort(container, [&p](typename T::value_type& lhs, typename T::value_type& rhs) {
			return p(lhs) < p(rhs);
			});
	}

	template <typename T, typename V, typename P>
	inline int LowerBoundBy(T const& container, V const& v, P const& p)
	{
		auto adapter = [&p](typename T::value_type const& lhs, V const& v) {
			return p(lhs) < v;
			};
		auto* first = container.GetData();
		auto* last = first + container.Num();
		auto iter = std::lower_bound(first, last, v, adapter);
		return int(ptrdiff_t(iter - first));
	}

} // namespace UeCompat

namespace Algo = UECompat;
namespace FMath = UECompat;
namespace FCStringAnsi = UECompat;
namespace FMemory = UECompat;

#endif // TRACE_UE_COMPAT_LAYER

#include <cstring>

#if PLATFORM_WINDOWS
#	pragma warning(push)
#	pragma warning(disable : 4200) // zero-sized arrays
#	pragma warning(disable : 4201) // anonymous structs
#	pragma warning(disable : 4127) // conditional expr. is constant
#endif
