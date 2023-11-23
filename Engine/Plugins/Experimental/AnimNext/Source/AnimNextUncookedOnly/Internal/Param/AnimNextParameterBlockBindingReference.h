// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParameterBlockEntry.h"
#include "IAnimNextParameterBlockReferenceInterface.h"
#include "IAnimNextParameterBlockParameterInterface.h"
#include "AnimNextParameterBlockBindingReference.generated.h"

class UAnimNextParameterBlock;
class UAnimNextParameterLibrary;
class UAssetDefinition_AnimNextParameterBlockBindingReference;

namespace UE::AnimNext::Editor
{
	struct FUtils;
	class SParameterBlockViewRow;
}

/** Parameter binding block entry */
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextParameterBlockBindingReference : public UAnimNextParameterBlockEntry, public IAnimNextParameterBlockParameterInterface, public IAnimNextParameterBlockReferenceInterface
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlock_EditorData;
	friend class UAssetDefinition_AnimNextParameterBlockBindingReference;

	// IAnimNextParameterBlockParameterInterface interface
	virtual FAnimNextParamType GetParamType() const override;
	virtual FName GetParameterName() const override;
	virtual void SetParameterName(FName InName, bool bSetupUndoRedo = true) override;	
	virtual bool SetParamType(const FAnimNextParamType& InType, bool bSetupUndoRedo = true) override { return false; }

	// IAnimNextParameterBlockReferenceInterface interface
	virtual const UAnimNextParameterBlock* GetBlock() const override;

	// UAnimNextParameterBlockEntry interface
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayNameTooltip() const override;
	virtual void GetEditedObjects(TArray<UObject*>& OutObjects) const override;

	/** Parameter name we reference */
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	FName ParameterName;

	/** Parameter block we reference */
	UPROPERTY()
	TObjectPtr<UAnimNextParameterBlock> Block;
};