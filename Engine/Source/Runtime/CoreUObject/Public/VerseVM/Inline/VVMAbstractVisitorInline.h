// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Templates/TypeCompatibleBytes.h"
#include "VerseVM/VVMAbstractVisitor.h"

class UObject;

namespace Verse
{

inline FAbstractVisitorDispatch::FReferrerToken::FReferrerToken(VCell* Cell)
	: EncodedBits(BitCast<uint64>(Cell) | static_cast<uint64>(EReferrerType::Cell))
{
}

inline FAbstractVisitorDispatch::FReferrerToken::FReferrerToken(UObject* Object)
	: EncodedBits(BitCast<uint64>(Object) | static_cast<uint64>(EReferrerType::UObject))
{
}

inline FAbstractVisitorDispatch::EReferrerType FAbstractVisitorDispatch::FReferrerToken::GetType() const
{
	return static_cast<EReferrerType>((EncodedBits & EncodingBits));
}

inline bool FAbstractVisitorDispatch::FReferrerToken::IsCell() const
{
	return GetType() == EReferrerType::Cell;
}

inline VCell* FAbstractVisitorDispatch::FReferrerToken::AsCell() const
{
	checkSlow(IsCell());
	return BitCast<VCell*>(EncodedBits & ~EncodingBits);
}

inline bool FAbstractVisitorDispatch::FReferrerToken::IsUObject() const
{
	return GetType() == EReferrerType::UObject;
}

inline UObject* FAbstractVisitorDispatch::FReferrerToken::AsUObject() const
{
	checkSlow(IsUObject());
	return BitCast<UObject*>(EncodedBits & ~EncodingBits);
}

inline FAbstractVisitorDispatch::FReferrerContext::FReferrerContext(FAbstractVisitorDispatch& InVisitor, FReferrerToken InReferrer)
	: Visitor(InVisitor)
	, Referrer(InReferrer)
{
	Previous = Visitor.Context;
	Visitor.Context = this;
}

inline FAbstractVisitorDispatch::FReferrerContext::~FReferrerContext()
{
	Visitor.Context = Previous;
}

} // namespace Verse
