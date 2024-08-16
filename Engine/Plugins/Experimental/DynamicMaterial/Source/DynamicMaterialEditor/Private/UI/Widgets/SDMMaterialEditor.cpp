// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/SDMMaterialEditor.h"

#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "DynamicMaterialEditorCommands.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialModule.h"
#include "Engine/Texture.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelBase.h"
#include "Model/DynamicMaterialModelDynamic.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Styling/SlateIconFinder.h"
#include "UI/Utils/DMPreviewMaterialManager.h"
#include "UI/Widgets/Editor/SDMMaterialComponentEditor.h"
#include "UI/Widgets/Editor/SDMMaterialGlobalSettingsEditor.h"
#include "UI/Widgets/Editor/SDMMaterialPreview.h"
#include "UI/Widgets/Editor/SDMMaterialPropertyPreviews.h"
#include "UI/Widgets/Editor/SDMMaterialPropertySelector.h"
#include "UI/Widgets/Editor/SDMMaterialSlotEditor.h"
#include "UI/Widgets/Editor/SDMStatusBar.h"
#include "UI/Widgets/Editor/SDMToolBar.h"
#include "UI/Widgets/SDMMaterialDesigner.h"
#include "Utils/DMMaterialModelFunctionLibrary.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SToolTip.h"

#define LOCTEXT_NAMESPACE "SDMMaterialEditor"

void SDMMaterialEditor::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

SDMMaterialEditor::SDMMaterialEditor()
	: CommandList(MakeShared<FUICommandList>())
	, PreviewMaterialManager(MakeShared<FDMPreviewMaterialManager>())
	, EditMode(EDMMaterialEditorMode::GlobalSettings)
{
}

SDMMaterialEditor::~SDMMaterialEditor()
{
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
	CloseMaterialPreviewTab();
	DestroyMaterialPreviewToolTip();

	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = EditorOnlyDataUpdateObject.Get())
	{
		EditorOnlyData->GetOnMaterialBuiltDelegate().RemoveAll(this);
		EditorOnlyData->GetOnPropertyUpdateDelegate().RemoveAll(this);
		EditorOnlyData->GetOnSlotListUpdateDelegate().RemoveAll(this);
	}
}

void SDMMaterialEditor::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialDesigner>& InDesignerWidget)
{
	DesignerWidgetWeak = InDesignerWidget;
	EditMode = EDMMaterialEditorMode::GlobalSettings;
	SelectedMaterialProperty = EDMMaterialPropertyType::None;

	SetCanTick(false);

	ContentSlot = TDMWidgetSlot<SWidget>(SharedThis(this), 0, SNullWidget::NullWidget);

	if (InArgs._MaterialProperty.IsSet())
	{
		SetObjectMaterialProperty(InArgs._MaterialProperty.GetValue());
	}
	else if (IsValid(InArgs._MaterialModelBase))
	{
		SetMaterialModelBase(InArgs._MaterialModelBase);
	}
	else
	{
		ensureMsgf(false, TEXT("No valid material model passed to Material DesignerWidget Editor."));
	}

	FCoreDelegates::OnEnginePreExit.AddSP(this, &SDMMaterialEditor::OnEnginePreExit);
}

TSharedPtr<SDMMaterialDesigner> SDMMaterialEditor::GetDesignerWidget() const
{
	return DesignerWidgetWeak.Pin();
}

UDynamicMaterialModelBase* SDMMaterialEditor::GetMaterialModelBase() const
{
	return MaterialModelBaseWeak.Get();
}

void SDMMaterialEditor::SetMaterialModelBase(UDynamicMaterialModelBase* InMaterialModelBase)
{
	MaterialModelBaseWeak = InMaterialModelBase;

	if (UDynamicMaterialModelDynamic* MaterialModelDynamic = Cast<UDynamicMaterialModelDynamic>(InMaterialModelBase))
	{
		MaterialModelDynamic->EnsureComponents();
	}

	EditGlobalSettings();

	CreateLayout();

	BindEditorOnlyDataUpdate(InMaterialModelBase);
}

