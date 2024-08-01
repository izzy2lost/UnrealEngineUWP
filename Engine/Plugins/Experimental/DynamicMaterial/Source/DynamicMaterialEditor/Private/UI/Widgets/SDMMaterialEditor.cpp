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
#include "Framework/Commands/GenericCommands.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Model/DynamicMaterialModelBase.h"
#include "Model/DynamicMaterialModelDynamic.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "UI/Utils/DMPreviewMaterialManager.h"
#include "UI/Widgets/Editor/SDMMaterialComponentEditor.h"
#include "UI/Widgets/Editor/SDMMaterialGlobalSettingsEditor.h"
#include "UI/Widgets/Editor/SDMMaterialPreview.h"
#include "UI/Widgets/Editor/SDMMaterialPropertySelector.h"
#include "UI/Widgets/Editor/SDMMaterialSlotEditor.h"
#include "UI/Widgets/Editor/SDMStatusBar.h"
#include "UI/Widgets/Editor/SDMToolBar.h"
#include "UI/Widgets/SDMMaterialDesigner.h"
#include "Utils/DMMaterialModelFunctionLibrary.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SDMMaterialEditor"

namespace UE::DynamicMaterialEditor::Private
{
	namespace SlotList
	{
		constexpr int32 ToolBar = 0;
		constexpr int32 MainLayout = 1;
		constexpr int32 StatusBar = 2;

		namespace Main
		{
			constexpr int32 Left = 0;
			constexpr int32 Right = 1;
		}

		namespace Left
		{
			constexpr int32 Preview = 0;
			constexpr int32 PropertySelector = 1;
		}

		namespace Right
		{
			constexpr int32 SlotEditor = 0;
			constexpr int32 ComponentEditor = 1;
		}
	}
}

void SDMMaterialEditor::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

SDMMaterialEditor::SDMMaterialEditor()
	: CommandList(MakeShared<FUICommandList>())
	, PreviewMaterialManager(MakeShared<FDMPreviewMaterialManager>())
	, bGlobalSettingsMode(true)
{
}

SDMMaterialEditor::~SDMMaterialEditor()
{
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
}

void SDMMaterialEditor::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialDesigner>& InDesignerWidget)
{
	DesignerWidgetWeak = InDesignerWidget;
	bGlobalSettingsMode = true;
	PropertyToSelect.Reset();

	Container = TDMWidgetSlot<SWidget>(SharedThis(this), 0, SNullWidget::NullWidget);

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
}

AActor* SDMMaterialEditor::GetMaterialActor() const
{
	if (ObjectMaterialPropertyOpt.IsSet())
	{
		return ObjectMaterialPropertyOpt.GetValue().GetTypedOuter<AActor>();
	}

	return nullptr;
}

bool SDMMaterialEditor::IsEditingGlobalSettings() const
{
	return bGlobalSettingsMode;
}

void SDMMaterialEditor::SetMaterialActor(AActor* InActor)
{
	if (GetMaterialActor() == InActor)
	{
		return;
	}

	TSharedRef<SDMToolBar> NewToolBar = SNew(SDMToolBar, SharedThis(this), InActor);

	ToolBar << NewToolBar;
}

TSharedPtr<SDMMaterialSlotEditor> SDMMaterialEditor::GetSlotEditorWidget() const
{
	return &SlotEditor;
}

TSharedPtr<SDMMaterialComponentEditor> SDMMaterialEditor::GetComponentEditorWidget() const
{
	return &ComponentEditor;
}

void SDMMaterialEditor::SelectProperty(EDMMaterialPropertyType InProperty, bool bInForceRefresh)
{
	if (bInForceRefresh || !PropertySelector.IsValid())
	{
		PropertyToSelect = InProperty;
		PropertySelector.Invalidate();
		return;
	}

	if (PropertySelector->GetSelectedProperty() != InProperty)
	{
		PropertySelector->SetSelectedProperty(InProperty);
	}
	
	PropertyToSelect.Reset();
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
	if (!bInForceRefresh && SlotEditor.IsValid() && SlotEditor->GetSlot() == InSlot)
	{
		return;
	}

	Right.Invalidate();

	SlotEditor.Invalidate();
	SlotToEdit = InSlot;

	ComponentEditor.Invalidate();
	ComponentToEdit.Reset();

	bGlobalSettingsMode = !InSlot;

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
	if (!bInForceRefresh && ComponentEditor.IsValid() && ComponentEditor->GetComponent() == InComponent)
	{
		return;
	}

	if (bGlobalSettingsMode)
	{
		Right.Invalidate();
		SlotEditor.Invalidate();
	}

	bGlobalSettingsMode = false;

	ComponentEditor.Invalidate();
	ComponentToEdit = InComponent;
}

