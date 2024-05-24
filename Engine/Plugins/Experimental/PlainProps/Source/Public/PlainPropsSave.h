// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlainPropsTypes.h"
#include "Templates/UniquePtr.h"

namespace PlainProps 
{

struct FBuiltStruct;
class FCustomBindings;
class FDeclarations;
class FSchemaBindings;

struct FSaveContext
{
	const FDeclarations&		Declarations;
	const FSchemaBindings&		Schemas;
	FCustomBindings&			Customs;
};

[[nodiscard]] TUniquePtr<FBuiltStruct> SaveStruct(const void* Struct, FStructSchemaId Id, const FSaveContext& Context);
[[nodiscard]] TUniquePtr<FBuiltStruct> SaveStructDelta(const void* Struct, const void* Default, FStructSchemaId Id, const FSaveContext& Context);

} // namespace PlainProps