UDynamicMaterialModel* SDMMaterialEditor::GetMaterialModel() const
{
	if (UDynamicMaterialModelBase* MaterialModelBase = MaterialModelBaseWeak.Get())
	{
		return MaterialModelBase->ResolveMaterialModel();
	}
	
	return nullptr;
}

bool SDMMaterialEditor::IsDynamicModel() const
{
	return !!Cast<UDynamicMaterialModelDynamic>(MaterialModelBaseWeak.Get());
}

const FDMObjectMaterialProperty* SDMMaterialEditor::GetMaterialObjectProperty() const
{
	if (ObjectMaterialPropertyOpt.IsSet())
	{
		return &ObjectMaterialPropertyOpt.GetValue();
	}

	return nullptr;
}

void SDMMaterialEditor::SetObjectMaterialProperty(const FDMObjectMaterialProperty& InObjectProperty)
{
	UDynamicMaterialModelBase* MaterialModelBase = InObjectProperty.GetMaterialModelBase();

	if (!ensureMsgf(MaterialModelBase, TEXT("Invalid object material property value.")))
	{
		ClearSlots();
		return;
	}

	ObjectMaterialPropertyOpt = InObjectProperty;
	SetMaterialModelBase(MaterialModelBase);

	BindEditorOnlyDataUpdate(MaterialModelBase);
}

AActor* SDMMaterialEditor::GetMaterialActor() const
{
	if (ObjectMaterialPropertyOpt.IsSet())
	{
		return ObjectMaterialPropertyOpt.GetValue().GetTypedOuter<AActor>();
	}

	return nullptr;
}

EDMMaterialEditorMode SDMMaterialEditor::GetEditMode() const
{
	return EditMode;
}

void SDMMaterialEditor::SetMaterialActor(AActor* InActor)
{
	if (GetMaterialActor() == InActor)
	{
		return;
	}

	TSharedRef<SDMToolBar> NewToolBar = SNew(SDMToolBar, SharedThis(this), InActor);

	ToolBarSlot << NewToolBar;
}

TSharedPtr<SDMMaterialSlotEditor> SDMMaterialEditor::GetSlotEditorWidget() const
{
	return &SlotEditorSlot;
}

TSharedPtr<SDMMaterialComponentEditor> SDMMaterialEditor::GetComponentEditorWidget() const
{
	return &ComponentEditorSlot;
}

UDMMaterialSlot* SDMMaterialEditor::GetSlotToEdit() const
{
	return SlotToEdit.Get();
}

UDMMaterialComponent* SDMMaterialEditor::GetComponentToEdit() const
{
	return ComponentToEdit.Get();
}

EDMMaterialPropertyType SDMMaterialEditor::GetSelectedPropertyType() const
{
	return SelectedMaterialProperty;
}

void SDMMaterialEditor::SelectProperty(EDMMaterialPropertyType InProperty, bool bInForceRefresh)
{
	if (EditMode == EDMMaterialEditorMode::EditSlot && SelectedMaterialProperty == InProperty && !bInForceRefresh)
	{
		return;
	}

	EditMode = EDMMaterialEditorMode::EditSlot;
	SelectedMaterialProperty = InProperty;

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelBaseWeak);

	if (!EditorOnlyData)
	{
		return;
	}

	UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForMaterialProperty(InProperty);

	if (!Slot)
	{
		return;
	}

	EditSlot(Slot);
}

const TSharedRef<FUICommandList>& SDMMaterialEditor::GetCommandList() const
{
	return CommandList;
}

TSharedRef<FDMPreviewMaterialManager> SDMMaterialEditor::GetPreviewMaterialManager() const
{
	return PreviewMaterialManager;
}

void SDMMaterialEditor::EditSlot(UDMMaterialSlot* InSlot, bool bInForceRefresh)
{
	if (!bInForceRefresh && SlotEditorSlot.IsValid() && SlotEditorSlot->GetSlot() == InSlot)
	{
		return;
	}

	SlotEditorSlot.Invalidate();
	SplitterSlot = nullptr;
	SlotToEdit = InSlot;

	ComponentEditorSlot.Invalidate();
	ComponentToEdit.Reset();

	EditMode = EDMMaterialEditorMode::EditSlot;

	if (InSlot)
	{
		for (const TObjectPtr<UDMMaterialLayerObject>& Layer : InSlot->GetLayers())
		{
			if (UDMMaterialStage* Stage = Layer->GetFirstValidStage(EDMMaterialLayerStage::All))
			{
				ComponentToEdit = Stage;
				break;
			}
		}
	}
}

