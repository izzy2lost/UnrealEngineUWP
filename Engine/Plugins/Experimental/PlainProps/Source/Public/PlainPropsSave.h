// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlainPropsTypes.h"
#include "PlainPropsBuild.h"
#include "Templates/UniquePtr.h"

namespace PlainProps 
{

class FCustomBindings;
class FDeclarations;
class FSchemaBindings;

// Temporary data structure, will be replaced by something more sophisticated
// perhaps deduplicating all zero-memory defaults
struct FDefaultStruct { FStructSchemaId Id; const void* Struct; };
using FDefaultStructs = TConstArrayView<FDefaultStruct>;

struct FSaveContext
{
	const FDeclarations&		Declarations;
	const FSchemaBindings&		Schemas;
	FCustomBindings&			Customs;
	FScratchAllocator&			Scratch;
	FDefaultStructs				Defaults;
};

template<typename Runtime>
FSaveContext MakeSaveContext(FDefaultStructs Defaults, FScratchAllocator& Scratch)
{
	return { Runtime::GetTypes(), Runtime::GetSchemas(), Runtime::GetCustoms(), Scratch, Defaults };
}

[[nodiscard]] PLAINPROPS_API FBuiltStructPtr SaveStruct(const void* Struct, FStructSchemaId Id, const FSaveContext& Context);
[[nodiscard]] PLAINPROPS_API FBuiltStructPtr SaveStructDelta(const void* Struct, const void* Default, FStructSchemaId Id, const FSaveContext& Context);

} // namespace PlainProps