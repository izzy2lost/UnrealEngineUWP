// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "Misc/NotifyHook.h"
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#include "UObject/Object.h"

#include "Engine/EngineTypes.h"
#include "MaterialDomain.h"
#include "UObject/WeakObjectPtrFwd.h"
#include "UObject/WeakObjectPtrTemplatesFwd.h"

#include "DynamicMaterialModelEditorOnlyData.generated.h"

class UDMMaterialComponent;
class UDMMaterialParameter;
class UDMMaterialProperty;
class UDMMaterialSlot;
class UDMMaterialValue;
class UDMMaterialValueFloat1;
class UDMTextureSet;
class UDMTextureUV;
class UDynamicMaterialModel;
class UDynamicMaterialModelBase;
class UDynamicMaterialModelEditorOnlyData;
class UMaterialExpression;
struct FDMComponentPath;
struct FDMComponentPathSegment;
struct FDMMaterialBuildState;
struct FDMMaterialChannelListPreset;

DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnMaterialBuilt, UDynamicMaterialModelBase*);
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnValueListUpdated, UDynamicMaterialModelBase*);
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnSlotListUpdated, UDynamicMaterialModelBase*);

UENUM(BlueprintType)
enum class EDMState : uint8
{
	Idle,
	Building
};

UCLASS(MinimalAPI, BlueprintType, EditInlineNew, DefaultToInstanced, ClassGroup = "Material Designer")
class UDynamicMaterialModelEditorOnlyData : public UObject, public IDynamicMaterialModelEditorOnlyDataInterface, public FNotifyHook, public IDMBuildable
{
	GENERATED_BODY()

	friend class UDynamicMaterialModelFactory;
	friend class SDMComponentEdit;

public:
	DYNAMICMATERIALEDITOR_API static const FString SlotsPathToken;
	DYNAMICMATERIALEDITOR_API static const FString BaseColorSlotPathToken;
	DYNAMICMATERIALEDITOR_API static const FString EmissiveSlotPathToken;
	DYNAMICMATERIALEDITOR_API static const FString OpacitySlotPathToken;
	DYNAMICMATERIALEDITOR_API static const FString RoughnessPathToken;
	DYNAMICMATERIALEDITOR_API static const FString SpecularPathToken;
	DYNAMICMATERIALEDITOR_API static const FString MetallicPathToken;
	DYNAMICMATERIALEDITOR_API static const FString NormalPathToken;
	DYNAMICMATERIALEDITOR_API static const FString PixelDepthOffsetPathToken;
	DYNAMICMATERIALEDITOR_API static const FString WorldPositionOffsetPathToken;
	DYNAMICMATERIALEDITOR_API static const FString AmbientOcclusionPathToken;
	DYNAMICMATERIALEDITOR_API static const FString AnisotropyPathToken;
	DYNAMICMATERIALEDITOR_API static const FString RefractionPathToken;
	DYNAMICMATERIALEDITOR_API static const FString TangentPathToken;
	DYNAMICMATERIALEDITOR_API static const FString Custom1PathToken;
	DYNAMICMATERIALEDITOR_API static const FString Custom2PathToken;
	DYNAMICMATERIALEDITOR_API static const FString Custom3PathToken;
	DYNAMICMATERIALEDITOR_API static const FString Custom4PathToken;
	DYNAMICMATERIALEDITOR_API static const FString PropertiesPathToken;

	DYNAMICMATERIALEDITOR_API static const TArray<EMaterialDomain> SupportedDomains;
	DYNAMICMATERIALEDITOR_API static const TArray<EBlendMode> SupportedBlendModes;

	DYNAMICMATERIALEDITOR_API static const FName AlphaValueName;

	DYNAMICMATERIALEDITOR_API static UDynamicMaterialModelEditorOnlyData* Get(UDynamicMaterialModelBase* InModelBase);
	DYNAMICMATERIALEDITOR_API static UDynamicMaterialModelEditorOnlyData* Get(const TWeakObjectPtr<UDynamicMaterialModelBase>& InModelBaseWeak);
	DYNAMICMATERIALEDITOR_API static UDynamicMaterialModelEditorOnlyData* Get(UDynamicMaterialModel* InModel);
	DYNAMICMATERIALEDITOR_API static UDynamicMaterialModelEditorOnlyData* Get(const TWeakObjectPtr<UDynamicMaterialModel>& InModelWeak);
	DYNAMICMATERIALEDITOR_API static UDynamicMaterialModelEditorOnlyData* Get(const TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface>& InInterface);
	DYNAMICMATERIALEDITOR_API static UDynamicMaterialModelEditorOnlyData* Get(IDynamicMaterialModelEditorOnlyDataInterface* InInterface);
	DYNAMICMATERIALEDITOR_API static UDynamicMaterialModelEditorOnlyData* Get(UDynamicMaterialInstance* InInstance);

