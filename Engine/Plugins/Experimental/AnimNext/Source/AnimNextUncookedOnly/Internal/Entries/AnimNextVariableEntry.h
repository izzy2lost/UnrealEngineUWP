// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetEntry.h"
#include "IAnimNextRigVMExportInterface.h"
#include "Variables/IAnimNextRigVMVariableInterface.h"
#include "Param/ParamType.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Variables/AnimNextVariableBinding.h"
#include "AnimNextVariableEntry.generated.h"

class UAnimNextRigVMAssetEditorData;
class UAnimNextModule_EditorData;
class UAnimNextVariableBinding;

namespace UE::AnimNext::Editor
{
	class FVariableCustomization;
	class FVariableBindingPropertyCustomization;
	class FUniversalObjectLocatorBindingCustomization;
	class FVariablesOutlinerMode;
}

namespace UE::AnimNext::Tests
{
	class FEditor_Variables;
}

UCLASS(MinimalAPI, Category = "Variables", DisplayName = "Variable")
class UAnimNextVariableEntry : public UAnimNextRigVMAssetEntry, public IAnimNextRigVMVariableInterface, public IAnimNextRigVMExportInterface
{
	GENERATED_BODY()

	friend class UAnimNextRigVMAssetEditorData;
	friend class UAnimNextModule_EditorData;
	friend class UE::AnimNext::Tests::FEditor_Variables;
	friend class UE::AnimNext::Editor::FVariableCustomization;
	friend class UE::AnimNext::Editor::FVariableBindingPropertyCustomization;
	friend class UE::AnimNext::Editor::FUniversalObjectLocatorBindingCustomization;
	friend class UE::AnimNext::Editor::FVariablesOutlinerMode;

	UAnimNextVariableEntry();

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;

	// IAnimNextRigVMExportInterface interface
	virtual FAnimNextParamType GetExportType() const override;
	virtual FName GetExportName() const override;
	virtual EAnimNextExportAccessSpecifier GetExportAccessSpecifier() const override;
	virtual void SetExportAccessSpecifier(EAnimNextExportAccessSpecifier InAccessSpecifier, bool bSetupUndoRedo = true) override;

	// UAnimNextRigVMAssetEntry interface
	virtual FName GetEntryName() const override;
	virtual void SetEntryName(FName InName, bool bSetupUndoRedo = true) override;
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayNameTooltip() const override;

	// IAnimNextRigVMVariableInterface interface
	virtual FAnimNextParamType GetType() const override;
	virtual bool SetType(const FAnimNextParamType& InType, bool bSetupUndoRedo = true) override;
	virtual FName GetVariableName() const override;
	virtual void SetVariableName(FName InName, bool bSetupUndoRedo = true) override;
	virtual bool SetDefaultValue(const FString& InDefaultValue, bool bSetupUndoRedo = true) override;
	virtual const FInstancedPropertyBag& GetPropertyBag() const override;
	virtual const FAnimNextVariableBinding& GetBinding() const override;

	/** Access specifier - whether the variable is visible external to this asset */
	UPROPERTY(EditAnywhere, Category = "Variable")
	EAnimNextExportAccessSpecifier Access = EAnimNextExportAccessSpecifier::Private;

	/** Parameter name we reference */
	UPROPERTY(VisibleAnywhere, Category = "Variable")
	FName ParameterName;

	/** The variable's type */
	UPROPERTY(EditAnywhere, Category = "Variable")
	FAnimNextParamType Type = FAnimNextParamType::GetType<bool>();

	/** Binding data */
	UPROPERTY(EditAnywhere, Category = "Variable")
	FAnimNextVariableBinding Binding;

	/** Comment to display in editor */
	UPROPERTY(EditAnywhere, Category = "Variable", meta=(MultiLine))
	FString Comment;

	/** Property bag holding the default value of the variable */
	UPROPERTY()
	FInstancedPropertyBag DefaultValue;
};
