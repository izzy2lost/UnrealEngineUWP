// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SDMEditor;
class SWidget;
class UDynamicMaterialModel;
enum class ECheckBoxState : uint8;
enum class EDMMaterialShadingModel : uint8;
enum EBlendMode : int;
enum EMaterialDomain : int;

class SDMMaterialWizard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMMaterialWizard) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SDMEditor>& InEditor);

	TSharedPtr<SDMEditor> GetEditor() const;

	UDynamicMaterialModel* GetMaterialModel() const;

protected:
	TWeakPtr<SDMEditor> EditorWeak;
	FName CurrentPreset;
	TSharedPtr<SBox> PresetChannelContainer;

	TSharedRef<SWidget> CreateLayout();
	TSharedRef<SWidget> CreateChannelPresets();
	TSharedRef<SWidget> CreateChannelList();
	TSharedRef<SWidget> CreateAcceptButton();

	ECheckBoxState Preset_GetState(FName InPresetName) const;
	void Preset_OnChange(ECheckBoxState InState, FName InPresetName);

	FReply Accept_OnClick();
};