	UDynamicMaterialModelEditorOnlyData();

	void Initialize();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDynamicMaterialModel* GetMaterialModel() const { return MaterialModel; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API UMaterial* GetGeneratedMaterial() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TEnumAsByte<EMaterialDomain> GetDomain() const { return Domain; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API void SetDomain(TEnumAsByte<EMaterialDomain> InDomain);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TEnumAsByte<EBlendMode> GetBlendMode() const { return BlendMode; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API void SetBlendMode(TEnumAsByte<EBlendMode> InBlendMode);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMMaterialShadingModel GetShadingModel() const { return ShadingModel; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API void SetShadingModel(EDMMaterialShadingModel InShadingModel);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API bool IsPixelAnimationFlagSet() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API void SetPixelAnimationFlag(bool bInFlagValue);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API bool IsTwoSidedFlagSet() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API void SetTwoSidedFlag(bool bInFlagValue);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API FName GetChannelListPreset() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API void SetChannelListPreset(FName InPresetName);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API void OpenMaterialEditor() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API TMap<EDMMaterialPropertyType, UDMMaterialProperty*> GetMaterialProperties() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API UDMMaterialProperty* GetMaterialProperty(EDMMaterialPropertyType MaterialProperty) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const TArray<UDMMaterialSlot*>& GetSlots() const { return Slots; }

	/** Gets slot by index. Highly recommended to use GetSlotForMaterialProperty(PropertyType). */
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API UDMMaterialSlot* GetSlot(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API UDMMaterialSlot* GetSlotForMaterialProperty(EDMMaterialPropertyType InType) const;

	/** Adds the next available slot. Highly recommended to use AddSlotForMaterialProperty(PropertyType). */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API UDMMaterialSlot* AddSlot();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API UDMMaterialSlot* AddSlotForMaterialProperty(EDMMaterialPropertyType InType);

	/** Removes the next slot by index. Highly recommended to use RemoveSlotForMaterialProperty(PropertyType). */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API UDMMaterialSlot* RemoveSlot(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API UDMMaterialSlot* RemoveSlotForMaterialProperty(EDMMaterialPropertyType InType);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API TArray<EDMMaterialPropertyType> GetMaterialPropertiesForSlot(const UDMMaterialSlot* Slot) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API void AssignMaterialPropertyToSlot(EDMMaterialPropertyType Property, UDMMaterialSlot* Slot);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API void UnassignMaterialProperty(EDMMaterialPropertyType Property);
	
	FDMOnMaterialBuilt::RegistrationType& GetOnMaterialBuiltDelegate() { return OnMaterialBuiltDelegate; }
	FDMOnValueListUpdated::RegistrationType& GetOnValueListUpdateDelegate() { return OnValueListUpdateDelegate; }
	FDMOnSlotListUpdated::RegistrationType& GetOnSlotListUpdateDelegate() { return OnSlotListUpdateDelegate; }

	DYNAMICMATERIALEDITOR_API TSharedRef<FDMMaterialBuildState> CreateBuildState(UMaterial* InMaterialToBuild, bool bInDirtyAssets = true) const;

	/**
	 * Integrates a Texture Set with the model.
	 * @param InTextureSet The set to integrate.
	 * @param bInReplaceSlots Whether to add to or completely replace slots.
	 * @return True on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API bool AddTextureSet(UDMTextureSet* InTextureSet, bool bInReplaceSlots);

	bool NeedsWizard() const;

	void OnWizardComplete();

	DYNAMICMATERIALEDITOR_API void ForEachMaterialPropertyType(TFunctionRef<EDMIterationResult(EDMMaterialPropertyType InType)> InCallable,
		EDMMaterialPropertyType InStart = static_cast<EDMMaterialPropertyType>(static_cast<uint8>(EDMMaterialPropertyType::None) + 1),
		EDMMaterialPropertyType InEnd = static_cast<EDMMaterialPropertyType>(static_cast<uint8>(EDMMaterialPropertyType::Any) - 1));

	void SaveEditor();

	//~ Begin FNotifyHook
	DYNAMICMATERIALEDITOR_API virtual void NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged);
	//~ End FNotifyHook

	//~ Begin UObject
	DYNAMICMATERIALEDITOR_API virtual void PostLoad() override;
	DYNAMICMATERIALEDITOR_API virtual void PostEditUndo() override;
	DYNAMICMATERIALEDITOR_API virtual void PostEditImport() override;
	DYNAMICMATERIALEDITOR_API virtual void PostDuplicate(bool bDuplicateForPIE) override;
	DYNAMICMATERIALEDITOR_API virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	DYNAMICMATERIALEDITOR_API virtual void Serialize(FArchive& Ar) override;
	//~ End UObject

	//~ Begin IDMBuildable
	virtual void DoBuild_Implementation(bool bInDirtyAssets) override;
	//~ End IDMBuildable

	//~ Begin IDynamicMaterialModelEditorOnlyDataInterface
	DYNAMICMATERIALEDITOR_API virtual void PostEditorDuplicate() override;
	DYNAMICMATERIALEDITOR_API virtual void RequestMaterialBuild() override;
	DYNAMICMATERIALEDITOR_API virtual void OnValueListUpdate() override;
	DYNAMICMATERIALEDITOR_API virtual void OnValueUpdated(UDMMaterialValue* InValue, EDMUpdateType InUpdateType) override;
	DYNAMICMATERIALEDITOR_API virtual void OnTextureUVUpdated(UDMTextureUV* InTextureUV) override;
	DYNAMICMATERIALEDITOR_API virtual void LoadDeprecatedModelData(UDynamicMaterialModel* InMaterialModel) override;
	DYNAMICMATERIALEDITOR_API virtual TSharedRef<IDMMaterialBuildStateInterface> CreateBuildStateInterface(UMaterial* InMaterialToBuild) const override;
	DYNAMICMATERIALEDITOR_API virtual void SetPropertyComponent(EDMMaterialPropertyType InPropertyType, FName InComponentName, UDMMaterialComponent* InComponent) override;
	DYNAMICMATERIALEDITOR_API virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath) const override;
	DYNAMICMATERIALEDITOR_API virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	//~ End IDynamicMaterialModelEditorOnlyDataInterface

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	TObjectPtr<UDynamicMaterialModel> MaterialModel;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, TextExportTransient, Category = "Material Designer")
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

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, TextExportTransient, Category = "Material Designer")
	TMap<EDMMaterialPropertyType, TObjectPtr<UDMMaterialSlot>> PropertySlotMap;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TArray<TObjectPtr<UDMMaterialSlot>> Slots;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TArray<TObjectPtr<UMaterialExpression>> Expressions;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	bool bCreateMaterialPackage;

	FDMOnMaterialBuilt OnMaterialBuiltDelegate;
	FDMOnValueListUpdated OnValueListUpdateDelegate;
	FDMOnSlotListUpdated OnSlotListUpdateDelegate;

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

	void AssignPropertyAlphaValues();

	UFUNCTION()
	TArray<FName> GetPresetOptions() const;

	void OnChannelListPresetChanged();
	void EnsurePresetSlots();
	void OnDomainChanged();
	void OnBlendModeChanged();
	void OnShadingModelChanged();
	void OnPixelAnimationFlagChanged();
	void OnTwoSidedFlagChanged();

	//~ Begin IDynamicMaterialModelEditorOnlyDataInterface
	virtual void ReinitComponents() override;
	//~ End IDynamicMaterialModelEditorOnlyDataInterface

	void LoadDeprecatedModelData_Base(bool bInCreateMaterialPackage, EBlendMode InBlendMode, EDMMaterialShadingModel InShadingModel);
	void LoadDeprecatedModelData_Expressions(TArray<TObjectPtr<UMaterialExpression>>& InExpressions);
	void LoadDeprecatedModelData_Properties(TMap<EDMMaterialPropertyType, TObjectPtr<UObject>>& InProperties);
	void LoadDeprecatedModelData_Slots(TArray<TObjectPtr<UObject>>& InSlots);
	void LoadDeprecatedModelData_PropertySlotMap(TMap<EDMMaterialPropertyType, TObjectPtr<UObject>>& InPropertySlotMap);
};
