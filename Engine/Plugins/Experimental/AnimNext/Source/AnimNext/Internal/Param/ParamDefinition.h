// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamId.h"
#include "Param/ParamType.h"
#include "Param/ParamTypeHandle.h"

namespace UE::AnimNext
{
	struct FParams;
	struct FParamAdapter;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext
{

enum class EParamDefinitionFlags : uint8
{
	None = 0,

	Mutable = (1 << 0),
};

ENUM_CLASS_FLAGS(EParamDefinitionFlags);

// Definition of a parameter for reserved for internal use
struct FParamDefinition
{
	FParamDefinition() = default;

	FParamDefinition(FName InName, const FAnimNextParamType& InType, const FText& InTooltip, EParamDefinitionFlags InFlags = EParamDefinitionFlags::None)
		: Id(InName)
		, Name(InName)
		, TypeHandle(InType.GetHandle())
		, Type(InType)
#if WITH_EDITORONLY_DATA
		, Tooltip(InTooltip)
#endif
		, Property(nullptr)
		, Function(nullptr)
		, Flags(InFlags)
	{
		// Param names should not be 'none'
		check(Name != NAME_None);
		// Param names should not contain periods. Use underscores.
		// Periods are only a display concern and we want parameters to be expressible as members of objects & structures. 
		check(!Name.ToString().Contains(TEXT(".")));
	}

	FParamDefinition(FName InName, const FAnimNextParamType& InType, EParamDefinitionFlags InFlags = EParamDefinitionFlags::None)
		: FParamDefinition(InName, InType, FText::GetEmpty(), InFlags)
	{
	}
	
	FParamDefinition(FParamId InId, const FAnimNextParamType& InType, const FText& InTooltip, EParamDefinitionFlags InFlags = EParamDefinitionFlags::None)
		: Id(InId)
		, Name(InId.ToName())
		, TypeHandle(InType.GetHandle())
		, Type(InType)
#if WITH_EDITORONLY_DATA
		, Tooltip(InTooltip)
#endif
		, Property(nullptr)
		, Function(nullptr)
		, Flags(InFlags)
	{
	}

	FParamDefinition(FParamId InId, const FAnimNextParamType& InType, EParamDefinitionFlags InFlags = EParamDefinitionFlags::None)
		: FParamDefinition(InId, InType, FText::GetEmpty(), InFlags)
	{
	}

	FName GetName() const
	{
		return Name;
	}

	FParamId GetId() const
	{
		return Id;
	}
	
	FParamTypeHandle GetTypeHandle() const
	{
		return TypeHandle;
	}
	
	const FAnimNextParamType& GetType() const
	{
		return Type;
	}

private:
	friend struct FParamId;
	friend struct FParams;
	friend struct FParamAdapter;
	friend struct UncookedOnly::FUtils;

	FParamDefinition(int32 InIdIndex, FName InName, const FAnimNextParamType& InType, const FText& InTooltip, EParamDefinitionFlags InFlags = EParamDefinitionFlags::None)
		: Id(InIdIndex)
		, Name(InName)
		, TypeHandle(InType.GetHandle())
		, Type(InType)
#if WITH_EDITORONLY_DATA
		, Tooltip(InTooltip)
#endif
		, Property(nullptr)
		, Function(nullptr)
		, Flags(InFlags)
	{
	}

	// Make a parameter definition from its ID and a UObject
	FParamDefinition(uint32 InIdIndex, FName InName, const UObject* InObject);

	// Make a parameter definition from its ID and a property
	FParamDefinition(uint32 InIdIndex, FName InName, const FProperty* InProperty);

	// Make a parameter definition from its ID and the return value of a function
	FParamDefinition(uint32 InIdIndex, FName InName, const UFunction* InFunction);

	FParamId Id;
	FName Name;
	FParamTypeHandle TypeHandle;
	FAnimNextParamType Type;
#if WITH_EDITORONLY_DATA
	FText Tooltip;
#endif
	const FProperty* Property = nullptr;
	const UFunction* Function = nullptr;
	EParamDefinitionFlags Flags = EParamDefinitionFlags::None;
};

}
