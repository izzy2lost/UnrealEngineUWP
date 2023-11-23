// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParameterBlockEntry.h"
#include "IAnimNextParameterBlockGraphInterface.h"
#include "IAnimNextParameterBlockParameterInterface.h"
#include "AnimNextParameterBlockBinding.generated.h"

class UAssetDefinition_AnimNextParameterBlockBinding;
class UAnimNextParameterLibrary;
class URigVMGraph;

namespace UE::AnimNext::Editor
{
	class SParameterBlockViewRow;
}

/** Parameter binding block entry */
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextParameterBlockBinding : public UAnimNextParameterBlockEntry, public IAnimNextParameterBlockParameterInterface, public IAnimNextParameterBlockGraphInterface
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlock_EditorData;
	friend class UAssetDefinition_AnimNextParameterBlockBinding;
	friend class UE::AnimNext::Editor::SParameterBlockViewRow;

	// IAnimNextParameterBlockParameterInterface interface
	virtual FAnimNextParamType GetParamType() const override;
	virtual void SetParameterName(FName InName, bool bSetupUndoRedo = true) override;
	virtual FName GetParameterName() const override;	
	virtual bool SetParamType(const FAnimNextParamType& InType, bool bSetupUndoRedo = true) override;

	// IAnimNextParameterBlockGraphInterface interface
	virtual URigVMGraph* GetGraph() const override { return BindingGraph; }
	virtual FName GetGraphName() const override { return ParameterName; }
	virtual void SetGraphName(FName InName, bool bSetupUndoRedo = true) override { SetParameterName(InName, bSetupUndoRedo); };

	// UAnimNextParameterBlockEntry interface
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayNameTooltip() const override;

	/** Parameter name we reference */
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	FName ParameterName;

	/** The parameter's type */
	UPROPERTY(EditAnywhere, Category = "Parameter", AssetRegistrySearchable)
	FAnimNextParamType Type = FAnimNextParamType::GetType<bool>();

	/** Binding graph */
	UPROPERTY()
	TObjectPtr<URigVMGraph> BindingGraph;
};