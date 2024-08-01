// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "DMObjectMaterialProperty.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"

class SBox;
class SDMMaterialDesigner;
class SWidget;
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
	TWeakPtr<SDMMaterialDesigner> DesignerWidgetWeak;
	FName CurrentPreset;
	TSharedPtr<SBox> PresetChannelContainer;
	TWeakObjectPtr<UDynamicMaterialModel> MaterialModelWeak;
	TOptional<FDMObjectMaterialProperty> MaterialObjectProperty;

	TSharedRef<SWidget> CreateLayout();
	TSharedRef<SWidget> CreateChannelPresets();
	TSharedRef<SWidget> CreateChannelList();
	TSharedRef<SWidget> CreateAcceptButton();

	ECheckBoxState Preset_GetState(FName InPresetName) const;
	void Preset_OnChange(ECheckBoxState InState, FName InPresetName);

	FReply Accept_OnClick();

	void OnMaterialBuilt(UDynamicMaterialModelBase* InMaterialModel);

	void OpenMaterialInEditor();
};
