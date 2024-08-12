// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "Widgets/SCompoundWidget.h"

#include "DMObjectMaterialProperty.h"
#include "Misc/Optional.h"
#include "UI/Utils/DMWidgetSlot.h"

class FDMPreviewMaterialManager;
class FSlotBase;
class FUICommandList;
class SDMMaterialComponentEditor;
class SDMMaterialDesigner;
class SDMMaterialGlobalSettingsEditor;
class SDMMaterialPreview;
class SDMMaterialPropertySelector;
class SDMMaterialSlotEditor;
class SDMStatusBar;
class SDMToolBar;
class SSplitter;
class UDMMaterialComponent;
class UDMMaterialSlot;
class UDMTextureSet;
class UDynamicMaterialModelBase;
enum class EDMMaterialPropertyType : uint8;
enum class EDMUpdateType : uint8;

namespace UE::DynamicMaterialEditor::Private
{
	inline const TCHAR* EditorDarkBackground = TEXT("Brushes.Title");
	inline const TCHAR* EditorLightBackground = TEXT("Brushes.Header");
}

class SDMMaterialEditor : public SCompoundWidget, public FSelfRegisteringEditorUndoClient
{
	SLATE_DECLARE_WIDGET(SDMMaterialEditor, SCompoundWidget)

	SLATE_BEGIN_ARGS(SDMMaterialEditor)
		: _MaterialModelBase(nullptr)
		, _MaterialProperty(TOptional<FDMObjectMaterialProperty>())
		{}
		SLATE_ARGUMENT(UDynamicMaterialModelBase*, MaterialModelBase)
		SLATE_ARGUMENT(TOptional<FDMObjectMaterialProperty>, MaterialProperty)
	SLATE_END_ARGS()

public:
	SDMMaterialEditor();

	virtual ~SDMMaterialEditor() override;

	void Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialDesigner>& InDesignerWidget);

	TSharedPtr<SDMMaterialDesigner> GetDesignerWidget() const;

	/** Material Model / Actor */
	UDynamicMaterialModelBase* GetMaterialModelBase() const;

	UDynamicMaterialModel* GetMaterialModel() const;

	bool IsDynamicModel() const;

	const FDMObjectMaterialProperty* GetMaterialObjectProperty() const;

	AActor* GetMaterialActor() const;

	/** Widget Components */
	bool IsEditingGlobalSettings() const;

	const TSharedRef<FUICommandList>& GetCommandList() const;

	TSharedRef<FDMPreviewMaterialManager> GetPreviewMaterialManager() const;

	TSharedPtr<SDMMaterialSlotEditor> GetSlotEditorWidget() const;

	TSharedPtr<SDMMaterialComponentEditor> GetComponentEditorWidget() const;

	/** Actions */
	void SelectProperty(EDMMaterialPropertyType InProperty, bool bInForceRefresh = false);

	virtual void EditSlot(UDMMaterialSlot* InSlot, bool bInForceRefresh = false);

	virtual void EditComponent(UDMMaterialComponent* InComponent, bool bInForceRefresh = false);

	virtual void EditGlobalSettings(bool bInForceRefresh = false);

	void Validate();

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEditedSlotChanged, const TSharedRef<SDMMaterialSlotEditor>&, UDMMaterialSlot*);
	FOnEditedSlotChanged::RegistrationType& GetOnEditedSlotChanged();

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEditedComponentChanged, const TSharedRef<SDMMaterialComponentEditor>&, UDMMaterialComponent*);
	FOnEditedComponentChanged::RegistrationType& GetOnEditedComponentChanged();

	//~ Begin SWidget
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyDown(const FGeometry& InMyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget

	//~ Begin FUndoClient
	virtual void PostUndo(bool bInSuccess) override;
	virtual void PostRedo(bool bInSuccess) override;
	//~ End FUndoClient

protected:
	TWeakPtr<SDMMaterialDesigner> DesignerWidgetWeak;

	TDMWidgetSlot<SWidget> ContentSlot;
	TDMWidgetSlot<SDMToolBar> ToolBarSlot;
	TDMWidgetSlot<SWidget> MainSlot;
	TDMWidgetSlot<SDMMaterialPreview> MaterialPreviewSlot;
	TDMWidgetSlot<SDMMaterialPropertySelector> PropertySelectorSlot;
	TDMWidgetSlot<SDMMaterialGlobalSettingsEditor> GlobalSettingsEditorSlot;
	FSlotBase* SplitterSlot;
	TDMWidgetSlot<SDMMaterialSlotEditor> SlotEditorSlot;
	TDMWidgetSlot<SDMMaterialComponentEditor> ComponentEditorSlot;
	TDMWidgetSlot<SDMStatusBar> StatusBarSlot;

	TWeakObjectPtr<UDynamicMaterialModelBase> MaterialModelBaseWeak;
	TOptional<FDMObjectMaterialProperty> ObjectMaterialPropertyOpt;

	TSharedRef<FUICommandList> CommandList;
	TSharedRef<FDMPreviewMaterialManager> PreviewMaterialManager;

	TOptional<EDMMaterialPropertyType> PropertyToSelect;
	TWeakObjectPtr<UDMMaterialSlot> SlotToEdit;
	TWeakObjectPtr<UDMMaterialComponent> ComponentToEdit;
	bool bGlobalSettingsMode;

	FOnEditedSlotChanged OnEditedSlotChanged;
	FOnEditedComponentChanged OnEditedComponentChanged;

	/** Operations */
	void SetMaterialModelBase(UDynamicMaterialModelBase* InMaterialModelBase);

	void SetObjectMaterialProperty(const FDMObjectMaterialProperty& InObjectProperty);

	void SetMaterialActor(AActor* InActor);

	void BindCommands(SDMMaterialSlotEditor* InSlotEditor);

	bool IsPropertyValidForModel(EDMMaterialPropertyType InProperty) const;

	void Close();

	void ValidateSlots();

	virtual void ValidateSlots_Main() = 0;

	void ClearSlots();

	virtual void ClearSlots_Main() = 0;

	/** Slots */
	void CreateLayout();

	TSharedRef<SWidget> CreateSlot_Container();

	TSharedRef<SDMToolBar> CreateSlot_ToolBar();

	virtual TSharedRef<SWidget> CreateSlot_Main() = 0;

	TSharedRef<SDMMaterialGlobalSettingsEditor> CreateSlot_GlobalSettingsEditor();

	TSharedRef<SDMMaterialPreview> CreateSlot_Preview();

	TSharedRef<SDMMaterialPropertySelector> CreateSlot_PropertySelector();

	virtual TSharedRef<SDMMaterialPropertySelector> CreateSlot_PropertySelector_Impl() = 0;

	TSharedRef<SDMMaterialSlotEditor> CreateSlot_SlotEditor();

	TSharedRef<SDMMaterialComponentEditor> CreateSlot_ComponentEditor();

	TSharedRef<SDMStatusBar> CreateSlot_StatusBar();

	/** Events */
	void OnUndo();

	/** The material preview window is not cleaned up properly on uobject shutdown, so do it here. */
	void OnEnginePreExit();

	void OnEditorSplitterResized();
};
