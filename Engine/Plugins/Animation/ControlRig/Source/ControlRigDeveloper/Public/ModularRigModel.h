// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRig.h"
#include "ModularRig.h"
#include "ModularRigController.h"
#include "ModularRigModel.generated.h"

USTRUCT(BlueprintType)
struct CONTROLRIGDEVELOPER_API FRigModuleReference
{
	GENERATED_BODY()

	FRigModuleReference()
		: Name(NAME_None)
		, ParentPath(FString())
		, Class(nullptr)
	{}
	
	FRigModuleReference(const FName& InName, TSubclassOf<UControlRig> InClass, const FString& InParentPath)
		: Name(InName)
		, ParentPath(InParentPath)
		, Class(InClass)
	{}

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FString ParentPath;

	UPROPERTY()
	TSoftClassPtr<UControlRig> Class;

	UPROPERTY()
	TMap<FRigElementKey, FRigElementKey> Connections; // Connectors to Connection element

	UPROPERTY()
	TMap<FName, FString> ConfigValues;

	UPROPERTY(transient)
	FName PreviousName;

	UPROPERTY(transient)
	FString PreviousParentPath;

	TArray<FRigModuleReference*> CachedChildren;

	FString GetPath() const;

	FString GetNamespace() const;
	
	friend bool operator==(const FRigModuleReference& A, const FRigModuleReference& B)
	{
		return A.ParentPath == B.ParentPath &&
			A.Name == B.Name;
	}
	
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
	TArray<FRigModuleReference> DeletedModules;

	UPROPERTY(transient)
	TObjectPtr<UModularRigController> Controller;

	UModularRigController* GetController(bool bCreateIfNeeded = true);

	UObject* GetOuter() const { return OuterClientHost.IsValid() ? OuterClientHost.Get() : nullptr; }

	void SetOuterClientHost(UObject* InOuterClientHost);

	void UpdateCachedChildren();

	FRigModuleReference* FindModule(const FString InPath) const;

	FString FindParentPath(const FString InPath) const;

	void ForEachModule(TFunction<bool(const FRigModuleReference*)> PerModule) const;

	TArray<FString> SortPaths(const TArray<FString>& InPaths) const;

private:
	TWeakObjectPtr<UObject> OuterClientHost;

	friend class UModularRigController;
	friend struct FRigModuleReference;
};