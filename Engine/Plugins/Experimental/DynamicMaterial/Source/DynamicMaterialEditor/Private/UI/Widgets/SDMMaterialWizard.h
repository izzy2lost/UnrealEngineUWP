// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "DMObjectMaterialProperty.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"

class FAssetThumbnail;
class SBox;
class SDMMaterialDesigner;
class SWidget;
class SWidgetSwitcher;
class UDynamicMaterialModel;
enum class ECheckBoxState : uint8;

class SDMMaterialWizard : public SCompoundWidget
{
public:
	SLATE_DECLARE_WIDGET(SDMMaterialWizard, SCompoundWidget)

	SLATE_BEGIN_ARGS(SDMMaterialWizard)
		: _MaterialModel(nullptr)
		, _MaterialProperty(TOptional<FDMObjectMaterialProperty>())
		{}
		SLATE_ARGUMENT(UDynamicMaterialModel*, MaterialModel)
		SLATE_ARGUMENT(TOptional<FDMObjectMaterialProperty>, MaterialProperty)
	SLATE_END_ARGS()

	virtual ~SDMMaterialWizard() override;

	void Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialDesigner>& InDesignerWidget);

	TSharedPtr<SDMMaterialDesigner> GetDesignerWidget() const;

	UDynamicMaterialModel* GetMaterialModel() const;

protected:
	static TArray<FAssetData> GetTemplateMaterials();

	TWeakPtr<SDMMaterialDesigner> DesignerWidgetWeak;
	FName CurrentPreset;
	TSharedPtr<SBox> PresetChannelContainer;
	TWeakObjectPtr<UDynamicMaterialModel> MaterialModelWeak;
	TOptional<FDMObjectMaterialProperty> MaterialObjectProperty;
	TSharedPtr<SWidgetSwitcher> Switcher;
	TArray<TSharedRef<FAssetThumbnail>> Thumbnails;
	TArray<FAssetData> Assets;

	TSharedRef<SWidget> CreateLayout();
	TSharedRef<SWidget> CreateModeSelector();
	TSharedRef<SWidget> CreateSelectPresetLayout();
	TSharedRef<SWidget> CreateSelectPreset_ChannelPresets();
	TSharedRef<SWidget> CreateSelectPreset_ChannelList();
	TSharedRef<SWidget> CreateSelectPreset_AcceptButton();
	TSharedRef<SWidget> CreateTemplateListLayout();
	TSharedRef<SWidget> CreateTemplateList_Entry(const FAssetData& InTemplateAsset);

	ECheckBoxState Preset_GetState(FName InPresetName) const;
	void Preset_OnChange(ECheckBoxState InState, FName InPresetName);

	FReply Accept_OnClick();

	void OnMaterialBuilt(UDynamicMaterialModelBase* InMaterialModel);

	void OpenMaterialInEditor();

	ECheckBoxState IsModeSelected(int32 InMode) const;

	void SetMode(ECheckBoxState InState, int32 InMode);

	FReply OnTemplateMouseDown(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent, int32 InAssetIndex);

	void CreateDynamicMaterialInInstance(UDynamicMaterialModel* InTemplateModel, UDynamicMaterialInstance* InToInstance);

	void CreateNewDynamicInstanceInActor(UDynamicMaterialModel* InFromModel, FDMObjectMaterialProperty& InMaterialObjectProperty);

	void SetChannelListInModel(FName InChannelList, UDynamicMaterialModel* InMaterialModel);

	void CreateTemplateMaterialInActor(FName InChannelList, FDMObjectMaterialProperty& InMaterialObjectProperty);
};
