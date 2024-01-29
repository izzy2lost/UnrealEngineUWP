// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialComponent.h"
#include "DMEDefs.h"
#include "DMMaterialStage.generated.h"

class FAssetThumbnailPool;
class FMenuBuilder;
class SWidget;
class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDMMaterialStageBlend;
class UDMMaterialStageExpression;
class UDMMaterialStageFunction;
class UDMMaterialStageGradient;
class UDMMaterialStageInput;
class UDMMaterialStageInputExpression;
class UDMMaterialStageInputFunction;
class UDMMaterialStageInputGradient;
class UDMMaterialStageInputSlot;
class UDMMaterialStageInputTextureUV;
class UDMMaterialStageInputValue;
class UDMMaterialStageSource;
class UDMMaterialStageThroughput;
class UDMMaterialValue;
class UDMTextureUV;
class UMaterial;
class UMaterialInstanceDynamic;
class UMaterialInterface;
enum class EDMMaterialLayerStage : uint8;
struct FDMMaterialBuildState;
struct FDMTextureUV;

/**
 * A node which handles a specific operation and manages its inputs and outputs.
 */
UCLASS(BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Stage"))
class DYNAMICMATERIALEDITOR_API UDMMaterialStage : public UDMMaterialComponent
{
	GENERATED_BODY()

	friend class FDMThroughputPropertyRowGenerator;

public:
	static const FString SourcePathToken;
	static const FString InputsPathToken;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStage* CreateMaterialStage(UDMMaterialLayerObject* InLayer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialLayerObject* GetLayer() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialStageSource* GetSource() const { return Source; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsEnabled() const { return bEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool CanChangeSource() const { return bCanChangeSource; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetCanChangeSource(bool bInCanChangeSource) { bCanChangeSource = bInCanChangeSource; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetSource(UDMMaterialStageSource* InSource);

	//~ Begin UDMMaterialComponent
	virtual FText GetComponentDescription() const override;
	//~ End UDMMaterialComponent

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const TArray<UDMMaterialStageInput*>& GetInputs() const { return Inputs; }

	/** Determines what connects to what on this stage's Source. */
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const TArray<FDMMaterialStageConnection>& GetInputConnectionMap() const { return InputConnectionMap; }

	TArray<FDMMaterialStageConnection>& GetInputConnectionMap() { return InputConnectionMap; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	EDMValueType GetSourceType(const FDMMaterialStageConnectorChannel& InChannel) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool IsInputMapped(int32 InputIndex) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual bool IsCompatibleWithPreviousStage(const UDMMaterialStage* InPreviousStage) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual bool IsCompatibleWithNextStage(const UDMMaterialStage* InNextStage) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void AddInput(UDMMaterialStageInput* InNewInput);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void RemoveInput(UDMMaterialStageInput* InInput);

	virtual void InputUpdated(UDMMaterialStageInput* InInput, EDMUpdateType InUpdateType);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	virtual void ResetInputConnectionMap();

	void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsBeingEdited() const { return bIsBeingEdited; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SetBeingEdited(bool bInBeingEdited);

	TMap<EDMMaterialPropertyType, UDMMaterialLayerObject*> GetPreviousStagesPropertyMap();
	TMap<EDMMaterialPropertyType, UDMMaterialLayerObject*> GetPropertyMap();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputValue* ChangeSource_NewLocalValue(EDMValueType InType);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputValue* ChangeSource_Value(UDMMaterialValue* InValue);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputValue* ChangeSource_NewValue(EDMValueType InType);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputSlot* ChangeSource_Slot(UDMMaterialSlot* InSlot, EDMMaterialPropertyType InProperty);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageExpression* ChangeSource_Expression(TSubclassOf<UDMMaterialStageExpression> InExpressionClass);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageBlend* ChangeSource_Blend(TSubclassOf<UDMMaterialStageBlend> InBlendClass);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageGradient* ChangeSource_Gradient(TSubclassOf<UDMMaterialStageGradient> InGradientClass);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputTextureUV* ChangeSource_UV(bool bInDoUpdate);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageFunction* ChangeSource_MaterialFunction();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageSource* ChangeInput_PreviousStage(int32 InInputIdx, int32 InInputChannel, EDMMaterialPropertyType InPreviousStageProperty, 
		int32 InOutputIdx, int32 InOutputChannel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputValue* ChangeInput_NewLocalValue(int32 InInputIdx, int32 InInputChannel, EDMValueType InType, int32 InOutputChannel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputValue* ChangeInput_Value(int32 InInputIdx, int32 InInputChannel, UDMMaterialValue* InValue, int32 InOutputChannel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputValue* ChangeInput_NewValue(int32 InInputIdx, int32 InInputChannel, EDMValueType InType, int32 InOutputChannel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputSlot* ChangeInput_Slot(int32 InInputIdx, int32 InInputChannel, UDMMaterialSlot* InSlot, EDMMaterialPropertyType InProperty, 
		int32 InOutputIdx, int32 InOutputChannel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputExpression* ChangeInput_Expression(int32 InInputIdx, int32 InInputChannel, TSubclassOf<UDMMaterialStageExpression> InExpressionClass, 
		int32 InOutputIdx, int32 InOutputChannel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputTextureUV* ChangeInput_UV(int32 InInputIdx, int32 InInputChannel, int32 InOutputChannel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputGradient* ChangeInput_Gradient(int32 InInputIdx, int32 InInputChannel, TSubclassOf<UDMMaterialStageGradient> InGradientClass, int32 InOutputChannel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStageInputFunction* ChangeInput_MaterialFunction(int32 InInputIdx, int32 InInputChannel);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void RemoveUnusedInputs();

	/** Returns true if any changes were made */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool VerifyAllInputMaps();

	/** Returns true if any changes were made */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool VerifyInputMap(int32 InInputIdx);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterialInterface* GetPreviewMaterial();

	const FDMMaterialStageConnectorChannel* FindInputChannel(UDMMaterialStageInput* InStageInput);

	void UpdateInputMap(int32 InInputIdx, int32 InSourceIndex, int32 InInputChannel, int32 InOutputIdx, int32 InOutputChannel, EDMMaterialPropertyType InStageProperty);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	int32 FindIndex() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStage* GetPreviousStage() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialStage* GetNextStage() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual bool IsRootStage() const;

	//~ Begin UDMMaterialComponent
	virtual void Update(EDMUpdateType InUpdateType) override;
	virtual void DoClean() override;
	virtual FString GetComponentPathComponent() const override;
	virtual UDMMaterialComponent* GetParentComponent() const override;
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

	//~ Begin UObject
	virtual bool Modify(bool bInAlwaysMarkDirty = true) override;
	virtual void PostEditUndo() override;
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
	//~ End UObject

protected:
	UDMMaterialStage();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TObjectPtr<UDMMaterialStageSource> Source;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TArray<TObjectPtr<UDMMaterialStageInput>> Inputs;

	/** How our inputs connect to the inputs of this stage's source */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TArray<FDMMaterialStageConnection> InputConnectionMap;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bEnabled;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bCanChangeSource;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	bool bIsBeingEdited;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TObjectPtr<UMaterial> PreviewMaterialBase;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TObjectPtr<UMaterialInstanceDynamic> PreviewMaterialDynamic;

	void CreatePreviewMaterial();
	void UpdatePreviewMaterial();

	//~ Begin UDMMaterialComponent
	virtual void OnComponentAdded() override;
	virtual void OnComponentRemoved() override;
	virtual void GetComponentPathInternal(TArray<FString>& OutChildComponentPathComponents) const override;
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	//~ End UDMMaterialComponent

	virtual void AddDelegates();
	virtual void RemoveDelegates();

	void OnValueUpdated(UDynamicMaterialModel* InMaterialModel, UDMMaterialValue* InValue);
	void OnTextureUVUpdated(UDynamicMaterialModel* InMaterialModel, UDMTextureUV* InTextureUV);

	void ApplyWholeLayerValue(UDMMaterialValue* InValue);
};
