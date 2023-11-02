// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "VVMVisitorWrapper.h"

class UObject;

namespace Verse
{
struct VCell;

struct FAbstractVisitorDispatch
{
	UE_NONCOPYABLE(FAbstractVisitorDispatch);

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
		FReferrerContext(FAbstractVisitorDispatch& InVisitor, FReferrerToken InReferrer);
		~FReferrerContext();

		FReferrerToken GetReferrer() const { return Referrer; }

	private:
		FAbstractVisitorDispatch& Visitor;
		FReferrerToken Referrer;
		FReferrerContext* Previous;
	};

	virtual ~FAbstractVisitorDispatch() = default;

	FReferrerContext* GetContext() const
	{
		return Context;
	}

	// Override the following methods to constomize how different values will be processed
	virtual void VisitNonNull(const VCell* InCell) {}
	virtual void VisitNonNull(const UObject* InObject) {}

protected:
	FAbstractVisitorDispatch() = default;

private:
	FReferrerContext* Context{nullptr};
};

using FAbstractVisitor = TVisitorWrapper<FAbstractVisitorDispatch>;

} // namespace Verse
