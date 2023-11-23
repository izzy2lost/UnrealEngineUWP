// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParameterBlockEntry.h"
#include "IAnimNextParameterBlockParameterInterface.h"
#include "AnimNextParameterBlockParameter.generated.h"

class UAnimNextParameterLibrary;
class UAnimNextParameterBlock_EditorData;

namespace UE::AnimNext::Editor
{
	class FParameterBlockParameterCustomization;
}

UCLASS(MinimalAPI)
class UAnimNextParameterBlockParameter : public UAnimNextParameterBlockEntry, public IAnimNextParameterBlockParameterInterface
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlock_EditorData;
	friend class FAnimationAnimNextParametersEditorTest_Block;
	friend class UE::AnimNext::Editor::FParameterBlockParameterCustomization;
	
	// UAnimNextParameterBlockEntry interface
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayNameTooltip() const override;

	// IAnimNextParameterBlockParameterInterface interface
	virtual FAnimNextParamType GetParamType() const override;
	virtual void SetParameterName(FName InName, bool bSetupUndoRedo = true) override;
	virtual FName GetParameterName() const override;
	virtual bool SetParamType(const FAnimNextParamType& InType, bool bSetupUndoRedo = true) override;
	
	/** Parameter name we reference */
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	FName ParameterName;

	/** The parameter's type */
	UPROPERTY(EditAnywhere, Category = "Parameter", AssetRegistrySearchable)
	FAnimNextParamType Type = FAnimNextParamType::GetType<bool>();
	
	/** Comment to display in editor */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta=(MultiLine))
	FString Comment;
};