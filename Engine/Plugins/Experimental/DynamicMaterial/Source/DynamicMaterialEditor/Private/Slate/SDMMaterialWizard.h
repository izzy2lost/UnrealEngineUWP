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
	TSharedRef<SWidget> CreateMaterialDomainOptions();
	TSharedRef<SWidget> CreateBlendModeOptions();
	TSharedRef<SWidget> CreateShadingModelOptions();
	TSharedRef<SWidget> CreateAnimationOptions();
	TSharedRef<SWidget> CreateTwoSidedOptions();
	TSharedRef<SWidget> CreateChannelPresets();
	TSharedRef<SWidget> CreateChannelList();
	TSharedRef<SWidget> CreateAcceptButton();

	bool Domain_IsEnabled() const;
	ECheckBoxState Domain_GetState(EMaterialDomain InDomain) const;
	void Domain_OnChange(ECheckBoxState InState, EMaterialDomain InDomain);

	bool BlendMode_IsEnabled() const;
	ECheckBoxState BlendMode_GetState(EBlendMode InBlendMode) const;
	void BlendMode_OnChange(ECheckBoxState InState, EBlendMode InBlendMode);

	bool Unlit_IsEnabled() const;
	ECheckBoxState Unlit_GetState(EDMMaterialShadingModel InValue) const;
	void Unlit_OnChange(ECheckBoxState InState, EDMMaterialShadingModel InValue);

	bool Animated_IsEnabled() const;
	ECheckBoxState Animated_GetState(bool bInValue) const;
	void Animated_OnChange(ECheckBoxState InState, bool bInValue);

	bool TwoSided_IsEnabled() const;
	ECheckBoxState TwoSided_GetState(bool bInValue) const;
	void TwoSided_OnChange(ECheckBoxState InState, bool bInValue);

	ECheckBoxState Preset_GetState(FName InPresetName) const;
	void Preset_OnChange(ECheckBoxState InState, FName InPresetName);

	FReply Accept_OnClick();
};
