// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "HAL/Platform.h"

class FString;

namespace Verse
{
struct VInt;
struct VCell;
struct VConstructor;
struct VUniqueString;
struct VUniqueStringSet;
struct VValue;
struct VRestValue;
struct FAllocationContext;
enum class EFieldType : int8;

struct FCellFormatter
{
	virtual ~FCellFormatter() {}
	virtual FString ToString(FAllocationContext, VCell& Cell) const = 0;
};

struct FDefaultCellFormatter : FCellFormatter
{
	COREUOBJECT_API virtual FString ToString(FAllocationContext, VCell& Cell) const;
};

FString ToString(const EFieldType FieldType);
COREUOBJECT_API FString ToString(const VInt& Int);
COREUOBJECT_API FString ToString(double Double);
COREUOBJECT_API FString ToString(FAllocationContext, const VValue& Value, const FCellFormatter& CellFormatter = FDefaultCellFormatter{});
COREUOBJECT_API FString ToString(FAllocationContext Context, const VUniqueString& String);
COREUOBJECT_API FString ToString(FAllocationContext Context, const VUniqueStringSet& String);
COREUOBJECT_API FString ToString(FAllocationContext Context, const VConstructor& Constructor, const FCellFormatter& CellFormatter = FDefaultCellFormatter{});
FString ToString(FAllocationContext, const VRestValue& Value, const FCellFormatter& CellFormatter = FDefaultCellFormatter{});

} // namespace Verse
