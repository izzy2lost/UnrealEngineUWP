// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include  "PlainPropsTypes.h"

namespace PlainProps
{

struct FSchemaBatch
{
	uint32 NumNestedScopes;
	uint32 NestedScopesOffset;
	uint32 NumParametricTypes;
	uint32 NumSchemas;
	uint32 SchemaOffsets[0];

	TConstArrayView<FNestedScope> GetNestedScopes() const
	{
		return MakeArrayView(reinterpret_cast<const FNestedScope*>(reinterpret_cast<const uint8*>(this) + NestedScopesOffset), NumNestedScopes);
	}

	TConstArrayView<FParametricType> GetParametricTypes() const
	{
		return MakeArrayView(reinterpret_cast<const FParametricType*>(GetNestedScopes().GetData() + NumNestedScopes), NumParametricTypes);
	}

	const FTypeId* GetFirstParameter() const
	{
		return reinterpret_cast<const FTypeId*>(GetParametricTypes().GetData() + NumParametricTypes);
	}

	void ValidateBounds(uint64 NumBytes) const;
};

static_assert(sizeof(FSchemaBatch) == 16 && alignof(FSchemaBatch) == 4,		"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FSchemaBatch, NumNestedScopes) == 0,						"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FSchemaBatch, NestedScopesOffset) == 4,					"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FSchemaBatch, NumParametricTypes) == 8,					"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FSchemaBatch, NumSchemas) == 12,							"Add binary format versioning and read old format in AllocateReadSchemas");

//////////////////////////////////////////////////////////////////////////

enum class ESuper : uint8 { No, Unused, Used, Reused };

inline bool UsesSuper(ESuper Inheritance) { return Inheritance == ESuper::Used || Inheritance == ESuper::Reused; }

struct FStructSchema
{
	FTypeId Type;
	uint16 NumMembers;
	uint16 NumRangeTypes;
	uint16 NumInnerSchemas;
	ESuper Inheritance : 2;
	uint8 IsDense : 1;
	// Needed? maybe sufficient check sizes match up
	//uint8 IsContiguous : 1; // Dense and only non-bool leaves. Could extend to handle nested contiguous structs by complicating format slightly.

	FMemberType Footer[0];
	
	static const FMemberType* GetMemberTypes(const FMemberType* Footer)
	{
		return Footer;
	}
	
	static const FMemberType* GetRangeTypes(const FMemberType* Footer, uint32 NumMembers)
	{
		return Footer + NumMembers;
	}

	static const FMemberId*	GetMemberNames(const FMemberType* Footer, uint32 NumMembers, uint32 NumRangeTypes)
	{
		return AlignPtr<FMemberId>(Footer + NumMembers + NumRangeTypes);
	}
	
	static const FSchemaId*	GetInnerSchemas(const FMemberType* Footer, uint32 NumMembers, uint32 NumRangeTypes, uint32 NumNames)
	{
		return AlignPtr<FSchemaId>(GetMemberNames(Footer, NumMembers, NumRangeTypes) + NumNames);
	}

	const FSchemaId* GetInnerSchemas() const
	{
		return GetInnerSchemas(Footer, NumMembers, NumRangeTypes, NumMembers - UsesSuper(Inheritance));
	}

	FOptionalStructSchemaId GetSuperSchema() const
	{
		const FSchemaId* FirstSchema = GetInnerSchemas();
		return Inheritance != ESuper::No ? ToOptional(static_cast<FStructSchemaId>(*FirstSchema)) : NoId;
	}
};

static_assert(sizeof(FStructSchema) == 16 && alignof(FStructSchema) == 4,		"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FStructSchema, Type) == 0,								"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FTypeId, Scope) == 0,									"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FTypeId, Name) == 4,										"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FStructSchema, NumMembers) == 8,							"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FStructSchema, NumRangeTypes) == 10,						"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FStructSchema, NumInnerSchemas) == 12,					"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FStructSchema, Footer) == 15,							"Add binary format versioning and read old format in AllocateReadSchemas");

//////////////////////////////////////////////////////////////////////////

struct FEnumSchema
{
	FTypeId Type;
	uint8 FlagMode : 1;
	uint8 ExplicitConstants : 1; // 0,1,2,3,4.. or 1,2,4,8..
	ELeafWidth Width;
	uint16 Num;
	FNameId Footer[0];
};

static_assert(sizeof(FEnumSchema) == 12 && alignof(FEnumSchema) == 4,	"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FEnumSchema, Type) == 0,							"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FEnumSchema, Width) == 9,						"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FEnumSchema, Num) == 10,							"Add binary format versioning and read old format in AllocateReadSchemas");
static_assert(offsetof(FEnumSchema, Footer) == 12,						"Add binary format versioning and read old format in AllocateReadSchemas");

//////////////////////////////////////////////////////////////////////////

inline bool IsStructOrEnum(FMemberType Type)
{
	return Type.GetKind() == EMemberKind::Struct || (Type.GetKind() == EMemberKind::Leaf && ELeafType::Enum == Type.AsLeaf().Type);
}

inline bool IsSuper(FMemberType Type)
{
	return Type.IsStruct() && Type.AsStruct().IsSuper;
}

inline constexpr uint64 GetLeafRangeSize(uint64 Num, FUnpackedLeafType Leaf)
{
	return Leaf.Type == ELeafType::Bool ? (Num + 7) / 8 : Num * SizeOf(Leaf.Width);
}

//////////////////////////////////////////////////////////////////////////

} // namespace PlainProps