void SDMMaterialEditor::EditGlobalSettings(bool bInForceRefresh)
{
	if (bGlobalSettingsMode && !bInForceRefresh)
	{
		return;
	}

	if (!bGlobalSettingsMode)
	{
		Right.Invalidate();
		SlotEditor.Invalidate();
		ComponentEditor.Invalidate();
	}

	bGlobalSettingsMode = true;

	GlobalSettingsEditor.Invalidate();
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

void SDMMaterialEditor::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

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
	if (Container.HasBeenInvalidated())
	{
		CreateLayout();
		return;
	}

	if (ToolBar.HasBeenInvalidated())
	{
		ToolBar << CreateSlot_ToolBar();
	}

	if (Main.HasBeenInvalidated())
	{
		Main << CreateSlot_Main();
	}
	else
	{
		if (Left.HasBeenInvalidated())
		{
			Left << CreateSlot_Left();
		}
		else
		{
			if (Preview.HasBeenInvalidated())
			{
				Preview << CreateSlot_Preview();
			}

			if (PropertySelector.HasBeenInvalidated())
			{
				PropertySelector << CreateSlot_PropertySelector();
			}
		}

		if (Right.HasBeenInvalidated())
		{
			Right << CreateSlot_Right();
		}
		else if (bGlobalSettingsMode)
		{
			if (GlobalSettingsEditor.HasBeenInvalidated())
			{
				GlobalSettingsEditor << CreateSlot_GlobalSettingsEditor();
			}
			else
			{
				GlobalSettingsEditor->Validate();
			}
		}
		else
		{
			if (SlotEditor.HasBeenInvalidated())
			{
				SlotEditor << CreateSlot_SlotEditor();
			}
			else
			{
				SlotEditor->ValidateSlots();
			}

			if (ComponentEditor.HasBeenInvalidated())
			{
				ComponentEditor << CreateSlot_ComponentEditor();
			}
			else
			{
				ComponentEditor->Validate();
			}
		}
	}

	if (StatusBar.HasBeenInvalidated())
	{
		StatusBar << CreateSlot_StatusBar();
	}
}

void SDMMaterialEditor::ClearSlots()
{
	Container.ClearWidget();
	ToolBar.ClearWidget();
	Main.ClearWidget();
	Left.ClearWidget();
	Right.ClearWidget();
	Preview.ClearWidget();
	PropertySelector.ClearWidget();
	SlotEditor.ClearWidget();
	ComponentEditor.ClearWidget();
	StatusBar.ClearWidget();
}

void SDMMaterialEditor::CreateLayout()
{
	Container << CreateSlot_Container();
}

