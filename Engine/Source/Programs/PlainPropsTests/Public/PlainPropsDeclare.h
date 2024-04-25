// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include  "PlainPropsTypes.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Templates/UniquePtr.h"

namespace PlainProps
{
struct FEnumerator
{
	FNameId					Name;
	uint64					Constant;
};

enum class EEnumMode { Flat, Flag };

struct FEnumDeclaration
{
	FTypeId					Type;			// Could be removed
	EEnumMode				Mode;
	ELeafWidth				Width;
	uint16					NumEnumerators;
	FEnumerator				Enumerators[0];	// Constants must be unique, no aliases allowed

	TConstArrayView<FEnumerator> GetEnumerators() const
	{
		return MakeArrayView(Enumerators, NumEnumerators);
	}
};

enum class EMemberPresence { RequireAll, AllowSparse };

struct FStructDeclaration
{
	FStructSchemaId			Id;				// Could be removed, might allow declaration dedup among templated types
	FTypeId					Type;			// Could be removed, might allow declaration dedup among templated types
	FOptionalStructSchemaId	Super;
	EMemberPresence			Occupancy;
	uint16					NumMembers;
	FMemberId				MemberOrder[0];

	TConstArrayView<FMemberId> GetMemberOrder() const
	{
		return MakeArrayView(MemberOrder, NumMembers);
	}
};

class FDeclarations
{
public:
	void											DeclareEnum(FEnumSchemaId Id, FTypeId Type, EEnumMode Mode, ELeafWidth Width, TConstArrayView<FEnumerator> Enumerators);
	void											DeclareStruct(FStructSchemaId Id, FTypeId Type, TConstArrayView<FMemberId> MemberOrder, EMemberPresence Occupancy, FOptionalStructSchemaId Super = {});
	
	const FEnumDeclaration&							Get(FEnumSchemaId Id) const		{ return *DeclaredEnums[Id.Idx]; }
	const FStructDeclaration&						Get(FStructSchemaId Id) const	{ return *DeclaredStructs[Id.Idx]; }
	
	TConstArrayView<TUniquePtr<FEnumDeclaration>>	GetEnums() const	{ return DeclaredEnums; }
	TConstArrayView<TUniquePtr<FStructDeclaration>>	GetStructs() const	{ return DeclaredStructs; }

protected:
	TArray<TUniquePtr<FEnumDeclaration>>			DeclaredEnums;
	TArray<TUniquePtr<FStructDeclaration>>			DeclaredStructs;

};

} // namespace PlainProps