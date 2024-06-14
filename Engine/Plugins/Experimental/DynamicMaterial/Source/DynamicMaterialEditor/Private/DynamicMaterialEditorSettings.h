// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "DMDefs.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/Texture.h"
#include "MaterialDomain.h"
#include "PropertyEditorDelegates.h"
#include "UObject/SoftObjectPtr.h"
#include "DynamicMaterialEditorSettings.generated.h"

class FModifierKeysState;
class UMaterialFunctionInterface;
enum EOrientation : int;
struct FDMMaterialChannelListPreset;
struct FPropertyChangedEvent;

USTRUCT(BlueprintType)
struct FDMMaterialEffectList
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Effects")
	FString Name;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TArray<TSoftObjectPtr<UMaterialFunctionInterface>> Effects;
};

UENUM(BlueprintType)
enum class EDMDefaultMaterialPropertySlotValueType : uint8
{
	Texture,
	Color
};

USTRUCT(BlueprintType)
struct FDMDefaultMaterialPropertySlotValue
{
	GENERATED_BODY()

	FDMDefaultMaterialPropertySlotValue();
	FDMDefaultMaterialPropertySlotValue(const TSoftObjectPtr<UTexture>& InTexture);
	FDMDefaultMaterialPropertySlotValue(const FLinearColor& InColor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Designer")
	EDMDefaultMaterialPropertySlotValueType Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Designer", 
		meta = (EditCondition = "Type == EDMDefaultMaterialPropertySlotValueType::Texture", EditConditionHides))
	TSoftObjectPtr<UTexture> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Designer",
		meta = (EditCondition = "Type == EDMDefaultMaterialPropertySlotValueType::Color", EditConditionHides))
	FLinearColor Color;
};

USTRUCT(BlueprintType)
struct FDMMaterialChannelListPreset
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Channel")
	FName Name;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Channel")
	bool bBaseColor = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Channel")
	bool bEmissive = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bOpacity = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bRoughness = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bSpecular = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bMetallic = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bNormal = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bPixelDepthOffset = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bWorldPositionOffset = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bAmbientOcclusion = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bAnisotropy = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bRefraction = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bTangent = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	TEnumAsByte<EBlendMode> DefaultBlendMode = BLEND_Opaque;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	EDMMaterialShadingModel DefaultShadingModel = EDMMaterialShadingModel::Unlit;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bDefaultAnimated = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channel")
	bool bDefaultTwoSided = true;

	bool IsPropertyEnabled(EDMMaterialPropertyType InProperty) const;
};

/**
 * Material Designer Settings
 */
UCLASS(Config=EditorPerProjectUserSettings, meta = (DisplayName = "Material Designer"))
class UDynamicMaterialEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDynamicMaterialEditorSettings();
	virtual ~UDynamicMaterialEditorSettings() override {}

	static UDynamicMaterialEditorSettings* Get();

	/** Changes the currently active material in the designer following actor/object selection. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bFollowSelection;

	/** Adjusts the the spin box value sensitivity. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Layout", meta = (
		ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float SpinBoxValueMultiplier_Default;

	/** Adjusts the the spin box value sensitivity. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Layout", meta = (
		ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float SpinBoxValueMultiplier_Shift;

	/** Adjusts the the spin box value sensitivity. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Layout", meta = (
		ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float SpinBoxValueMultiplier_AltShift;

	/** Adjusts the the spin box value sensitivity. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Layout", meta = (
		ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float SpinBoxValueMultiplier_Cmd;

	/** Adjusts the the spin box value sensitivity. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Layout", meta = (
		ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float SpinBoxValueMultiplier_AltCmd;

	/** Adjusts the vertical size of the material layer view. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (
		ClampMin = "0.05", UIMin = "0.05", ClampMax = "0.95", UIMax = "0.95"))
	float SplitterLocation;

	/** Sets the maximum width for float-based sliders in the editor */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWRite, Category = "Layout", meta = (
		ClampMin = "100", UIMin = "100", ClampMax = "1000", UIMax = "1000"))
	float MaxFloatSliderWidth;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bUVVisualizerVisible;

	/** The size of the material layer preview images. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview", meta = (ClampMin = "32", UIMin = "32", ClampMax = "128", UIMax = "128"))
	int32 LayerPreviewSize;

	/** The size of the material slot preview images. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview", meta = (ClampMin = "40", UIMin = "40", ClampMax = "128", UIMax = "128"))
	int32 SlotPreviewSize;

	/** The size of the material layer details preview images. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview", meta = (ClampMin = "32", UIMin = "32", ClampMax = "128", UIMax = "128"))
	int32 DetailsPreviewSize;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bPreviewImagesUseTextureUVs;

	/**
	 * If true, will display the hovered preview image in an enlarged format inside the tooltip.
	 * 
	 * NOTE: The Material Designer must be re-opened for changes to take effect.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "ToolTips")
	bool bShowTooltipPreview;

	/** The size of the preview image when displayed in the tooltip. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "ToolTips", meta = (EditCondition = "bShowTooltipPreview", ClampMin = "128", UIMin = "128", ClampMax = "2048", UIMax = "2048"))
	int32 TooltipTextureSize;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview")
	TSoftObjectPtr<UTexture> DefaultMask;

	/**
	 * Overrides the default values given to slots created in the given material property.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channels")
	TMap<EDMMaterialPropertyType, FDMDefaultMaterialPropertySlotValue> DefaultSlotValueOverrides;

	/*
	 * Add paths to search for custom effects.
	 *
	 * Format examples:
	 * - /Game/Some/Path
	 * - /Plugin/Some/Path
	 *
	 * The assets must be in a sub-folder of the base path. The sub-folder
	 * will be used as the category name.
	 *
	 * Asset Examples:
	 * - /Game/Some/Path/UV/Asset.Asset -> Category: UV
	 * - /Plugin/Some/Path/Color/OtherAsset.OtherAsset -> Category: Color
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TArray<FName> CustomEffectsFolders;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Channels", meta = (TitleProperty = Name))
	TArray<FDMMaterialChannelListPreset> MaterialChannelPresets;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Material Functions")
	bool bUseLinearColorForVectors;

	UPROPERTY(Config)
	bool bValidatedPresets = false;

	/** This variable is accessed in multiple places, so this is a quick accessor. */
	static bool IsUseLinearColorForVectorsEnabled();

	FOnFinishedChangingProperties OnSettingsChanged;

	//~ Begin UObject
	virtual void PostInitProperties() override;
	virtual void PreEditChange(FEditPropertyChain& InPropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	//~ End UObject

	void OpenEditorSettingsWindow() const;

	void ResetAllLayoutSettings();

	float GetSpinboxValueChangeMultiplier(const FModifierKeysState& InModifierKeys) const;

	TArray<FDMMaterialEffectList> GetEffectList() const;

	const FDMDefaultMaterialPropertySlotValue& GetDefaultSlotValue(EDMMaterialPropertyType InProperty) const;

	const FDMMaterialChannelListPreset* GetPresetByName(FName InName) const;

private:
	TArray<FName> PreEditPresetNames;

	void EnsureUniqueChannelPresetNames();
};