TSharedRef<SWidget> SDMMaterialEditor::CreateSlot_Container()
{
	SVerticalBox::FSlot* ToolBarSlot = nullptr;
	SVerticalBox::FSlot* MainSlot = nullptr;
	SVerticalBox::FSlot* StatusBarSlot = nullptr;

	TSharedRef<SVerticalBox> NewContainer = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Expose(ToolBarSlot)
		.AutoHeight()
		[
			SNullWidget::NullWidget
		]

		+ SVerticalBox::Slot()
		.Expose(MainSlot)
		.FillHeight(1.0f)
		[
			SNullWidget::NullWidget
		]

		+ SVerticalBox::Slot()
		.Expose(StatusBarSlot)
		.AutoHeight()
		[
			SNullWidget::NullWidget
		];

	ToolBar = TDMWidgetSlot<SDMToolBar>(ToolBarSlot, CreateSlot_ToolBar());
	Main = TDMWidgetSlot<SWidget>(MainSlot, CreateSlot_Main());
	StatusBar = TDMWidgetSlot<SDMStatusBar>(StatusBarSlot, CreateSlot_StatusBar());

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

TSharedRef<SWidget> SDMMaterialEditor::CreateSlot_Main()
{
	SHorizontalBox::FSlot* LeftSlot = nullptr;
	SHorizontalBox::FSlot* RightSlot = nullptr;

	TSharedRef<SHorizontalBox> NewMain = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Expose(LeftSlot)
		.AutoWidth()
		[
			SNullWidget::NullWidget
		]

		+ SHorizontalBox::Slot()
		.Expose(RightSlot)
		.FillWidth(1.0f)
		[
			SNullWidget::NullWidget
		];

	Left = TDMWidgetSlot<SWidget>(LeftSlot, CreateSlot_Left());
	Right = TDMWidgetSlot<SWidget>(RightSlot, CreateSlot_Right());

	return NewMain;
}

TSharedRef<SWidget> SDMMaterialEditor::CreateSlot_Left()
{
	SVerticalBox::FSlot* PreviewSlot = nullptr;
	SVerticalBox::FSlot* PropertySelectorSlot = nullptr;

	TSharedRef<SVerticalBox> NewLeft = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Expose(PreviewSlot)
		.AutoHeight()
		.Padding(5.f)
		[
			SNullWidget::NullWidget
		]

		+ SVerticalBox::Slot()
		.Expose(PropertySelectorSlot)
		.FillHeight(1.0f)
		.Padding(5.f, 0.f, 5.f, 5.f)
		[
			SNullWidget::NullWidget
		];

	Preview = TDMWidgetSlot<SDMMaterialPreview>(PreviewSlot, CreateSlot_Preview());
	PropertySelector = TDMWidgetSlot<SDMMaterialPropertySelector>(PropertySelectorSlot, CreateSlot_PropertySelector());

	return NewLeft;
}

TSharedRef<SWidget> SDMMaterialEditor::CreateSlot_Right()
{
	const bool bHasSlotToEdit = SlotToEdit.IsValid();

	if (!bGlobalSettingsMode && !bHasSlotToEdit)
	{
		bGlobalSettingsMode = true;
	}
	else if (bHasSlotToEdit)
	{
		bGlobalSettingsMode = false;
	}

	return bGlobalSettingsMode
		? CreateSlot_Right_GlobalSettings()
		: CreateSlot_Right_Slot();
}

TSharedRef<SWidget> SDMMaterialEditor::CreateSlot_Right_GlobalSettings()
{
	SVerticalBox::FSlot* GlobalSettingsSlot = nullptr;

	TSharedRef<SVerticalBox> NewRight = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Expose(GlobalSettingsSlot)
		.AutoHeight()
		[
			SNullWidget::NullWidget
		];

	GlobalSettingsEditor = TDMWidgetSlot<SDMMaterialGlobalSettingsEditor>(GlobalSettingsSlot, CreateSlot_GlobalSettingsEditor());

	return NewRight;
}

TSharedRef<SDMMaterialGlobalSettingsEditor> SDMMaterialEditor::CreateSlot_GlobalSettingsEditor()
{
	return SNew(SDMMaterialGlobalSettingsEditor, SharedThis(this), GetMaterialModelBase());
}

TSharedRef<SWidget> SDMMaterialEditor::CreateSlot_Right_Slot()
{
	float SplitterValue = 0.5;

	if (UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get())
	{
		SplitterValue = Settings->SplitterLocation;
	}

	SSplitter::FSlot* SlotEditorSlot = nullptr;
	SSplitter::FSlot* ComponentEditorSlot = nullptr;

	TSharedRef<SSplitter> NewRight = SNew(SSplitter)
		.Style(FAppStyle::Get(), "DetailsView.Splitter")
		.Orientation(Orient_Vertical)
		.ResizeMode(ESplitterResizeMode::Fill)
		.PhysicalSplitterHandleSize(3.0f)
		.HitDetectionSplitterHandleSize(4.0f)
		.OnSplitterFinishedResizing(this, &SDMMaterialEditor::OnRightSlotSplitterResized)

		+ SSplitter::Slot()
		.Expose(SlotEditorSlot)
		.Resizable(true)
		.SizeRule(SSplitter::ESizeRule::FractionOfParent)
		.MinSize(50)
		.Value(SplitterValue)
		[
			SNullWidget::NullWidget
		]

		+ SSplitter::Slot()
		.Expose(ComponentEditorSlot)
		.Resizable(true)
		.SizeRule(SSplitter::ESizeRule::FractionOfParent)
		.MinSize(50)
		.Value(1.f - SplitterValue)
		[
			SNullWidget::NullWidget
		];

	SlotEditor = TDMWidgetSlot<SDMMaterialSlotEditor>(SlotEditorSlot, CreateSlot_SlotEditor());
	ComponentEditor = TDMWidgetSlot<SDMMaterialComponentEditor>(ComponentEditorSlot, CreateSlot_ComponentEditor());

	return NewRight;
}

TSharedRef<SDMMaterialPreview> SDMMaterialEditor::CreateSlot_Preview()
{
	return SNew(SDMMaterialPreview, SharedThis(this), GetMaterialModelBase());
}

TSharedRef<SDMMaterialPropertySelector> SDMMaterialEditor::CreateSlot_PropertySelector()
{
	TSharedRef<SDMMaterialPropertySelector> NewPropertySelector = SNew(SDMMaterialPropertySelector, SharedThis(this));

	if (!PropertyToSelect.IsSet())
	{
		if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
		{
			if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
			{
				for (const TPair<EDMMaterialPropertyType, UDMMaterialProperty*>& PropertyPair : EditorOnlyData->GetMaterialProperties())
				{
					if (PropertyPair.Value->IsEnabled() && PropertyPair.Value->IsValidForModel(*EditorOnlyData))
					{
						PropertyToSelect = PropertyPair.Key;
						break;
					}
				}
			}
		}
	}

	if (PropertyToSelect.IsSet())
	{
		NewPropertySelector->SetSelectedProperty(PropertyToSelect.GetValue());
		PropertyToSelect.Reset();
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

	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelBaseWeak))
	{
		for (const TPair<EDMMaterialPropertyType, UDMMaterialProperty*>& PropertyPair : ModelEditorOnlyData->GetMaterialProperties())
		{
			if (PropertyPair.Value->IsEnabled())
			{
				PropertySelector->SetSelectedProperty(PropertyPair.Key);
			}
		}
	}
}

void SDMMaterialEditor::OnEnginePreExit()
{
	Preview.ClearWidget();
}

void SDMMaterialEditor::OnRightSlotSplitterResized()
{
	UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();

	if (SlotEditor.GetSlot())
	{
		Settings->SplitterLocation = static_cast<SSplitter::FSlot*>(SlotEditor.GetSlot())->GetSizeValue();
		Settings->SaveConfig();
	}
}

#undef LOCTEXT_NAMESPACE
