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
	
	const UModularRig* GetModularRig() const
	{
		return ModularRig.Get();
	}
	const FString& GetPath() const { return Path; }

	const FRigModuleInstance* Get() const;

private:

	mutable TSoftObjectPtr<UModularRig> ModularRig;
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

	UPROPERTY()
	TMap<FName, FRigVMExternalVariable> VariableBindings;

	TArray<FRigModuleInstance*> CachedChildren;

	FString GetPath() const;
	FString GetNamespace() const;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleExecutionElement
{
	GENERATED_USTRUCT_BODY()

	FRigModuleExecutionElement()
		: ModulePath(FString())
		, ModuleInstance(nullptr)
		, EventName(NAME_None)
		, bExecuted(false)
	{}

	FRigModuleExecutionElement(FRigModuleInstance* InModule, FName InEvent)
		: ModulePath(InModule->GetPath())
		, ModuleInstance(InModule)
		, EventName(InEvent)
		, bExecuted(false)
	{}

	UPROPERTY()
	FString ModulePath;
	FRigModuleInstance* ModuleInstance;

	UPROPERTY()
	FName EventName;

	UPROPERTY()
	bool bExecuted;
};

/** Runs logic for mapping input data to transforms (the "Rig") */
UCLASS(Blueprintable, Abstract, editinlinenew)
class CONTROLRIG_API UModularRig : public UControlRig
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FRigModuleInstance> Modules;
	TArray<FRigModuleInstance*> RootModules;

	TArray<FName> SupportedEvents;

public:

	// BEGIN ControlRig
	virtual void Initialize(bool bRequestInit) override;
	virtual void InitializeVMs(bool bRequestInit = true) override;
	virtual bool InitializeVMs(const FName& InEventName) override;
	virtual void InitializeVMsFromCDO() override { URigVMHost::InitializeFromCDO(); }
	virtual void InitializeFromCDO() override;
	virtual void RequestInitVMs() override { URigVMHost::RequestInit(); }
	virtual bool Execute_Internal(const FName& InEventName) override;
	virtual void Evaluate_AnyThread() override;
	virtual FRigElementKeyRedirector& GetElementKeyRedirector() override { return ElementKeyRedirector; }
	virtual bool SupportsEvent(const FName& InEventName) const override { return SupportedEvents.Contains(InEventName); }
	virtual const TArray<FName>& GetSupportedEvents() const override { return SupportedEvents; }
	// END ControlRig

	UPROPERTY()
	TArray<FRigModuleExecutionElement> ExecutionQueue;
	int32 ExecutionQueueFront = 0;
	void ExecuteQueue();
	void ResetExecutionQueue();

	// BEGIN UObject
	virtual void BeginDestroy() override;
	// END UObject
	
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	void ResetModules();

	void UpdateCachedChildren();
	void UpdateSupportedEvents();

	/** Adds a module to the rig*/
	bool AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, FString InParentPath, const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariableDefaultValues, const TMap<FName, FString>& InVariableBindings);
	FRigModuleInstance* AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, FRigModuleInstance* InParent, const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariableDefaultValues, const TMap<FName, FString>& InVariableBindings);
	FRigModuleInstance* AddModuleInstance(const FRigModuleInstance* InOtherModule);

	const FRigModuleInstance* FindModule(const FString& InPath) const;
	const FRigModuleInstance* FindModule(const UControlRig* InModuleInstance) const;
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
		if(FindModule(InPath))
		{
			return FModuleInstanceHandle((UModularRig*)this, InPath);
		}
		return FModuleInstanceHandle();
	}

	static const FString NamespaceSeparator;
};
