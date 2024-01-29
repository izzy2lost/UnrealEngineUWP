// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageThroughput.h"
#include "DMMaterialStageFunction.generated.h"

class UDMMaterialLayerObject;
class UMaterialFunctionInterface;

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageFunction : public UDMMaterialStageThroughput
{
	GENERATED_BODY()

public:
	static constexpr int32 InputPreviousStage = 0;

	static UDMMaterialStage* CreateStage(UDMMaterialLayerObject* InLayer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterialFunctionInterface* GetMaterialFunction() const { return MaterialFunction.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetMaterialFunction(UMaterialFunctionInterface* InMaterialFunction);

	//~ Begin UDMMaterialStageThroughput
	virtual void AddDefaultInput(int32 InInputIndex) const override;
	virtual bool CanChangeInput(int32 InputIndex) const override;
	virtual bool CanChangeInputType(int32 InputIndex) const override;
	virtual bool IsInputVisible(int32 InputIndex) const override;
	//~ End UDMMaterialStageThroughput

	//~ Begin UDMMaterialStageSource
	virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;
	//~ End UDMMaterialStageSource

	//~ Begin UObject
	virtual void PreEditChange(FEditPropertyChain& PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject

protected:
	static TSoftObjectPtr<UMaterialFunctionInterface> NoOp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = GetMaterialFunction, Setter = SetMaterialFunction, BlueprintSetter = SetMaterialFunction, Category = "Material Designer",
		meta = (DisplayThumbnail = true, AllowPrivateAccess = "true", HighPriority, NotKeyframeable))
	TObjectPtr<UMaterialFunctionInterface> MaterialFunction;

	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TObjectPtr<UMaterialFunctionInterface> MaterialFunction_PreEdit;

	UDMMaterialStageFunction();

	void OnMaterialFunctionChanged();
};
