// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include  "PlainPropsBuild.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"

namespace PlainProps
{

struct FMemberSchema;

struct FBuiltStructSchema
{
	FTypeId Type;
	bool bUsed = false;
	bool bDense = false;
	FOptionalStructSchemaId Super;
	TArray<FMemberId> MemberNames;
	TArray<const FMemberSchema*> MemberSchemas;
};

struct FBuiltEnumSchema
{
	FTypeId Type;
	bool bUsed = false;
	EEnumMode Mode = EEnumMode::Flat;
	ELeafWidth Width = ELeafWidth::B8;
	TArray<FNameId> Names;
	TArray<uint64> Constants;
};

struct FBuiltSchemas
{
	TArray<FBuiltStructSchema> Structs; // Same size as number of declared structs
	TArray<FBuiltEnumSchema> Enums; // Same size as number of declared enums
};

class FSchemasBuilder;

struct FStructSchemaBuilder
{
	const FStructDeclaration&		Declaration;
	FSchemasBuilder&				AllSchemas;
	const FDebugIds&				Debug;
	TMap<FOptionalMemberId, FMemberSchema>	NotedMembers;
	bool							bMissingMemberNoted = false;
	mutable bool					bBuilt = false;

	void							NoteMembersRecursively(const FBuiltStruct& Struct);
	void							NoteRangeRecursively(ERangeSizeType NumType, TConstArrayView<FMemberType> Types, FSchemaId InnermostSchema, const FBuiltRange& Range);
	FBuiltStructSchema				Build() const;
};

struct FEnumSchemaBuilder
{
	const FEnumDeclaration&			Declaration;
	TSet<uint64>					NotedConstants;
	mutable bool					bBuilt = false;

	void							NoteValue(uint64 Value);
	FBuiltEnumSchema				Build() const;
};

class FSchemasBuilder
{
public:
	FSchemasBuilder(TConstArrayView<TUniquePtr<FStructDeclaration>> InStructs, TConstArrayView<TUniquePtr<FEnumDeclaration>> InEnums, const FDebugIds& InDebug);

	void NoteMembers(FStructSchemaId Schema, const FBuiltStruct& Struct)
	{
		Structs[Schema.Idx].NoteMembersRecursively(Struct);
	}

	void NoteValue(FEnumSchemaId Schema, uint64 Value)
	{
		Enums[Schema.Idx].NoteValue(Value);
	}
	
	FBuiltSchemas Build() const;

private:
	TArray<FStructSchemaBuilder> Structs;
	TArray<FEnumSchemaBuilder> Enums;
	const FDebugIds& Debug;
};

} // namespace PlainProps