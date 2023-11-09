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
	virtual void VisitNonNull(VCell* InCell);
	virtual void VisitNonNull(UObject* InObject);

	// This method is only invoked by VCell to visit the emergent type of the cell.  It should not be
	// called in any other situtation.
	virtual void VisitEmergentType(const VCell* InEmergentType);

	// The default implementation of the following methods just check for null values and then forward to the non-null variants
	virtual void Visit(VCell* InCell);
	virtual void Visit(UObject* InObject);

	// The default implementation looks for either a VCell or UObject pointer and invokes the proper Visit method if found
	virtual void Visit(VValue Value);

	// The default implementation forwards the call to the VRestValue::Visit method
	virtual void Visit(VRestValue& Value);

	template <typename T>
	FORCEINLINE void Visit(TWriteBarrier<T>& Value)
	{
		Visit(Value.Get());
	}

	template <typename T>
	FORCEINLINE void Visit(T Begin, T End)
	{
		for (; Begin != End; ++Begin)
		{
			Visit(*Begin);
		}
	}

	template <typename T>
	FORCEINLINE void Visit(T* Values, uint32 Count)
	{
		Visit(Values, Values + Count);
	}

protected:
	FAbstractVisitor() = default;

private:
	FReferrerContext* Context{nullptr};
};

} // namespace Verse
