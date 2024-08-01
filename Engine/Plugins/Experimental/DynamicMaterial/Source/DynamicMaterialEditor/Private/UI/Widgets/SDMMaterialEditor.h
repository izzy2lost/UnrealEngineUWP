// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "Widgets/SCompoundWidget.h"

#include "DMObjectMaterialProperty.h"
#include "Misc/Optional.h"
#include "UI/Utils/DMWidgetSlot.h"

class FDMPreviewMaterialManager;
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

	void EditSlot(UDMMaterialSlot* InSlot, bool bInForceRefresh = false);

	void EditComponent(UDMMaterialComponent* InComponent, bool bInForceRefresh = false);

	void EditGlobalSettings(bool bInForceRefresh = false);

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEditedSlotChanged, const TSharedRef<SDMMaterialSlotEditor>&, UDMMaterialSlot*);
	FOnEditedSlotChanged::RegistrationType& GetOnEditedSlotChanged();

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEditedComponentChanged, const TSharedRef<SDMMaterialComponentEditor>&, UDMMaterialComponent*);
	FOnEditedComponentChanged::RegistrationType& GetOnEditedComponentChanged();

	//~ Begin SWidget
	virtual bool SupportsKeyboardFocus() const override;
	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnKeyDown(const FGeometry& InMyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget

	//~ Begin FUndoClient
	virtual void PostUndo(bool bInSuccess) override;
	virtual void PostRedo(bool bInSuccess) override;
	//~ End FUndoClient

protected:
	TWeakPtr<SDMMaterialDesigner> DesignerWidgetWeak;

	TDMWidgetSlot<SWidget> Container;
	TDMWidgetSlot<SDMToolBar> ToolBar;
	TDMWidgetSlot<SWidget> Main;
	TDMWidgetSlot<SWidget> Left;
	TDMWidgetSlot<SWidget> Right;
	TDMWidgetSlot<SDMMaterialPreview> Preview;
	TDMWidgetSlot<SDMMaterialPropertySelector> PropertySelector;
	TDMWidgetSlot<SDMMaterialGlobalSettingsEditor> GlobalSettingsEditor;
	TDMWidgetSlot<SDMMaterialSlotEditor> SlotEditor;
	TDMWidgetSlot<SDMMaterialComponentEditor> ComponentEditor;
	TDMWidgetSlot<SDMStatusBar> StatusBar;

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

	void ClearSlots();

	/** Slots */
	void CreateLayout();

	TSharedRef<SWidget> CreateSlot_Container();

	TSharedRef<SDMToolBar> CreateSlot_ToolBar();

	TSharedRef<SWidget> CreateSlot_Main();

	TSharedRef<SWidget> CreateSlot_Left();

	TSharedRef<SWidget> CreateSlot_Right();

	TSharedRef<SWidget> CreateSlot_Right_GlobalSettings();

	TSharedRef<SDMMaterialGlobalSettingsEditor> CreateSlot_GlobalSettingsEditor();

	TSharedRef<SWidget> CreateSlot_Right_Slot();

	TSharedRef<SDMMaterialPreview> CreateSlot_Preview();

	TSharedRef<SDMMaterialPropertySelector> CreateSlot_PropertySelector();

	TSharedRef<SDMMaterialSlotEditor> CreateSlot_SlotEditor();

	TSharedRef<SDMMaterialComponentEditor> CreateSlot_ComponentEditor();

	TSharedRef<SDMStatusBar> CreateSlot_StatusBar();

	/** Events */
	void OnUndo();

	/** The material preview window is not cleaned up properly on uobject shutdown, so do it here. */
	void OnEnginePreExit();

	void OnRightSlotSplitterResized();
};
