// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Containers/Set.h"
#include "Containers/StringView.h"
#include "Templates/UnrealTemplate.h"
#include "VVMCell.h"
#include "VVMContext.h"
#include "VVMGlobalHeapCensusRoot.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMWeakBarrier.h"

namespace Verse
{
struct VUTF8String;
struct VUniqueString;

struct FUniqueStringSetKeyFuncs : BaseKeyFuncs<TWeakBarrier<VUniqueString>, TWeakBarrier<VUniqueString>, false>
{
	typedef FUtf8StringView KeyInitType;
	typedef VUniqueString& ElementInitType;

	static FUtf8StringView GetSetKey(VUniqueString& Element);
	static FUtf8StringView GetSetKey(const TWeakBarrier<VUniqueString>& Element);

	static FORCEINLINE bool Matches(FUtf8StringView A, FUtf8StringView B)
	{
		return A.Equals(B, ESearchCase::CaseSensitive);
	}

	static FORCEINLINE uint32 GetKeyHash(FUtf8StringView Key)
	{
		return GetTypeHash(Key);
	}
};

class VStringInternPool final : FGlobalHeapCensusRoot
{
private:
	/// Private constructor since there should only ever be one global instance of this.
	/// There's no virtual destructor since `TLazyInitialized` is never destroyed and this is meant to be a global string pool.
	VStringInternPool() = default;

	VUniqueString& Intern(FAllocationContext Context, FUtf8StringView String);

	/// This gives the string intern pool the ability to conduct census on its own to clear references to the strings.
	virtual void ConductCensus() override;

	// The pool doesn't own the string data, the context does. So these strings are stored as weakrefs.
	// When the GC conducts a census, this string pool is also cleared of the strings that are marked.
	TSet<TWeakBarrier<VUniqueString>, FUniqueStringSetKeyFuncs> UniqueStrings;
	static UE::FMutex Mutex;

	friend struct VUniqueString;
	friend struct TLazyInitialized<VStringInternPool>;
};

/// Representation of a UTF-8 string in the Verse compiler.
struct VUTF8String : VHeapValue
{
	using SizeType = uint32;

	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static VUTF8String& New(FAllocationContext Context, const SizeType NumUTF8CHARs)
	{
		const size_t NumBytes = AllocationSize(NumUTF8CHARs);
		return *new (Context.AllocateFastCell(NumBytes)) VUTF8String(Context, NumUTF8CHARs);
	}

	/**
	 * Creates a new string. Use this if you do not require your string to be unique-able.
	 * (i.e. a one-off string not used for repeated property field lookups or some other string literal.)
	 */
	static VUTF8String& New(FAllocationContext Context, FUtf8StringView String)
	{
		const size_t NumBytes = AllocationSize(String.Len());
		return *new (Context.AllocateFastCell(NumBytes)) VUTF8String(Context, String);
	}

	/**
	 * Creates a new string from the concatenation of two input strings.
	 */
	static VUTF8String& Concat(FAllocationContext Context, VUTF8String& Left, VUTF8String& Right)
	{
		const SizeType NumUTF8CHARs = Left.Num() + Right.Num();
		const size_t NumBytes = AllocationSize(NumUTF8CHARs);
		VUTF8String& NewString = *new (Context.AllocateFastCell(NumBytes)) VUTF8String(Context, NumUTF8CHARs);
		checkSlow(NewString.Data() && Left.Data() && Right.Data());
		memcpy(NewString.Data(), Left.Data(), Left.Num());
		memcpy(NewString.Data() + Left.Num(), Right.Data(), Right.Num());
		NewString.Data()[NumUTF8CHARs] = static_cast<UTF8CHAR>(0);
		return NewString;
	}

	SizeType Num() const
	{
		return NumUTF8CHARs;
	}

	bool Equals(FUtf8StringView String) const
	{
		return AsStringView().Equals(String, ESearchCase::CaseSensitive);
	}

	bool operator==(const VUTF8String& Other) const
	{
		return Equals(Other.AsStringView());
	}

	const char* AsCString() const
	{
		return reinterpret_cast<const char*>(Data());
	}

	FUtf8StringView AsStringView() const
	{
		return FUtf8StringView(Data(), IntCastChecked<int32>(NumUTF8CHARs));
	}

	COREUOBJECT_API static uint32 GetTypeHashImpl(VCell* ThisCell);

private:
	static size_t DataOffset()
	{
		return Align(sizeof(VUTF8String), alignof(UTF8CHAR));
	}

	static size_t AllocationSize(const SizeType NumUTF8CHARs)
	{
		return DataOffset() + ((NumUTF8CHARs + 1) * sizeof(UTF8CHAR)); // Additional space for null terminator.
	}

	VUTF8String(FAllocationContext Context, const SizeType InNumUTF8CHARs)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumUTF8CHARs(InNumUTF8CHARs)
	{
	}

	VUTF8String(FAllocationContext Context, FUtf8StringView String)
		: VUTF8String(Context, String.Len())
	{
		if (String.Len())
		{
			memcpy(Data(), String.GetData(), String.Len());
		}
		Data()[String.Len()] = static_cast<UTF8CHAR>(0);
	}

	UTF8CHAR* Data() const
	{
		return BitCast<UTF8CHAR*>(BitCast<char*>(this) + DataOffset());
	}

	const SizeType NumUTF8CHARs;

	friend class VStringInternPool;
	friend struct VUniqueString;
};

/// A unique string that lives in the global string intern pool.
struct VUniqueString final : VUTF8String
{
	COREUOBJECT_API static VCppClassInfo StaticCppClassInfo;
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	/**
	 * Creates a new unique string if it does not already exist.
	 * Subsequent calls to this function with the same string will return a reference to the string that already exists in the string intern pool.
	 */
	static VUniqueString& New(FAllocationContext Context, FUtf8StringView String)
	{
		return StringPool->Intern(Context, String);
	}

	bool operator==(const VUniqueString& Other) const
	{
		return this == &Other;
	}

private:
	static VUniqueString& Make(FAllocationContext Context, FUtf8StringView String)
	{
		const size_t NumBytes = AllocationSize(String.Len());
		return *new (Context.AllocateFastCell(NumBytes)) VUniqueString(Context, String);
	}

	VUniqueString(FAllocationContext Context, FUtf8StringView String)
		: VUTF8String(Context, String)
	{
	}

	/// Global unique string table. This has to be wrapped in a `TLazyInitialized` so that the Verse heap is first
	/// initialized before this attempts to be initialized.
	static TLazyInitialized<VStringInternPool> StringPool;
	friend class VStringInternPool;
};

inline FUtf8StringView FUniqueStringSetKeyFuncs::GetSetKey(VUniqueString& Element)
{
	return Element.AsStringView();
}

inline FUtf8StringView FUniqueStringSetKeyFuncs::GetSetKey(const TWeakBarrier<VUniqueString>& Element)
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

/// Allows for `VUniqueString` to be used with Unreal hashtable containers like `TMap`/`TSet`.
inline uint32 GetTypeHash(const VUniqueString& String)
{
	return PointerHash(&String);
}

/// Allows for `VUTF8String` to be used with Unreal hashtable containers like `TMap`/`TSet`.
inline uint32 GetTypeHash(const VUTF8String& String)
{
	return GetTypeHash(String.AsStringView());
}
} // namespace Verse
