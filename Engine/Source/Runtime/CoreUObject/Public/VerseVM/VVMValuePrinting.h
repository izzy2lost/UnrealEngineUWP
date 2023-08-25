// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "HAL/Platform.h"

class FString;

namespace Verse
{
struct VInt;
struct VCell;
struct VValue;
struct VRestValue;
struct FRunningContext;

struct FCellFormatter
{
	virtual ~FCellFormatter() {}
	virtual FString ToString(FRunningContext, VCell& Cell) const = 0;
};

struct FDefaultCellFormatter : FCellFormatter
{
	COREUOBJECT_API virtual FString ToString(FRunningContext, VCell& Cell) const;
};

COREUOBJECT_API FString ToString(const VInt& Int);
COREUOBJECT_API FString ToString(double Double);
COREUOBJECT_API FString ToString(FRunningContext, const VValue& Value, const FCellFormatter& CellFormatter = FDefaultCellFormatter{});
FString ToString(FRunningContext, const VRestValue& Value, const FCellFormatter& CellFormatter = FDefaultCellFormatter{});
} // namespace Verse
