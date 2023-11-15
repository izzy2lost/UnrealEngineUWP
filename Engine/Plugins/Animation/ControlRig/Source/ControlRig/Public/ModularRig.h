// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ControlRig.h"

#include "ModularRig.generated.h"

struct FRigModuleInstance;

struct CONTROLRIG_API FModuleInstanceHandle
{
public:

	FModuleInstanceHandle()
		: ModularRig(nullptr)
		, Path()
	{}

	FModuleInstanceHandle(UModularRig* InModularRig, const FString& InPath);
	FModuleInstanceHandle(UModularRig* InModularRig, const FRigModuleInstance* InElement);

	bool IsValid() const { return Get() != nullptr; }
	operator bool() const { return IsValid(); }
	
	const UModularRig* GetModularRig() const { return ModularRig.Get(); }
	UModularRig* GetHierarchy() { return ModularRig.Get(); }
	const FString& GetPath() const { return Path; }

	const FRigModuleInstance* Get() const;
	FRigModuleInstance* Get();

private:

	TWeakObjectPtr<UModularRig> ModularRig;
	FString Path;
};

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

	UPROPERTY()
	FName Name;

	UPROPERTY()
	TSoftObjectPtr<UControlRig> Rig;

	UPROPERTY()
	FString ParentPath;

	TArray<FRigModuleInstance*> CachedChildren;

	FString GetPath() const;
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

	void UpdateCachedChildren();

	/** Adds a module to the rig*/
	bool AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, FString InParentPath, const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariables);
	FRigModuleInstance* AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, FRigModuleInstance* InParent, const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariables);

	FRigModuleInstance* FindModule(const FString& InPath) const;
	FString GetParentPath(const FString& InPath) const;

	void ForEachModule(TFunctionRef<bool(FRigModuleInstance*)> PerModuleFunction);
	void ForEachModule(TFunctionRef<bool(const FRigModuleInstance*)> PerModuleFunction) const;

	/**
	 * Returns a handle to an existing element
	 * @param InKey The key of the handle to retrieve.
	 * @return The retrieved handle (may be invalid)
	 */
	FModuleInstanceHandle GetHandle(const FString& InPath) const
	{
		if(FRigModuleInstance* Module = FindModule(InPath))
		{
			return FModuleInstanceHandle((UModularRig*)this, InPath);
		}
		return FModuleInstanceHandle();
	}

	static const FString NamespaceSeparator;
};
