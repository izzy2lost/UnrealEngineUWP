// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMGlobalHeapRoot.h"
#include "VVMLazyInitialized.h"
#include "VVMMarkStack.h"
#include "VVMWriteBarrier.h"

namespace Verse
{

template <typename T>
struct TGlobalHeapPtrImpl : FGlobalHeapRoot
	, TWriteBarrier<T>
{
	TGlobalHeapPtrImpl() = default;

	TGlobalHeapPtrImpl(FAccessContext Context, typename TWriteBarrier<T>::TValue Value)
		: FGlobalHeapRoot()
		, TWriteBarrier<T>(Context, Value)
	{
	}

	void MarkReferencedCells(FMarkStack& MarkStack) override
	{
		TWriteBarrier<T>::Mark(MarkStack);
	}
};

template <typename T>
struct TGlobalHeapPtr
{
	static constexpr bool bIsVValue = std::is_same_v<T, VValue>;
	using TValue = typename std::conditional<bIsVValue, VValue, T*>::type;

	TGlobalHeapPtr() = default;

	TGlobalHeapPtr(const TGlobalHeapPtr&) = delete;
	TGlobalHeapPtr& operator=(const TGlobalHeapPtr&) = delete;
	TGlobalHeapPtr(TGlobalHeapPtr&&) = delete;
	TGlobalHeapPtr& operator=(TGlobalHeapPtr&&) = delete;

	void Set(FAccessContext Context, TValue NewValue)
	{
		Impl->Set(Context, NewValue);
	}

	template <typename TResult = void>
	std::enable_if_t<bIsVValue, TResult> SetNonCellNorPlaceholder(VValue NewValue)
	{
		Impl->SetNonCellNorPlaceholder(NewValue);
	}

	TValue Get() const { return Impl->Get(); }

	// nb: operators "*" and "->" disabled for TGlobalHeapPtr<VValue>;
	//     use Get() + VValue member functions to check/access boxed values

	template <typename TResult = TValue>
	std::enable_if_t<!bIsVValue, TResult> operator->() const { return Impl->Get(); }

	template <typename TResult = T>
	std::enable_if_t<!bIsVValue, TResult&> operator*() const { return *Impl->Get(); }

	explicit operator bool() const { return !!Impl->Get(); }

private:
	TLazyInitialized<TGlobalHeapPtrImpl<T>> Impl;
};

} // namespace Verse