void SDMMaterialEditor::EditComponent(UDMMaterialComponent* InComponent, bool bInForceRefresh)
{
	if (!bInForceRefresh && ComponentEditorSlot.IsValid() && ComponentEditorSlot->GetComponent() == InComponent)
	{
		return;
	}

	if (EditMode != EDMMaterialEditorMode::EditSlot)
	{
		SlotEditorSlot.Invalidate();
		SplitterSlot = nullptr;
		GlobalSettingsEditorSlot.Invalidate();
		MaterialPropertyPreviewsSlot.Invalidate();
	}

	EditMode = EDMMaterialEditorMode::EditSlot;

	ComponentEditorSlot.Invalidate();
	ComponentToEdit = InComponent;
}

void SDMMaterialEditor::EditGlobalSettings(bool bInForceRefresh)
{
	if (EditMode == EDMMaterialEditorMode::GlobalSettings && !bInForceRefresh)
	{
		return;
	}

	if (EditMode != EDMMaterialEditorMode::GlobalSettings)
	{
		SlotEditorSlot.Invalidate();
		SplitterSlot = nullptr;
		ComponentEditorSlot.Invalidate();
		MaterialPropertyPreviewsSlot.Invalidate();
	}

	EditMode = EDMMaterialEditorMode::GlobalSettings;
	SelectedMaterialProperty = EDMMaterialPropertyType::None;

	GlobalSettingsEditorSlot.Invalidate();
}

void SDMMaterialEditor::ShowPropertyPreviews(bool bInForceRefresh)
{
	if (EditMode == EDMMaterialEditorMode::PropertyPreviews && !bInForceRefresh)
	{
		return;
	}

	if (EditMode != EDMMaterialEditorMode::PropertyPreviews)
	{
		SlotEditorSlot.Invalidate();
		SplitterSlot = nullptr;
		ComponentEditorSlot.Invalidate();
		GlobalSettingsEditorSlot.Invalidate();
	}

	EditMode = EDMMaterialEditorMode::PropertyPreviews;
	SelectedMaterialProperty = EDMMaterialPropertyType::None;

	MaterialPropertyPreviewsSlot.Invalidate();
}

void SDMMaterialEditor::OpenMaterialPreviewTab()
{
	UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

	if (!MaterialModelBase)
	{
		return;
	}

	CloseMaterialPreviewTab();

	FSlateApplication::Get().CloseToolTip();

	const FName TabId = TEXT("MaterialPreviewTab");

	if (!FGlobalTabmanager::Get()->HasTabSpawner(TabId))
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			TabId,
			FOnSpawnTab::CreateLambda(
				[TabId](const FSpawnTabArgs& InArgs)
				{
					TSharedRef<SDockTab> DockTab = SNew(SDockTab)
						.Label(FText::FromName(TabId))
						.LabelSuffix(LOCTEXT("TabSuffix", "Material Preview"));

					DockTab->SetTabIcon(FSlateIconFinder::FindIconForClass(UMaterial::StaticClass()).GetIcon());

					return DockTab;
				}
			)
		);
	}

	MaterialPreviewTab = FGlobalTabmanager::Get()->TryInvokeTab(TabId);
	MaterialPreviewTab->ActivateInParent(ETabActivationCause::SetDirectly);
	MaterialPreviewTab->SetLabel(FText::FromString(MaterialModelBase->GetPathName()));
	MaterialPreviewTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateSPLambda(
		this,
		[this](TSharedRef<SDockTab> InDockTab)
		{
			MaterialPreviewTabSlot.ClearWidget();
		}));

	TSharedRef<SBox> Wrapper = SNew(SBox);

	MaterialPreviewTabSlot = TDMWidgetSlot<SDMMaterialPreview>(
		Wrapper, 
		0, 
		SNew(SDMMaterialPreview, SharedThis(this), MaterialModelBase)
	);

	MaterialPreviewTab->SetContent(Wrapper);
}

