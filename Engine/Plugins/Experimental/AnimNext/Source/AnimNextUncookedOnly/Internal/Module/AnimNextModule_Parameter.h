// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetEntry.h"
#include "IAnimNextRigVMExportInterface.h"
#include "IAnimNextRigVMParameterInterface.h"
#include "Param/ParamType.h"
#include "AnimNextModule_Parameter.generated.h"

class UAnimNextModule_EditorData;

namespace UE::AnimNext::Editor
{
	class FParameterCustomization;
}

namespace UE::AnimNext::Tests
{
	class FEditor_Parameters;
}

UCLASS(MinimalAPI, Category = "Parameters")
class UAnimNextModule_Parameter : public UAnimNextRigVMAssetEntry, public IAnimNextRigVMParameterInterface, public IAnimNextRigVMExportInterface
{
	GENERATED_BODY()

	friend class UAnimNextModule_EditorData;
	friend class UE::AnimNext::Tests::FEditor_Parameters;
	friend class UE::AnimNext::Editor::FParameterCustomization;

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

	// IAnimNextRigVMParameterInterface interface
	virtual FAnimNextParamType GetParamType() const override;
	virtual bool SetParamType(const FAnimNextParamType& InType, bool bSetupUndoRedo = true) override;
	virtual FName GetParamName() const override;
	virtual void SetParamName(FName InName, bool bSetupUndoRedo = true) override;
	virtual FInstancedPropertyBag& GetPropertyBag() const override;

	/** Access specifier - whether the parameter is visible external to this asset */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EAnimNextExportAccessSpecifier Access = EAnimNextExportAccessSpecifier::Private;

	/** Parameter name we reference */
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	FName ParameterName;

	/** The parameter's type */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FAnimNextParamType Type = FAnimNextParamType::GetType<bool>();
	
	/** Comment to display in editor */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta=(MultiLine))
	FString Comment;
};

// Old deprecated class
UCLASS()
class UAnimNextParameterBlockParameter : public UAnimNextRigVMAssetEntry
{
	GENERATED_BODY()

	// UAnimNextRigVMAssetEntry interface
	virtual FName GetEntryName() const { return NAME_None; }
};