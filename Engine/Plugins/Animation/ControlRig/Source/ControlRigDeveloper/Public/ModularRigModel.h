// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRig.h"
#include "ModularRigController.h"
#include "ModularRigModel.generated.h"

USTRUCT(BlueprintType)
struct CONTROLRIGDEVELOPER_API FRigModuleReference
{
	GENERATED_BODY()

	FRigModuleReference()
		: Name(NAME_None)
		, ParentNamespace(FString())
		, Class(nullptr)
	{}
	
	FRigModuleReference(const FName& InName, TSubclassOf<UControlRig> InClass, const FString& InParentPath)
		: Name(InName)
		, ParentNamespace(InParentPath)
		, Class(InClass)
	{}

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FString ParentNamespace;

	UPROPERTY()
	TSoftClassPtr<UControlRig> Class;

	UPROPERTY()
	TMap<FRigElementKey, FRigElementKey> Connections; // Connectors to Connection element

	UPROPERTY()
	TMap<FName, FString> ConfigValues;

	TArray<FRigModuleReference*> CachedChildren;

	FString GetNamespace() const;
	
	friend class UModularRigController;
};

// A management struct containing all modules in the rig
USTRUCT(BlueprintType)
struct CONTROLRIGDEVELOPER_API FModularRigModel
{
public:

	GENERATED_BODY()

	UPROPERTY()
	TArray<FRigModuleReference> Modules;
	TArray<FRigModuleReference*> RootModules;

	UPROPERTY(transient)
	TObjectPtr<UModularRigController> Controller;

	UModularRigController* GetController(bool bCreateIfNeeded = true);

	UObject* GetOuter() const { return OuterClientHost.IsValid() ? OuterClientHost.Get() : nullptr; }

	void SetOuterClientHost(UObject* InOuterClientHost);

	void UpdateCachedChildren();

	FRigModuleReference* FindModule(const FString InNameSpace) const;

	FString FindParentNamespace(const FString InNameSpace) const;
	
private:
	TWeakObjectPtr<UObject> OuterClientHost;

	friend class UModularRigController;
	friend struct FRigModuleReference;
};