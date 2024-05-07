// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include  "PlainPropsBuild.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/PagedArray.h"

namespace PlainProps
{

struct FMemberSchema;
struct FEnumSchemaBuilder;
struct FStructSchemaBuilder;

struct FBuiltStructSchema
{
	FTypeId							Type;
	FStructSchemaId					Id;
	FOptionalStructSchemaId			Super;
	bool							bDense = false;
	TArray<FMemberId>				MemberNames;
	TArray<const FMemberSchema*>	MemberSchemas;
};

struct FBuiltEnumSchema
{
	FTypeId							Type;
	FEnumSchemaId					Id;
	EEnumMode						Mode = EEnumMode::Flat;
	ELeafWidth						Width = ELeafWidth::B8;
	TArray<FNameId>					Names;
	TArray<uint64>					Constants;
};

struct FBuiltSchemas
{
	TArray<FBuiltStructSchema>		Structs; // Same size as number of declared structs
	TArray<FBuiltEnumSchema>		Enums; // Same size as number of declared enums
};

class FSchemasBuilder
{
public:
	using FStructDeclarations = TConstArrayView<TUniquePtr<FStructDeclaration>>;
	using FEnumDeclarations = TConstArrayView<TUniquePtr<FEnumDeclaration>>;

	FSchemasBuilder(FStructDeclarations InStructs, FEnumDeclarations InEnums, const FDebugIds& InDebug);
	~FSchemasBuilder();

	void										NoteMembers(FStructSchemaId Id, const FBuiltStruct& Struct);
	void										NoteValue(FEnumSchemaId Id, uint64 Value);
	FBuiltSchemas								Build();

private:
	FStructDeclarations							DeclaredStructs;
	FEnumDeclarations							DeclaredEnums;
	TArray<int32>								StructIndices;
	TArray<int32>								EnumIndices;
	TPagedArray<FStructSchemaBuilder, 4096>		Structs;		// TPagedArray for stable references
	TPagedArray<FEnumSchemaBuilder, 4096>		Enums;			// TPagedArray for stable references
	const FDebugIds&							Debug;
	bool										bBuilt = false;

	void										NoteInheritanceChains();
};

} // namespace PlainProps