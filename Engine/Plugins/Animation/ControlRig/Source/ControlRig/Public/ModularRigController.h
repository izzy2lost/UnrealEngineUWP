// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularRigModel.h"
#include "ModularRigController.generated.h"

struct FRigModuleReference;

DECLARE_MULTICAST_DELEGATE_TwoParams(FModularRigModifiedEvent, EModularRigNotification /* type */, const FRigModuleReference* /* element */);

UCLASS(BlueprintType)
class CONTROLRIG_API UModularRigController : public UObject
{
	GENERATED_UCLASS_BODY()

	UModularRigController()
		: Model(nullptr)
		, bSuspendNotifications(false)
	{
	}

	FModularRigModel* Model;
	FModularRigModifiedEvent ModifiedEvent;
	bool bSuspendNotifications;

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	FString AddModule(const FName& InModuleName, TSubclassOf<UControlRig> InClass, const FString& InParentModulePath, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	bool CanConnectConnectorToElement(const FRigModuleConnector& InConnector, const FRigElementKey& InTargetKey, FText& OutErrorMessage);

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	bool ConnectConnectorToElement(const FRigElementKey& InConnectorKey, const FRigElementKey& InTargetKey, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	bool SetConfigValueInModule(const FString& InModulePath, const FName& InVariableName, const FString& InValue, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	bool BindModuleVariable(const FString& InModulePath, const FName& InVariableName, const FString& InSourcePath, bool bSetupUndo = true);
	bool CanBindModuleVariable(const FString& InModulePath, const FName& InVariableName, const FString& InSourcePath, FText& OutErrorMessage);
	TArray<FString> GetPossibleBindings(const FString& InModulePath, const FName& InVariableName);

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	bool UnBindModuleVariable(const FString& InModulePath, const FName& InVariableName, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	bool DeleteModule(const FString& InModulePath, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	bool RenameModule(const FString& InModulePath, const FName& InNewName, bool bSetupUndo = true);
	bool CanRenameModule(const FString& InModulePath, const FName& InNewName, FText& OutErrorMessage);

	UFUNCTION(BlueprintCallable, Category = "ControlRig | Modules")
	bool ReparentModule(const FString& InModulePath, const FString& InNewParentModulePath, bool bSetupUndo = true);
	
	FName GetSafeNewName(const FString& InModuleDesiredPath);
	void SetModel(FModularRigModel* InModel) { Model = InModel; }
	FRigModuleReference* FindModule(const FString& InPath);
	FModularRigModifiedEvent& OnModified() { return ModifiedEvent; }
	void Notify(const EModularRigNotification& InNotification, const FRigModuleReference* InElement);
};