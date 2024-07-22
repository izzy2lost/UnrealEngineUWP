// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMObjectVersion.h"
#include "RigVMNodeLayout.generated.h"

USTRUCT(BlueprintType)
struct RIGVM_API FRigVMPinCategory
{
	GENERATED_BODY()
	
	FRigVMPinCategory()
	: Path()
	, Elements()
	{}

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	FString Path;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	TArray<FString> Elements;

	friend uint32 GetTypeHash(const FRigVMPinCategory& Category)
	{
		uint32 Hash = GetTypeHash(Category.Path);
		for (const FString& Element : Category.Elements)
		{
			Hash = HashCombine(Hash, GetTypeHash(Element));
		}
		return Hash;
	}

	bool operator < (const FRigVMPinCategory& Other) const
	{
		return FCString::Strcmp(*Path, *Other.Path) < 0;
	}

	friend FArchive& operator<<(FArchive& Ar, FRigVMPinCategory& Category)
	{
		Ar << Category.Path;
		Ar << Category.Elements;
		return Ar;
	}
};

USTRUCT(BlueprintType)
struct RIGVM_API FRigVMNodeLayout
{
	GENERATED_BODY()
	
	FRigVMNodeLayout()
	: Categories()
	, DisplayNames()
	{}

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	TArray<FRigVMPinCategory> Categories;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	TMap<FString, int32> PinIndexInCategory;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=FunctionArgument)
	TMap<FString, FString> DisplayNames;

	void Reset()
	{
		Categories.Reset();
		PinIndexInCategory.Reset();
		DisplayNames.Reset();
	}

	friend uint32 GetTypeHash(const FRigVMNodeLayout& Layout)
	{
		uint32 Hash = 0;;
		for (const FRigVMPinCategory& Category : Layout.Categories)
		{
			Hash = HashCombine(Hash, GetTypeHash(Category));
		}
		for(const TPair<FString, int32>& Pair : Layout.PinIndexInCategory)
		{
			Hash = HashCombine(Hash, GetTypeHash(Pair));
		}
		for(const TPair<FString, FString>& Pair : Layout.DisplayNames)
		{
			Hash = HashCombine(Hash, GetTypeHash(Pair));
		}
		return Hash;
	}

	friend RIGVM_API FArchive& operator<<(FArchive& Ar, FRigVMNodeLayout& Layout);

	const FString* FindCategory(const FString& InElement) const;
	const FString* FindDisplayName(const FString& InElement) const;
};

