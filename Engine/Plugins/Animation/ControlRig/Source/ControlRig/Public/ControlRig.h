// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BaseControlRig.h"

#include "ControlRig.generated.h"

/** Runs logic for mapping input data to transforms (the "Rig") */
UCLASS(Blueprintable, Abstract, editinlinenew)
class CONTROLRIG_API UControlRig : public UBaseControlRig
{
	GENERATED_UCLASS_BODY()

public:

	// BEGIN BaseControlRig
	virtual void InitializeVMs(bool bRequestInit = true) override { URigVMHost::Initialize(bRequestInit); }
	virtual bool InitializeVMs(const FName& InEventName) override { return URigVMHost::InitializeVM(InEventName); }
	virtual void InitializeVMsFromCDO() override { URigVMHost::InitializeFromCDO(); }
	virtual void RequestInitVMs() override { URigVMHost::RequestInit(); }
	virtual bool Execute_Internal(const FName& InEventName) override;
	virtual void EvaluateVMs_AnyThread() override { URigVMHost::Evaluate_AnyThread(); }
#if WITH_EDITOR
	virtual void SetFirstEntryEventInEventQueue(FRigVMExtendedExecuteContext& Context, const FName& InFirstEventName) override;
#endif
	// END BaseControlRig

	UE_DEPRECATED(5.4, "InteractionRig is no longer used") UFUNCTION(BlueprintGetter, meta = (DeprecatedFunction, DeprecationMessage = "InteractionRig is no longer used"))
	UControlRig* GetInteractionRig() const
	{
#if WITH_EDITORONLY_DATA
		return InteractionRig_DEPRECATED;
#endif
		return nullptr;
	}

	UE_DEPRECATED(5.4, "InteractionRig is no longer used")
	UFUNCTION(BlueprintSetter, meta = (DeprecatedFunction, DeprecationMessage = "InteractionRig is no longer used"))
	void SetInteractionRig(UControlRig* InInteractionRig) {}

	UE_DEPRECATED(5.4, "InteractionRig is no longer used")
	UFUNCTION(BlueprintGetter, meta = (DeprecatedFunction, DeprecationMessage = "InteractionRig is no longer used"))
	TSubclassOf<UControlRig> GetInteractionRigClass() const
	{
#if WITH_EDITORONLY_DATA
		return InteractionRigClass_DEPRECATED;
#endif
		return nullptr;
	}

	UE_DEPRECATED(5.4, "InteractionRig is no longer used")
	UFUNCTION(BlueprintSetter, meta = (DeprecatedFunction, DeprecationMessage = "InteractionRig is no longer used"))
	void SetInteractionRigClass(TSubclassOf<UControlRig> InInteractionRigClass) {}
	
private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UControlRig> InteractionRig_DEPRECATED;

	UPROPERTY()
	TSubclassOf<UControlRig> InteractionRigClass_DEPRECATED;
#endif

	
};
