// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "VVMCell.h"
#include "VVMContext.h"
#include "VVMTypeTag.h"
#include <new>

namespace Verse
{
struct VCppClassInfo;

template <typename T>
struct TGlobalHeapPtr;

// Represents Verse types, which may be independent of object shape, and independent of C++ type.
struct VType : VCell
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;

	EVerseTypeTag Tag;

	FString DebugName() const
	{
		return Verse::DebugName(Tag);
	}

	template <typename CastType>
	bool IsA() const
	{
		static_assert(std::is_base_of_v<VType, CastType>);
		return Tag == CastType::Tag;
	}

	template <typename CastType>
	const CastType& StaticCast() const
	{
		checkf(IsA<CastType>(),
			TEXT("Expected type %s, but got type %s."),
			*Verse::DebugName(CastType::Tag),
			*DebugName());
		return *static_cast<const CastType*>(this);
	}

	template <typename CastType>
	CastType& StaticCast()
	{
		checkf(IsA<CastType>(),
			TEXT("Expected type %s, but got type %s."),
			*Verse::DebugName(CastType::Type::Tag),
			*DebugName());
		return *static_cast<CastType*>(this);
	}

	template <typename CastType>
	CastType* DynamicCast()
	{
		return IsA<CastType>() ? &StaticCast<CastType>() : nullptr;
	}

protected:
	COREUOBJECT_API explicit VType(FAllocationContext Context, EVerseTypeTag T);
};

struct VTrivialType : VType
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;

	static constexpr EVerseTypeTag Tag = EVerseTypeTag::Trivial;

	COREUOBJECT_API static TGlobalHeapPtr<VTrivialType> Singleton;

	static bool Equals(const VType& Type)
	{
		return Type.IsA<VTrivialType>();
	}

	static void Initialize(FAllocationContext Context);

private:
	VTrivialType(FAllocationContext Context)
		: VType(Context, Tag)
	{
	}
};

} // namespace Verse
