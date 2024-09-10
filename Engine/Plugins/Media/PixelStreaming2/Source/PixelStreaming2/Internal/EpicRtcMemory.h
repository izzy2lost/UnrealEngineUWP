// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/RefCounting.h"

namespace UE::PixelStreaming2
{

	/**
	 * Ref-counting mixin, designed to add ref-counting to an object without requiring a virtual destructor.
	 * Implements support for AutoRTFM, is thread-safe by default, and can support custom deleters via T::StaticDestroyObject.
	 *
	 * @note This class is almost a 1:1 copy of TRefCountingMixin except for the initial ref count being 1. This is required
	 * by EpicRtc
	 */
	template <typename T, ERefCountingMode Mode = ERefCountingMode::ThreadSafe>
	class PIXELSTREAMING2_API TEpicRtcRefCountPtr
	{
		using RefCountType = std::conditional_t<Mode == ERefCountingMode::ThreadSafe, std::atomic<uint32>, uint32>;

	public:
		TEpicRtcRefCountPtr() = default;

		TEpicRtcRefCountPtr(const TEpicRtcRefCountPtr&) = delete;
		TEpicRtcRefCountPtr& operator=(const TEpicRtcRefCountPtr&) = delete;

		uint32 AddRef() const
		{
			if constexpr (Mode == ERefCountingMode::ThreadSafe)
			{
				// Incrementing a reference count with relaxed ordering is always safe because no other action is taken
				// in response to the increment, so there's nothing to order with.

				AutoRTFM::OnCommit([this] {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
					// We do a regular SC increment here because it maps to an _InterlockedIncrement (lock inc).
					// The codegen for a relaxed fetch_add is actually much worse under MSVC (lock xadd).
					++RefCount;
#else
					RefCount.fetch_add(1, std::memory_order_relaxed);
#endif
				});
			}
			else
			{
				++RefCount;
			}

			// Note: TRefCountPtr doesn't use the return value
			return 0;
		}

		uint32 Release() const
		{
			if constexpr (Mode == ERefCountingMode::ThreadSafe)
			{
				AutoRTFM::OnCommit([this] {
					// std::memory_order_acq_rel is used here so that, if we do end up executing the destructor, it's not possible
					// for side effects from executing the destructor end up being visible before we've determined that the
					// reference count is actually zero.

					uint32 OldCount = RefCount.fetch_sub(1, std::memory_order_acq_rel);
					checkSlow(OldCount > 0);
					if (OldCount == 1)
					{
						T::StaticDestroyObject(static_cast<const T*>(this));
					}
				});
			}
			else
			{
				checkSlow(RefCount > 0);

				if (--RefCount == 0)
				{
					T::StaticDestroyObject(static_cast<const T*>(this));
				}
			}

			// Note: TRefCountPtr doesn't use the return value
			return 0;
		}

		uint32 GetRefCount() const
		{
			if constexpr (Mode == ERefCountingMode::ThreadSafe)
			{
				// A 'live' reference count is unstable by nature and so there's no benefit
				// to try and enforce memory ordering around the reading of it.
				//
				// This is equivalent to https://en.cppreference.com/w/cpp/memory/shared_ptr/use_count

				uint32 Count = 0;

				UE_AUTORTFM_OPEN
				{
					// This reference count may be accessed by multiple threads
					Count = RefCount.load(std::memory_order_relaxed);
				};

				return Count;
			}
			else
			{
				return RefCount;
			}
		}

		static void StaticDestroyObject(const T* Obj)
		{
			delete Obj;
		}

	private:
		mutable RefCountType RefCount{ 1 };
	};

} // namespace UE::PixelStreaming2