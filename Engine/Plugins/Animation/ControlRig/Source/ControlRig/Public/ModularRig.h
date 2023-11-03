// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ControlRig.h"

#include "ModularRig.generated.h"

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleInstance 
{
	GENERATED_USTRUCT_BODY()

	FRigModuleInstance()
	: Name(NAME_None)
	, Rig(nullptr)
	, ParentPath(FString())
	{
	}

	~FRigModuleInstance();

	UPROPERTY()
	FName Name;

	UPROPERTY()
	TSoftObjectPtr<UControlRig> Rig;

	UPROPERTY()
	FString ParentPath;

	TArray<FRigModuleInstance*> CachedChildren;
};

/** Runs logic for mapping input data to transforms (the "Rig") */
UCLASS(Blueprintable, Abstract, editinlinenew)
class CONTROLRIG_API UModularRig : public UControlRig
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FRigModuleInstance> Modules;
	TArray<FRigModuleInstance*> RootModules;

public:

	// BEGIN ControlRig
	virtual void InitializeVMs(bool bRequestInit = true) override;
	virtual bool InitializeVMs(const FName& InEventName) override;
	virtual void InitializeVMsFromCDO() override { URigVMHost::InitializeFromCDO(); }
	virtual void RequestInitVMs() override { URigVMHost::RequestInit(); }
	virtual bool Execute_Internal(const FName& InEventName) override;
	virtual FRigElementKeyRedirector& GetElementKeyRedirector() override { return ElementKeyRedirector; }
	// END ControlRig

	// BEGIN UObject
	virtual void BeginDestroy() override;
	// END UObject
	
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	void ResetModules();

	/** Adds a module to the rig*/
	bool AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, FString InParentPath, const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariables);
	FRigModuleInstance* AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, FRigModuleInstance* InParent, const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariables);

	FRigModuleInstance* FindModule(const FString& InPath);

	void ForEachModule(TFunctionRef<bool(FRigModuleInstance*)> PerModuleFunction);

	static const FString NamespaceSeparator;
};
