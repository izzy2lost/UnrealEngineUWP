// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Templates/TypeHash.h"
#include "VerseVM/VVMUTF8String.h"

namespace Verse
{
template <typename T>
inline FUtf8StringView FUniqueStringSetKeyFuncsBase<T>::GetSetKey(VUniqueString& Element)
{
	return Element.AsStringView();
}

template <typename T>
inline FUtf8StringView FUniqueStringSetKeyFuncsBase<T>::GetSetKey(const T& Element)
{
	if (const VUniqueString* String = Element.Get())
	{
		return String->AsStringView();
	}
	else
	{
		return FUtf8StringView();
	}
}

template <typename T>
inline bool FUniqueStringSetKeyFuncsBase<T>::Matches(FUtf8StringView A, FUtf8StringView B)
{
	return A.Equals(B, ESearchCase::CaseSensitive);
}

template <typename T>
inline uint32 FUniqueStringSetKeyFuncsBase<T>::GetKeyHash(FUtf8StringView Key)
{
	return GetTypeHash(Key);
}

template struct FUniqueStringSetKeyFuncsBase<TWeakBarrier<VUniqueString>>;
template struct FUniqueStringSetKeyFuncsBase<TWriteBarrier<VUniqueString>>;

inline VUniqueStringSet::FConstIterator::FConstIterator(SetType::TRangedForConstIterator InCurrentIteration)
	: CurrentIteration(InCurrentIteration) {}

inline const TWriteBarrier<VUniqueString>& VUniqueStringSet::FConstIterator::operator*() const
{
	return *CurrentIteration;
}

inline bool VUniqueStringSet::FConstIterator::operator==(const FConstIterator& Rhs) const
{
	return CurrentIteration == Rhs.CurrentIteration;
}

inline bool VUniqueStringSet::FConstIterator::operator!=(const FConstIterator& Rhs) const
{
	return CurrentIteration != Rhs.CurrentIteration;
}

inline VUniqueStringSet::FConstIterator& VUniqueStringSet::FConstIterator::operator++()
{
	++CurrentIteration;
	return *this;
}

inline VUniqueStringSet::FConstIterator VUniqueStringSet::begin() const
{
	return Strings.begin();
}

inline VUniqueStringSet::FConstIterator VUniqueStringSet::end() const
{
	return Strings.end();
}

inline VUniqueStringSet& VUniqueStringSet::New(FAllocationContext Context, TSet<VUniqueString*> InStrings)
{
	return Pool->Intern(Context, InStrings);
}

inline VUniqueStringSet& VUniqueStringSet::New(FAllocationContext Context, const std::initializer_list<FUtf8StringView>& InStrings)
{
	TSet<VUniqueString*> StringSet;
	StringSet.Reserve(InStrings.size());
	for (const FUtf8StringView& String : InStrings)
	{
		VUniqueString& UniqueString = VUniqueString::New(Context, String);
		StringSet.Add(&UniqueString);
	}
	return Pool->Intern(Context, StringSet);
}

inline VUniqueStringSet& VUniqueStringSet::Make(FAllocationContext Context, const TSet<VUniqueString*>& InSet)
{
	// All of the string sets' memory is being managed externally via `TSet` so this is OK.
	const size_t NumBytes = sizeof(VUniqueStringSet);
	return *new (Context.Allocate(FHeap::DestructorSpace, NumBytes)) VUniqueStringSet(Context, InSet);
}

inline bool VUniqueStringSet::operator==(const VUniqueStringSet& Other) const
{
	// We can compare by pointer address, because two unique string sets that are the same should have been vended the same way.
	return this == &Other;
}

inline uint32 VUniqueStringSet::Num() const
{
	return Strings.Num();
}

inline FSetElementId VUniqueStringSet::FindId(const FUtf8StringView& String) const
{
	return Strings.FindId(String);
}

inline bool VUniqueStringSet::IsValidId(const FSetElementId& Id) const
{
	return Strings.IsValidId(Id);
}

inline VUniqueStringSet::SetType VUniqueStringSet::FormSet(FAllocationContext Context, const TSet<VUniqueString*>& InSet)
{
	VUniqueStringSet::SetType Strings;
	Strings.Reserve(InSet.Num());
	for (VUniqueString* InString : InSet)
	{
		TWriteBarrier<VUniqueString> Test{Context, InString};
		Strings.Add({Context, *InString});
	}
	return Strings;
}

inline VUniqueStringSet::VUniqueStringSet(FAllocationContext Context, const TSet<VUniqueString*>& InSet)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	, Strings(FormSet(Context, InSet))
{
}

inline void VUniqueStringSet::RunDestructorImpl(VCell* This)
{
	VUniqueStringSet& ThisSet = This->StaticCast<VUniqueStringSet>();
	ThisSet.~VUniqueStringSet();
}

inline void VUniqueStringSet::MarkReferencedCellsImpl(VCell* ThisCell, FMarkStack& MarkStack)
{
	// We still have to mark each of the strings in the set as being used.
	VCell::MarkReferencedCellsImpl(ThisCell, MarkStack);
	VUniqueStringSet& This = ThisCell->StaticCast<VUniqueStringSet>();
	for (TWriteBarrier<VUniqueString>& UniqueString : This.Strings)
	{
		UniqueString.Mark(MarkStack);
	}
}

inline bool VUniqueStringSet::Equals(const TSet<VUniqueString*>& A, const TSet<VUniqueString*>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}
	return GetTypeHash(A) == GetTypeHash(B);
}

inline uint32 GetTypeHash(const TSet<VUniqueString*>& Set)
{
	// Each of the strings in the set should already be pointing at a globally-unique string; thus we should be
	// able to reliably hash just the pointers of each string, rather than having to hash the contents of each
	// specific string.
	// TODO: (yiliang.siew) Is there potentially a better commutative hash function than just `XOR`?
	uint32 Result = 0;
	for (const VUniqueString* const Element : Set)
	{
		Result ^= PointerHash(Element);
	}
	return Result;
}

inline uint32 GetTypeHash(const VUniqueStringSet& Set)
{
	// Each of the strings in the set should already be pointing at a globally-unique string; thus we should be
	// able to reliably hash just the pointers of each string, rather than having to hash the contents of each
	// specific string.
	uint32 Result = 0;
	for (const TWriteBarrier<VUniqueString>& Element : Set)
	{
		Result ^= PointerHash(Element.Get());
	}
	return Result;
}

inline FHashableUniqueStringSetKeyFuncs::KeyInitType FHashableUniqueStringSetKeyFuncs::GetSetKey(const TWeakBarrier<VUniqueStringSet>& Element)
{
	TSet<VUniqueString*> Result;
	Result.Reserve(Element->Strings.Num());
	for (const TWriteBarrier<VUniqueString>& String : Element->Strings)
	{
		Result.Add(String.Get());
	}
	return Result;
}

inline bool FHashableUniqueStringSetKeyFuncs::Matches(FHashableUniqueStringSetKeyFuncs::KeyInitType A, FHashableUniqueStringSetKeyFuncs::KeyInitType B)
{
	return VUniqueStringSet::Equals(A, B);
}

inline uint32 FHashableUniqueStringSetKeyFuncs::GetKeyHash(KeyInitType Key)
{
	return GetTypeHash(Key);
}

} // namespace Verse
