// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyElements.h"
#include "RigModuleDefines.generated.h"

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleIdentifier
{
	GENERATED_BODY()
	
	FRigModuleIdentifier()
		: Name()
		, Type(TEXT("Module"))
	{}

	// The name of the module used to find it in the module library
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Connector)
	FString Name;

	// The kind of module this is (for example "Arm")
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Connector)
	FString Type;

	bool IsValid() const { return !Name.IsEmpty(); }
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleConnector
{
	GENERATED_BODY()
	
	FRigModuleConnector()
	{}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Connector)
	FString Name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Connector)
	FRigConnectorSettings Settings;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleSettings
{
	GENERATED_BODY()
	
	FRigModuleSettings()
	{}

	bool IsValidModule(bool bRequireExposedConnectors = true) const
	{
		return
			Identifier.IsValid() &&
			(!bRequireExposedConnectors || !ExposedConnectors.IsEmpty());
	}

	// The identifier used to retrieve the module in the module library
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Module)
	FRigModuleIdentifier Identifier;

	// The icon used for the module in the module library
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Module,  meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	FSoftObjectPath Icon;

	UPROPERTY(BlueprintReadOnly, Category = Module)
	TArray<FRigModuleConnector> ExposedConnectors;
};


USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleDescription
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Module)
	FSoftObjectPath Path;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Module)
	FRigModuleSettings Settings;
};