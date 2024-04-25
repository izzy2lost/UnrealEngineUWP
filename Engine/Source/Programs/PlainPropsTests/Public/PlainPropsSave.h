// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include  "PlainPropsTypes.h"
#include "Templates/UniquePtr.h"

namespace PlainProps 
{

struct FBuiltStruct;
class FDeclarations;
class FStructBindings;

struct FSaveContext
{
	const FDeclarations& Declarations;
	const FStructBindings& Bindings;
	const FDebugIds& Debug;
};

[[nodiscard]] TUniquePtr<FBuiltStruct> SaveStruct(const uint8* Struct, FStructSchemaId Id, const FSaveContext& Context);

} // namespace PlainProps

