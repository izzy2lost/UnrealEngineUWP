// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#include "DMEDefs.h"
#include "Engine/EngineTypes.h"
#include "MaterialDomain.h"
#include "Misc/NotifyHook.h"
#include "UObject/WeakObjectPtrFwd.h"
#include "UObject/WeakObjectPtrTemplatesFwd.h"
#include "DynamicMaterialModelEditorOnlyData.generated.h"

class UDMMaterialComponent;
class UDMMaterialParameter;
class UDMMaterialProperty;
class UDMMaterialSlot;
class UDMMaterialValue;
class UDMMaterialValueFloat1;
class UDMTextureUV;
class UDynamicMaterialModel;
class UDynamicMaterialModelEditorOnlyData;
class UMaterialExpression;
struct FDMComponentPath;
struct FDMComponentPathSegment;
struct FDMMaterialBuildState;
struct FDMMaterialChannelListPreset;

DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnMaterialBuilt, UDynamicMaterialModel*);
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnValueListUpdated, UDynamicMaterialModel*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnValueUpdated, UDynamicMaterialModel*, UDMMaterialValue*);
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnSlotListUpdated, UDynamicMaterialModel*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnTextureUVUpdated, UDynamicMaterialModel*, UDMTextureUV*);

UENUM(BlueprintType)
enum class EDMState : uint8
{
	Idle,
	Building
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDynamicMaterialModelEditorOnlyData : public UObject, public IDynamicMaterialModelEditorOnlyDataInterface, public FNotifyHook, public IDMBuildable
{
	GENERATED_BODY()

	friend class UDynamicMaterialModelFactory;
	friend class SDMComponentEdit;

public:
	static const FString SlotsPathToken;
	static const FString RGBSlotPathToken;
	static const FString OpacitySlotPathToken;
	static const FString RoughnessPathToken;
	static const FString SpecularPathToken;
	static const FString MetallicPathToken;
	static const FString NormalPathToken;
	static const FString PixelDepthOffsetPathToken;
	static const FString WorldPositionOffsetPathToken;
	static const FString AmbientOcclusionPathToken;
	static const FString AnisotropyPathToken;
	static const FString RefractionPathToken;
	static const FString TangentPathToken;
	static const FString Custom1PathToken;
	static const FString Custom2PathToken;
	static const FString Custom3PathToken;
	static const FString Custom4PathToken;
	static const FString PropertiesPathToken;

	static const TArray<EMaterialDomain> SupportedDomains;
	static const TArray<EBlendMode> SupportedBlendModes;

	static const FName AlphaValueName;

	static UDynamicMaterialModelEditorOnlyData* Get(UDynamicMaterialModel* InModel);
	static UDynamicMaterialModelEditorOnlyData* Get(TWeakObjectPtr<UDynamicMaterialModel> InModelWeak);
	static UDynamicMaterialModelEditorOnlyData* Get(const TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface>& InInterface);
	static UDynamicMaterialModelEditorOnlyData* Get(IDynamicMaterialModelEditorOnlyDataInterface* InInterface);

	UDynamicMaterialModelEditorOnlyData();

	void Initialize();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDynamicMaterialModel* GetMaterialModel() const { return MaterialModel; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UMaterial* GetGeneratedMaterial() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TEnumAsByte<EMaterialDomain> GetDomain() const { return Domain; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetDomain(TEnumAsByte<EMaterialDomain> InDomain);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TEnumAsByte<EBlendMode> GetBlendMode() const { return BlendMode; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetBlendMode(TEnumAsByte<EBlendMode> InBlendMode);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMMaterialShadingModel GetShadingModel() const { return ShadingModel; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetShadingModel(EDMMaterialShadingModel InShadingModel);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsPixelAnimationFlagSet() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetPixelAnimationFlag(bool bInFlagValue);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsTwoSidedFlagSet() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetTwoSidedFlag(bool bInFlagValue);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	FName GetChannelListPreset() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetChannelListPreset(FName InPresetName);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void OpenMaterialEditor() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TMap<EDMMaterialPropertyType, UDMMaterialProperty*> GetMaterialProperties() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialProperty* GetMaterialProperty(EDMMaterialPropertyType MaterialProperty) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const TArray<UDMMaterialSlot*>& GetSlots() const { return Slots; }

	/** Gets slot by index. Highly recommended to use GetSlotForMaterialProperty(PropertyType). */
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialSlot* GetSlot(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialSlot* GetSlotForMaterialProperty(EDMMaterialPropertyType InType) const;

	/** Adds the next available slot. Highly recommended to use AddSlotForMaterialProperty(PropertyType). */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialSlot* AddSlot();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialSlot* AddSlotForMaterialProperty(EDMMaterialPropertyType InType);

	/** Removes the next slot by index. Highly recommended to use RemoveSlotForMaterialProperty(PropertyType). */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialSlot* RemoveSlot(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialSlot* RemoveSlotForMaterialProperty(EDMMaterialPropertyType InType);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TArray<EDMMaterialPropertyType> GetMaterialPropertiesForSlot(UDMMaterialSlot* Slot) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void AssignMaterialPropertyToSlot(EDMMaterialPropertyType Property, UDMMaterialSlot* Slot);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void UnassignMaterialProperty(EDMMaterialPropertyType Property);
	
	FDMOnMaterialBuilt& GetOnMaterialBuiltDelegate() { return OnMaterialBuiltDelegate; }
	FDMOnValueListUpdated& GetOnValueListUpdateDelegate() { return OnValueListUpdateDelegate; }
	FDMOnValueUpdated& GetOnValueUpdateDelegate() { return OnValueUpdateDelegate; }
	FDMOnSlotListUpdated& GetOnSlotListUpdateDelegate() { return OnSlotListUpdateDelegate; }
	FDMOnTextureUVUpdated& GetOnTextureUVUpdateDelegate() { return OnTextureUVUpdateDelegate; }

	void GenerateOpacityExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState, UDMMaterialSlot* InFromSlot,
		EDMMaterialPropertyType InFromProperty, UMaterialExpression*& OutExpression, int32& OutOutputIndex, int32& OutOutputChannel) const;

	TSharedRef<FDMMaterialBuildState> CreateBuildState(UMaterial* InMaterialToBuild, bool bInDirtyAssets = true) const;

	bool NeedsWizard() const;

	void OnWizardComplete();

	void ForEachMaterialPropertyType(TFunctionRef<EDMIterationResult(EDMMaterialPropertyType InType)> InCallable,
		EDMMaterialPropertyType InStart = static_cast<EDMMaterialPropertyType>(static_cast<uint8>(EDMMaterialPropertyType::None) + 1),
		EDMMaterialPropertyType InEnd = static_cast<EDMMaterialPropertyType>(static_cast<uint8>(EDMMaterialPropertyType::Any) - 1));

	//~ Begin FNotifyHook
	virtual void NotifyPreChange(class FEditPropertyChain* PropertyAboutToChange) {}
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged);
	//~ End FNotifyHook

	//~ Begin UObject
	virtual void PostLoad() override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	//~ End UObject

	void SaveEditor();

	//~ Begin IDMBuildable
	virtual void DoBuild_Implementation(bool bInDirtyAssets) override;
	//~ End IDMBuildable

	//~ Begin IDynamicMaterialModelEditorOnlyDataInterface
	virtual void PostEditorDuplicate() override;
	virtual void RequestMaterialBuild() override;
	virtual void OnValueUpdated(UDMMaterialValue* InValue, EDMUpdateType InUpdateType) override;
	virtual void OnValueListUpdate() override;
	virtual void OnTextureUVUpdated(UDMTextureUV* InTextureUV) override;
	virtual void LoadDeprecatedModelData(UDynamicMaterialModel* InMaterialModel) override;
	virtual TSharedRef<IDMMaterialBuildStateInterface> CreateBuildStateInterface(UMaterial* InMaterialToBuild) const override;
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath) const override;
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	//~ End IDynamicMaterialModelEditorOnlyDataInterface

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TObjectPtr<UDynamicMaterialModel> MaterialModel;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	EDMState State;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer", 
		meta = (ValidEnumValues = "MD_Surface,MD_PostProcess,MD_DeferredDecal"))
	TEnumAsByte<EMaterialDomain> Domain;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer",
		meta = (ValidEnumValues = "BLEND_Opaque,BLEND_Translucent,BLEND_Masked,BLEND_Additive,BLEND_Modulate"))
	TEnumAsByte<EBlendMode> BlendMode;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer", meta = (ValidEnumValues = "Unlit,DefaultLit"))
	EDMMaterialShadingModel ShadingModel;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bPixelAnimationFlag;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bTwoSidedFlag;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Material Designer", 
		meta = (GetOptions = GetPresetOptions, NoResetToDefault, DisplayName = "Material Type Preset",
			ToolTip = "Sets the available channels and default material properties."))
	FName ChannelListPreset;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TMap<EDMMaterialPropertyType, TObjectPtr<UDMMaterialProperty>> Properties;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TMap<EDMMaterialPropertyType, TObjectPtr<UDMMaterialSlot>> PropertySlotMap;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TArray<TObjectPtr<UDMMaterialSlot>> Slots;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TArray<TObjectPtr<UMaterialExpression>> Expressions;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bCreateMaterialPackage;

	FDMOnMaterialBuilt OnMaterialBuiltDelegate;
	FDMOnValueListUpdated OnValueListUpdateDelegate;
	FDMOnValueUpdated OnValueUpdateDelegate;
	FDMOnSlotListUpdated OnSlotListUpdateDelegate;
	FDMOnTextureUVUpdated OnTextureUVUpdateDelegate;

	void CreateMaterial();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void BuildMaterial(bool bInDirtyAssets);

	FString GetMaterialAssetPath() const;
	FString GetMaterialAssetName() const;
	FString GetMaterialPackageName(const FString& MaterialBaseName) const;

	void OnSlotConnectorsUpdated(UDMMaterialSlot* Slot);

	/** Swaps the material properties from one slot to another, unless both slots exist and/or are the same. */
	void SwapSlotMaterialProperty(EDMMaterialPropertyType InPropertyFrom, EDMMaterialPropertyType InPropertyTo);

	/** 
	 * Swaps the material properties from one slot to another, unless both slots exist and/or are the same. 
	 * Ensuring that the To Property exists.
	 */
	void EnsureSwapSlotMaterialProperty(EDMMaterialPropertyType InPropertyFrom, EDMMaterialPropertyType InPropertyTo);

	//~ Begin IDynamicMaterialModelEditorOnlyDataInterface
	virtual void ReinitComponents() override;
	virtual void ResetData() override;
	//~ End IDynamicMaterialModelEditorOnlyDataInterface

	void AssignPropertyAlphaValues();

	UFUNCTION()
	TArray<FName> GetPresetOptions() const;

	void OnChannelListPresetChanged();
	void OnDomainChanged();
	void OnBlendModeChanged();
	void OnShadingModelChanged();
	void OnPixelAnimationFlagChanged();
	void OnTwoSidedFlagChanged();

	void LoadDeprecatedModelData_Base(bool bInCreateMaterialPackage, EBlendMode InBlendMode, EDMMaterialShadingModel InShadingModel);
	void LoadDeprecatedModelData_Expressions(TArray<TObjectPtr<UMaterialExpression>>& InExpressions);
	void LoadDeprecatedModelData_Properties(TMap<EDMMaterialPropertyType, TObjectPtr<UObject>>& InProperties);
	void LoadDeprecatedModelData_Slots(TArray<TObjectPtr<UObject>>& InSlots);
	void LoadDeprecatedModelData_PropertySlotMap(TMap<EDMMaterialPropertyType, TObjectPtr<UObject>>& InPropertySlotMap);
};
