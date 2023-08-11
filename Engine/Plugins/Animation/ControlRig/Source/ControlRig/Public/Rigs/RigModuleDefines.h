// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyElements.h"
#include "RigModuleDefines.generated.h"

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleConnector
{
	GENERATED_BODY()
	
	FRigModuleConnector()
		: bIsRoot(false)
	{}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Connector)
	FString Name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Connector)
	bool bIsRoot;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Connector)
	FRigConnectorSettings Settings;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleSettings
{
	GENERATED_BODY()
	
	FRigModuleSettings()
	{}

	bool IsValidModule() const { return !ExposedConnectors.IsEmpty(); }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Module,  meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	FSoftObjectPath Icon;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Module)
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