void SDMMaterialEditor::CloseMaterialPreviewTab()
{
	if (MaterialPreviewTab.IsValid())
	{
		MaterialPreviewTabSlot.ClearWidget();
		MaterialPreviewTab->RequestCloseTab();
		MaterialPreviewTab.Reset();
	}
}

TSharedPtr<IToolTip> SDMMaterialEditor::GetMaterialPreviewToolTip()
{
	UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

	if (!MaterialModelBase)
	{
		return nullptr;
	}

	UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();

	if (!Settings)
	{
		return nullptr;
	}

	DestroyMaterialPreviewToolTip();

	TSharedRef<SBox> Wrapper = SNew(SBox)
		.WidthOverride(TAttribute<FOptionalSize>::CreateWeakLambda(
			Settings,
			[Settings]
			{
				return Settings->ThumbnailSize;
			}
		))
		.HeightOverride(TAttribute<FOptionalSize>::CreateWeakLambda(
			Settings,
			[Settings]
			{
				return Settings->ThumbnailSize;
			}
		));

	MaterialPreviewToolTipSlot = TDMWidgetSlot<SDMMaterialPreview>(
		Wrapper,
		0,
		SNew(SDMMaterialPreview, SharedThis(this), MaterialModelBase)
		.ShowMenu(false)
	);

	MaterialPreviewToolTip = SNew(SToolTip)
		.IsInteractive(false)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolTip.Background"))
		[
			Wrapper
		];

	return MaterialPreviewToolTip.ToSharedRef();
}

void SDMMaterialEditor::DestroyMaterialPreviewToolTip()
{
	if (MaterialPreviewToolTip.IsValid())
	{
		MaterialPreviewToolTipSlot.ClearWidget();
		MaterialPreviewToolTip.Reset();
	}
}

void SDMMaterialEditor::Validate()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

	if (!IsValid(MaterialModelBase))
	{
		Close();
		return;
	}

	if (ObjectMaterialPropertyOpt.IsSet() && ObjectMaterialPropertyOpt->IsValid())
	{
		const FDMObjectMaterialProperty& ObjectMaterialProperty = ObjectMaterialPropertyOpt.GetValue();
		UDynamicMaterialModelBase* MaterialModelBaseFromProperty = ObjectMaterialProperty.GetMaterialModelBase();

		if (!UDMMaterialModelFunctionLibrary::IsModelValid(MaterialModelBaseFromProperty))
		{
			MaterialModelBase = nullptr;
		}

		if (MaterialModelBase != MaterialModelBaseFromProperty)
		{
			if (TSharedPtr<SDMMaterialDesigner> DesignerWidget = DesignerWidgetWeak.Pin())
			{
				DesignerWidget->OpenObjectMaterialProperty(ObjectMaterialProperty);
				return;
			}
		}
	}
	else if (!UDMMaterialModelFunctionLibrary::IsModelValid(MaterialModelBase))
	{
		Close();
		return;
	}

	ValidateSlots();
}

SDMMaterialEditor::FOnEditedSlotChanged::RegistrationType& SDMMaterialEditor::GetOnEditedSlotChanged()
{
	return OnEditedSlotChanged;
}

SDMMaterialEditor::FOnEditedComponentChanged::RegistrationType& SDMMaterialEditor::GetOnEditedComponentChanged()
{
	return OnEditedComponentChanged;
}

bool SDMMaterialEditor::SupportsKeyboardFocus() const
{
	return true;
}

