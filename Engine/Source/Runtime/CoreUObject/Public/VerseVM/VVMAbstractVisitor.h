// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "VVMValue.h"
#include "VVMWriteBarrier.h"

class UObject;

namespace Verse
{
struct VCell;
struct VRestValue;

struct FAbstractVisitor
{
	UE_NONCOPYABLE(FAbstractVisitor);

	enum class EReferrerType
	{
		Cell,
		UObject,
	};

	// The referrer token represents the cell or object that is currently being visited
	struct FReferrerToken
	{
		FReferrerToken(VCell* Cell);
		FReferrerToken(UObject* Object);

		EReferrerType GetType() const;
		bool IsCell() const;
		VCell* AsCell() const;
		bool IsUObject() const;
		UObject* AsUObject() const;

	private:
		static constexpr uint64 EncodingBits = 0b1;
		uint64 EncodedBits{0};
	};

	// A stack based context to maintain the chain of referrers
	struct FReferrerContext
	{
		FReferrerContext(FAbstractVisitor& InVisitor, FReferrerToken InReferrer);
		~FReferrerContext();

		FReferrerToken GetReferrer() const { return Referrer; }

	private:
		FAbstractVisitor& Visitor;
		FReferrerToken Referrer;
		FReferrerContext* Previous;
	};

	virtual ~FAbstractVisitor() = default;

	// The context provides information about the current cell being visited
	FReferrerContext* GetContext() const
	{
		return Context;
	}

	// Override the following methods to constomize how different values will be processed.  For visitors that just need
	// to enumerate VCell and UObject references, these are the only methods that need to be overridden
	virtual void VisitNonNull(VCell* InCell, const char* ElementName);
	virtual void VisitNonNull(UObject* InObject, const char* ElementName);

	// This method is only invoked by VCell to visit the emergent type of the cell.  It should not be
	// called in any other situtation.
	virtual void VisitEmergentType(const VCell* InEmergentType);

	// Override the following methods to handle nesting of elements.  Begin/EndObject are intended for when
	// objects are elements in arrays.
	virtual void BeginArray(const char* ElementName);
	virtual void EndArray();
	virtual void BeginSet(const char* ElementName);
	virtual void EndSet();
	virtual void BeginMap(const char* ElementName);
	virtual void EndMap();
	virtual void BeginObject();
	virtual void EndObject();

	virtual bool IsMarked(VCell* InCell, const char* ElementName) { return true; }

	// The default implementation of the following methods just check for null values and then forward to the non-null variants
	virtual void Visit(VCell* InCell, const char* ElementName);
	virtual void Visit(UObject* InObject, const char* ElementName);

	// The default implementation looks for either a VCell or UObject pointer and invokes the proper Visit method if found
	virtual void Visit(VValue Value, const char* ElementName);

	// The default implementation forwards the call to the VRestValue::Visit method
	virtual void Visit(VRestValue& Value, const char* ElementName);

	template <typename T>
	FORCEINLINE void Visit(TWriteBarrier<T>& Value, const char* ElementName)
	{
		Visit(Value.Get(), ElementName);
	}

	// Simple arrays
	template <typename T>
	FORCEINLINE void Visit(T Begin, T End, const char* ElementName)
	{
		BeginArray(ElementName);
		for (; Begin != End; ++Begin)
		{
			Visit(*Begin, ElementName);
		}
		EndArray();
	}

	template <typename T>
	FORCEINLINE void Visit(T* Values, uint32 Count, const char* ElementName)
	{
		Visit(Values, Values + Count, ElementName);
	}

	template <typename T>
	FORCEINLINE void Visit(T* Values, uint64 Count, const char* ElementName)
	{
		Visit(Values, Values + Count, ElementName);
	}

	template <typename T>
	FORCEINLINE void Visit(const TArray<T>& Values, const char* ElementName)
	{
		Visit(Values.begin(), Values.end(), ElementName);
	}

	// Sets
	template <typename T>
	FORCEINLINE void Visit(const TSet<T>& Values, const char* ElementName)
	{
		BeginSet(ElementName);
		for (const auto& Value : Values)
		{
			Visit(Value, ElementName);
		}
		EndSet();
	}

	// Maps
	template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
	FORCEINLINE void Visit(TMap<KeyType, ValueType, SetAllocator, KeyFuncs>& Values, const char* ElementName)
	{
		BeginMap(ElementName);
		for (auto& Kvp : Values)
		{
			BeginObject();
			Visit(Kvp.Key, "Key");
			Visit(Kvp.Value, "Value");
			EndObject();
		}
		EndMap();
	}

	virtual void ReportNativeBytes(size_t Bytes) {}

protected:
	FAbstractVisitor() = default;

private:
	FReferrerContext* Context{nullptr};
};

} // namespace Verse
