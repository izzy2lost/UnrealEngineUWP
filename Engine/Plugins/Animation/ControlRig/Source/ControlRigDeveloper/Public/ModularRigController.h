// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRig.h"
#include "ModularRigController.generated.h"

struct FRigModuleReference;
struct FModularRigModel;

DECLARE_MULTICAST_DELEGATE_TwoParams(FModularRigModifiedEvent, EModularRigNotification /* type */, const FRigModuleReference* /* element */);


UENUM()
enum class EModularRigNotification : uint8
{
	ModuleAdded,

	ModuleRenamed,

	ModuleRemoved,

	ModuleReparented,

	ConnectionChanged,

	ModuleConfigValueChanged,

	/** MAX - invalid */
	Max UMETA(Hidden),
};

UCLASS(BlueprintType)
class CONTROLRIGDEVELOPER_API UModularRigController : public UObject
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
	bool AddModule(const FName& InModuleName, TSubclassOf<UControlRig> InClass, const FString& InParentModulePath, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	bool ConnectModuleToElement(const FRigElementKey& InConnectorKey, const FRigElementKey& InTargetKey, bool bSetupUndo = true);

	UFUNCTION(BlueprintCallable, Category = "Control Rig | Modules")
	bool SetConfigValueInModule(const FString& InModulePath, const FName& InVariableName, const FString& InValue, bool bSetupUndo = true);

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