FReply SDMMaterialEditor::OnKeyDown(const FGeometry& InMyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	// We accept the delete key bind, so we don't want this accidentally deleting actors and such.
	// Always return handled to stop the event bubbling.
	const TArray<TSharedRef<const FInputChord>> DeleteChords = {
		FGenericCommands::Get().Delete->GetActiveChord(EMultipleKeyBindingIndex::Primary),
		FGenericCommands::Get().Delete->GetActiveChord(EMultipleKeyBindingIndex::Secondary)
	};

	for (const TSharedRef<const FInputChord>& DeleteChord : DeleteChords)
	{
		if (DeleteChord->Key == InKeyEvent.GetKey())
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SDMMaterialEditor::PostUndo(bool bInSuccess)
{
	OnUndo();
}

void SDMMaterialEditor::PostRedo(bool bInSuccess)
{
	OnUndo();
}

void SDMMaterialEditor::BindCommands(SDMMaterialSlotEditor* InSlotEditor)
{
	const FGenericCommands& GenericCommands = FGenericCommands::Get();

	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(
		FDynamicMaterialEditorCommands::Get().AddDefaultLayer,
		FExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::AddNewLayer),
		FCanExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::CanAddNewLayer)
	);

	CommandList->MapAction(
		FDynamicMaterialEditorCommands::Get().InsertDefaultLayerAbove,
		FExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::InsertNewLayer),
		FCanExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::CanInsertNewLayer)
	);

	CommandList->MapAction(
		GenericCommands.Copy,
		FExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::CopySelectedLayer),
		FCanExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::CanCopySelectedLayer)
	);

	CommandList->MapAction(
		GenericCommands.Cut,
		FExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::CutSelectedLayer),
		FCanExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::CanCutSelectedLayer)
	);

	CommandList->MapAction(
		GenericCommands.Paste,
		FExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::PasteLayer),
		FCanExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::CanPasteLayer)
	);

	CommandList->MapAction(
		GenericCommands.Duplicate,
		FExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::DuplicateSelectedLayer),
		FCanExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::CanDuplicateSelectedLayer)
	);

	CommandList->MapAction(
		GenericCommands.Delete,
		FExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::DeleteSelectedLayer),
		FCanExecuteAction::CreateSP(InSlotEditor, &SDMMaterialSlotEditor::CanDeleteSelectedLayer)
	);
}

bool SDMMaterialEditor::IsPropertyValidForModel(EDMMaterialPropertyType InProperty) const
{
	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelBaseWeak);

	if (!EditorOnlyData)
	{
		return false;
	}

	if (UDMMaterialProperty* Property = EditorOnlyData->GetMaterialProperty(InProperty))
	{
		if (Property->IsValidForModel(*EditorOnlyData))
		{
			return true;
		}
	}

	if (InProperty == EDMMaterialPropertyType::Opacity)
	{
		if (UDMMaterialProperty* Property = EditorOnlyData->GetMaterialProperty(EDMMaterialPropertyType::OpacityMask))
		{
			return Property->IsValidForModel(*EditorOnlyData);
		}
	}

	return false;
}

void SDMMaterialEditor::Close()
{
	if (TSharedPtr<SDMMaterialDesigner> DesignerWidget = DesignerWidgetWeak.Pin())
	{
		DesignerWidget->ShowSelectPrompt();
	}
}

void SDMMaterialEditor::ValidateSlots()
{
	if (ContentSlot.HasBeenInvalidated())
	{
		CreateLayout();
		return;
	}

	if (ToolBarSlot.HasBeenInvalidated())
	{
		ToolBarSlot << CreateSlot_ToolBar();
	}

	if (MainSlot.HasBeenInvalidated())
	{
		MainSlot << CreateSlot_Main();
	}
	else
	{
		ValidateSlots_Main();

		if (MaterialPreviewSlot.HasBeenInvalidated())
		{
			MaterialPreviewSlot << CreateSlot_Preview();
		}

		if (PropertySelectorSlot.HasBeenInvalidated())
		{
			PropertySelectorSlot << CreateSlot_PropertySelector();
		}

		if (EditMode == EDMMaterialEditorMode::GlobalSettings)
		{
			if (GlobalSettingsEditorSlot.HasBeenInvalidated())
			{
				GlobalSettingsEditorSlot << CreateSlot_GlobalSettingsEditor();
			}
			else
			{
				GlobalSettingsEditorSlot->Validate();
			}
		}
		else if (EditMode == EDMMaterialEditorMode::PropertyPreviews)
		{
			if (MaterialPropertyPreviewsSlot.HasBeenInvalidated())
			{
				MaterialPropertyPreviewsSlot << CreateSlot_MaterialPropertyPreviews();
			}
			else
			{
				MaterialPropertyPreviewsSlot->Validate();
			}
		}
		else
		{
			if (SlotEditorSlot.HasBeenInvalidated())
			{
				SlotEditorSlot << CreateSlot_SlotEditor();
			}
			else
			{
				SlotEditorSlot->ValidateSlots();
			}

			if (ComponentEditorSlot.HasBeenInvalidated())
			{
				ComponentEditorSlot << CreateSlot_ComponentEditor();
			}
			else
			{
				ComponentEditorSlot->Validate();
			}
		}
	}

	if (StatusBarSlot.HasBeenInvalidated())
	{
		StatusBarSlot << CreateSlot_StatusBar();
	}
}

