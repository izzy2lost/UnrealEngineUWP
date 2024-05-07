// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMObjectVersion.h"
#include "RigVMVariant.generated.h"

struct FRigVMVariantRef;

USTRUCT(BlueprintType)
struct RIGVM_API FRigVMTag
{
	GENERATED_BODY()
	
	// User applied tag
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category=Variant)
	FString Label;
	
	friend FArchive& operator<<(FArchive& Ar, FRigVMTag& Data)
	{
		Ar.UsingCustomVersion(FRigVMObjectVersion::GUID);
		
		Ar << Data.Label;
		return Ar;
	}
};

USTRUCT(BlueprintType)
struct RIGVM_API FRigVMVariant
{
	GENERATED_BODY()

	FRigVMVariant(const FRigVMVariant& OtherVariant)
	{
		Guid = OtherVariant.Guid;
		Tags = OtherVariant.Tags;
	}

	// Guid which is shared by all variants of the same element
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Variant)
	FGuid Guid;
	
	// Tags applied to this variant
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category=Variant)
	TArray<FRigVMTag> Tags;

	FRigVMVariant()
		: Guid(FGuid()) {}

	friend FArchive& operator<<(FArchive& Ar, FRigVMVariant& Data)
	{
		Ar.UsingCustomVersion(FRigVMObjectVersion::GUID);
		
		Ar << Data.Guid;
		Ar << Data.Tags;
		return Ar;
	}

	static FGuid GenerateGUID(const FString InPath = FString())
	{
		if (!InPath.IsEmpty())
		{
			return FGuid::NewDeterministicGuid(InPath);
		}
		return FGuid::NewGuid();
	}
};

// This struct should not be serialized.
// It is generated on demand.
USTRUCT(BlueprintType)
struct RIGVM_API FRigVMVariantRef
{
	GENERATED_BODY()
	
	FRigVMVariantRef(){}

	FRigVMVariantRef(const FSoftObjectPath& InPath, const FRigVMVariant& InVariant)
		: ObjectPath(InPath), Variant(InVariant) {}
	
	FSoftObjectPath ObjectPath;
	FRigVMVariant Variant;
};
