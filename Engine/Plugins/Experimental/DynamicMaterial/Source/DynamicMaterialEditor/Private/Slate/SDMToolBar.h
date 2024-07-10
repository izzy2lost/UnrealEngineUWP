// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/SlateDelegates.h"
#include "Model/DynamicMaterialModel.h"
#include "Widgets/SCompoundWidget.h"

class AActor;
class SBox;
class SDMEditor;
class STextBlock;
class SWidget;
class UDMMaterialStageExpression;
class UDynamicMaterialModel;
class UObject;
class UPackage;
enum class EDMExpressionMenu : uint8;
struct FDMObjectMaterialProperty;

DECLARE_DELEGATE_OneParam(FDMOnActorMaterailSlotChanged, TSharedPtr<FDMObjectMaterialProperty> /** NewSelectedSlot */)

/**
 * Material Designer ToolBar
 * 
 * Displays the selected actor that the Material Designer is editing and allows for switching between slots for that actor.
 */
class SDMToolBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMToolBar) {}
		SLATE_EVENT(FDMOnActorMaterailSlotChanged, OnSlotChanged)
		SLATE_EVENT(FOnGetContent, OnGetSettingsMenu)
	SLATE_END_ARGS()

	virtual ~SDMToolBar() {}

	void Construct(const FArguments& InArgs, const TSharedRef<SDMEditor>& InEditor);

	const TArray<TSharedPtr<FDMObjectMaterialProperty>>& GetMaterialProperties() const { return ActorMaterialProperties; }
	void SetMaterialProperties(const TArray<TSharedPtr<FDMObjectMaterialProperty>>& InActorMaterialProperties);

	AActor* GetMaterialActor() const { return MaterialActorWeak.Get(); }
	void SetMaterialActor(AActor* InActor, const int32 InActiveSlotIndex = 0);

	UDynamicMaterialModelBase* GetMaterialModelBase() const;

	void OnMaterialModelChanged();

	FText GetActorName() const;

protected:
	static UPackage* GetSaveablePackage(UObject* InObject);

	TWeakPtr<SDMEditor> EditorWeak;
	TWeakObjectPtr<AActor> MaterialActorWeak;
	FDMOnActorMaterailSlotChanged OnSlotChanged;
	FOnGetContent OnGetSettingsMenu;

	TArray<TSharedPtr<FDMObjectMaterialProperty>> ActorMaterialProperties;
	int32 SelectedMaterialSlotIndex;

	TSharedPtr<SBox> SlotSelectorContainer;
	TSharedPtr<SWidget> SaveButtonWidget;
	TSharedPtr<SWidget> ActorRowWidget;
	TSharedPtr<SWidget> AssetRowWidget;
	TSharedPtr<STextBlock> ActorNameWidget;
	TSharedPtr<STextBlock> AssetNameWidget;
	TSharedPtr<SWidget> OpenParentButton;
	TSharedPtr<SWidget> ConvertToEditableButton;

	TSharedRef<SWidget> CreateToolBarEntries();

	TSharedRef<SWidget> CreateToolBarButton(TAttribute<const FSlateBrush*> InImageBrush, const TAttribute<FText>& InTooltipText, FOnClicked InOnClicked);
	TSharedRef<SWidget> CreateSlotsComboBoxWidget();

	TSharedRef<SWidget> GenerateSelectedMaterialSlotRow(TSharedPtr<FDMObjectMaterialProperty> InSelectedSlot) const;
	FText GetSlotDisplayName(TSharedPtr<FDMObjectMaterialProperty> InSlot) const;
	FText GetSelectedMaterialSlotName() const;
	void OnMaterialSlotChanged(TSharedPtr<FDMObjectMaterialProperty> InSelectedSlot, ESelectInfo::Type InSelectInfoType);

	const FMargin GetDefaultToolBarButtonContentPadding() const { return FMargin(2.0f); }
	const FVector2D GetDefaultToolBarButtonSize() const { return FVector2D(20.0f); }

	const FMargin GetLargeIconToolBarButtonContentPadding() const { return FMargin(4.0f); }
	const FVector2D GetLargeIconToolBarButtonSize() const { return FVector2D(16.0f); }

	const FSlateBrush* GetFollowSelectionBrush() const;
	FSlateColor GetFollowSelectionColor() const;
	FReply OnFollowSelectionButtonClicked();

	FReply OnExportMaterialInstanceButtonClicked();

	FReply OnBrowseClicked();

	FReply OnUseClicked();

	FText GetAssetName() const;

	FText GetAssetToolTip() const;

	bool CanSave() const;

	const FSlateBrush* GetSaveIcon() const;

	FReply OnSaveClicked();

	FReply OnOpenParentClicked();

	FReply OnConvertToEditableClicked();
};