void SDMMaterialEditor::ClearSlots()
{
	ContentSlot.ClearWidget();
	ToolBarSlot.ClearWidget();
	MainSlot.ClearWidget();
	SlotEditorSlot.ClearWidget();
	MaterialPreviewSlot.ClearWidget();
	PropertySelectorSlot.ClearWidget();
	GlobalSettingsEditorSlot.ClearWidget();
	SplitterSlot = nullptr;
	ComponentEditorSlot.ClearWidget();
	StatusBarSlot.ClearWidget();

	ClearSlots_Main();
}

void SDMMaterialEditor::CreateLayout()
{
	ContentSlot << CreateSlot_Container();
}

TSharedRef<SWidget> SDMMaterialEditor::CreateSlot_Container()
{
	SVerticalBox::FSlot* ToolBarSlotPtr = nullptr;
	SVerticalBox::FSlot* MainSlotPtr = nullptr;
	SVerticalBox::FSlot* StatusBarSlotPtr = nullptr;

	TSharedRef<SVerticalBox> NewContainer = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Expose(ToolBarSlotPtr)
		.AutoHeight()
		[
			SNullWidget::NullWidget
		]

		+ SVerticalBox::Slot()
		.Expose(MainSlotPtr)
		.FillHeight(1.0f)
		[
			SNullWidget::NullWidget
		]

		+ SVerticalBox::Slot()
		.Expose(StatusBarSlotPtr)
		.AutoHeight()
		[
			SNullWidget::NullWidget
		];

	ToolBarSlot = TDMWidgetSlot<SDMToolBar>(ToolBarSlotPtr, CreateSlot_ToolBar());
	MainSlot = TDMWidgetSlot<SWidget>(MainSlotPtr, CreateSlot_Main());
	StatusBarSlot = TDMWidgetSlot<SDMStatusBar>(StatusBarSlotPtr, CreateSlot_StatusBar());

	return NewContainer;
}

TSharedRef<SDMToolBar> SDMMaterialEditor::CreateSlot_ToolBar()
{
	return SNew(
		SDMToolBar, 
		SharedThis(this), 
		ObjectMaterialPropertyOpt.IsSet()
			? ObjectMaterialPropertyOpt->GetTypedOuter<AActor>()
			: nullptr
	);
}

TSharedRef<SDMMaterialGlobalSettingsEditor> SDMMaterialEditor::CreateSlot_GlobalSettingsEditor()
{
	return SNew(SDMMaterialGlobalSettingsEditor, SharedThis(this), GetMaterialModelBase());
}

TSharedRef<SDMMaterialPropertyPreviews> SDMMaterialEditor::CreateSlot_MaterialPropertyPreviews()
{
	return SNew(SDMMaterialPropertyPreviews, SharedThis(this));
}

TSharedRef<SDMMaterialPreview> SDMMaterialEditor::CreateSlot_Preview()
{
	return SNew(SDMMaterialPreview, SharedThis(this), GetMaterialModelBase());
}

TSharedRef<SDMMaterialPropertySelector> SDMMaterialEditor::CreateSlot_PropertySelector()
{
	TSharedRef<SDMMaterialPropertySelector> NewPropertySelector = CreateSlot_PropertySelector_Impl();

	if (EditMode == EDMMaterialEditorMode::EditSlot && SelectedMaterialProperty != EDMMaterialPropertyType::None)
	{
		if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
		{
			if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
			{
				for (const TPair<EDMMaterialPropertyType, UDMMaterialProperty*>& PropertyPair : EditorOnlyData->GetMaterialProperties())
				{
					if (PropertyPair.Value->IsEnabled() && PropertyPair.Value->IsValidForModel(*EditorOnlyData))
					{
						SelectedMaterialProperty = PropertyPair.Key;
						break;
					}
				}
			}
		}
	}

	return NewPropertySelector;
}

