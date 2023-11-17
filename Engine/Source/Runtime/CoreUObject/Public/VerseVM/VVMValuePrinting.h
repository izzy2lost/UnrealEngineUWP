// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Containers/StringFwd.h"
#include "HAL/Platform.h"

class FString;

namespace Verse
{
struct VInt;
struct VCell;
struct VValue;
struct VRestValue;
struct FAllocationContext;

struct FCellFormatter
{
	virtual ~FCellFormatter() {}

	// Format the cell into a string. Where possible, use a string builder and the Append method below
	virtual FString ToString(FAllocationContext Context, VCell& Cell) const = 0;

	// Format the cell into an existing string builder.
	virtual void Append(FStringBuilderBase& Builder, FAllocationContext Context, VCell& Cell) const = 0;
};

struct FDefaultCellFormatter : FCellFormatter
{
	// FCellFormatter implementation
	COREUOBJECT_API virtual FString ToString(FAllocationContext Context, VCell& Cell) const override;
	COREUOBJECT_API virtual void Append(FStringBuilderBase& Builder, FAllocationContext Context, VCell& Cell) const override;

protected:
	// This helper method appends the cell to the string builder but without any of the debugging address text.
	// This allows such things as unit tests to override Append to provide stable strings to compare.
	COREUOBJECT_API virtual bool TryAppend(FStringBuilderBase& Builder, FAllocationContext Context, VCell& Cell) const;
};

COREUOBJECT_API FString ToString(const VInt& Int);
COREUOBJECT_API FString ToString(FAllocationContext Context, const FCellFormatter& Formatter, const VValue& Value);
COREUOBJECT_API FString ToString(FAllocationContext Context, const FCellFormatter& Formatter, const VRestValue& Value);
COREUOBJECT_API void ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter, const VValue& Value);
COREUOBJECT_API void ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter, const VRestValue& Value);

} // namespace Verse