TSharedRef<SDMMaterialSlotEditor> SDMMaterialEditor::CreateSlot_SlotEditor()
{
	UDMMaterialSlot* Slot = SlotToEdit.Get();
	SlotToEdit.Reset();

	TSharedRef<SDMMaterialSlotEditor> NewSlotEditor = SNew(SDMMaterialSlotEditor, SharedThis(this), Slot);

	BindCommands(&*NewSlotEditor);

	OnEditedSlotChanged.Broadcast(NewSlotEditor, Slot);

	return NewSlotEditor;
}

TSharedRef<SDMMaterialComponentEditor> SDMMaterialEditor::CreateSlot_ComponentEditor()
{
	UDMMaterialComponent* Component = ComponentToEdit.Get();
	ComponentToEdit.Reset();

	TSharedRef<SDMMaterialComponentEditor> NewComponentEditor = SNew(SDMMaterialComponentEditor, SharedThis(this), Component);

	OnEditedComponentChanged.Broadcast(NewComponentEditor, Component);

	return NewComponentEditor;
}

TSharedRef<SDMStatusBar> SDMMaterialEditor::CreateSlot_StatusBar()
{
	return SNew(SDMStatusBar, SharedThis(this), GetMaterialModelBase());
}

void SDMMaterialEditor::OnUndo()
{
	UDynamicMaterialModelBase* MaterialModel = GetMaterialModelBase();

	if (!IsValid(MaterialModel))
	{
		Close();
		return;
	}

	if (EditMode == EDMMaterialEditorMode::EditSlot)
	{
		if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelBaseWeak))
		{
			for (const TPair<EDMMaterialPropertyType, UDMMaterialProperty*>& PropertyPair : EditorOnlyData->GetMaterialProperties())
			{
				if (PropertyPair.Value->IsEnabled() && PropertyPair.Value->IsValidForModel(*EditorOnlyData))
				{
					SelectProperty(PropertyPair.Key);
					break;
				}
			}
		}
	}
}

void SDMMaterialEditor::OnEnginePreExit()
{
	MaterialPreviewSlot.ClearWidget();
	CloseMaterialPreviewTab();
	DestroyMaterialPreviewToolTip();
}

void SDMMaterialEditor::OnEditorSplitterResized()
{
	UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();

	if (SplitterSlot)
	{
		const float SplitterLocation = static_cast<SSplitter::FSlot*>(SplitterSlot)->GetSizeValue();
		Settings->SplitterLocation = SplitterLocation;
		Settings->SaveConfig();
	}
}

void SDMMaterialEditor::BindEditorOnlyDataUpdate(UDynamicMaterialModelBase* InMaterialModelBase)
{
	if (UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(InMaterialModelBase))
	{
		if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
		{
			EditorOnlyDataUpdateObject = EditorOnlyData;
			EditorOnlyData->GetOnMaterialBuiltDelegate().AddSP(this, &SDMMaterialEditor::OnMaterialBuilt);
			EditorOnlyData->GetOnPropertyUpdateDelegate().AddSP(this, &SDMMaterialEditor::OnPropertyUpdate);
			EditorOnlyData->GetOnSlotListUpdateDelegate().AddSP(this, &SDMMaterialEditor::OnSlotListUpdate);
		}
	}
}

void SDMMaterialEditor::OnMaterialBuilt(UDynamicMaterialModelBase* InMaterialModelBase)
{
	PropertySelectorSlot.Invalidate();
}

void SDMMaterialEditor::OnPropertyUpdate(UDynamicMaterialModelBase* InMaterialModelBase)
{
	PropertySelectorSlot.Invalidate();
}

void SDMMaterialEditor::OnSlotListUpdate(UDynamicMaterialModelBase* InMaterialModelBase)
{
	PropertySelectorSlot.Invalidate();
}

#undef LOCTEXT_NAMESPACE
