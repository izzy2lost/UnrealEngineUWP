// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMEditor.h"
#include "AssetThumbnail.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "Components/PrimitiveComponent.h"
#include "DetailLayoutBuilder.h"
#include "DMBlueprintFunctionLibrary.h"
#include "DMPrivate.h"
#include "DMWorldSubsystem.h"
#include "DynamicMaterialEditorCommands.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Engine/World.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/InputChord.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "MaterialDomain.h"
#include "Menus/DMToolBarMenus.h"
#include "Misc/CoreDelegates.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "PropertyHandle.h"
#include "Slate/Properties/Editors/SDMPropertyEditOpacity.h"
#include "Slate/Properties/SDMMaterialParameters.h"
#include "Slate/SDMComponentEdit.h"
#include "Slate/SDMMaterialWizard.h"
#include "Slate/SDMSlot.h"
#include "Slate/SDMToolBar.h"
#include "SlateOptMacros.h"
#include "Styling/StyleColors.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMEditor"

namespace UE::DynamicMaterialEditor::Private
{
	TSharedPtr<IDetailTreeNode> SearchGeneratorForNode(const TSharedRef<IPropertyRowGenerator>& InGenerator, FName InPropertyName)
	{
		for (const TSharedRef<IDetailTreeNode>& CategoryNode : InGenerator->GetRootTreeNodes())
		{
			if (CategoryNode->GetNodeName() != TEXT("Material Designer"))
			{
				continue;
			}

			TArray<TSharedRef<IDetailTreeNode>> ChildNodes;
			CategoryNode->GetChildren(ChildNodes);

			for (const TSharedRef<IDetailTreeNode>& ChildNode : ChildNodes)
			{
				if (ChildNode->GetNodeType() != EDetailNodeType::Item)
				{
					continue;
				}

				if (ChildNode->GetNodeName() != InPropertyName)
				{
					continue;
				}

				return ChildNode;
			}
		}

		return nullptr;
	}

	TSharedPtr<IPropertyRowGenerator> SearchForGenerator(const TArray<FDMPropertyHandle>& InPropertyHandles, UObject* InObject)
	{
		if (!InObject)
		{
			return nullptr;
		}

		for (const FDMPropertyHandle& PropertyHandle : InPropertyHandles)
		{
			if (PropertyHandle.PropertyRowGenerator.IsValid())
			{
				for (const TWeakObjectPtr<UObject>& WeakObject : PropertyHandle.PropertyRowGenerator->GetSelectedObjects())
				{
					if (WeakObject.Get() == InObject)
					{
						return PropertyHandle.PropertyRowGenerator;
					}
				}
			}
		}

		return nullptr;
	}
}

TSharedPtr<FAssetThumbnailPool> SDMEditor::ThumbnailPool = nullptr;
TMap<const SWidget*, TArray<FDMPropertyHandle>> SDMEditor::PropertyHandleMap;

TSharedRef<FAssetThumbnailPool> SDMEditor::GetThumbnailPool()
{
	if (TSharedPtr<FAssetThumbnailPool> SharedPool = UThumbnailManager::Get().GetSharedThumbnailPool())
	{
		return SharedPool.ToSharedRef();
	}

	if (!ThumbnailPool.IsValid())
	{
		ThumbnailPool = MakeShared<FAssetThumbnailPool>(1024);

		FCoreDelegates::OnEnginePreExit.AddLambda([]()
			{
				ThumbnailPool.Reset();
			});
	}

	return ThumbnailPool.ToSharedRef();
}

void SDMEditor::Construct(const FArguments& InArgs, TWeakObjectPtr<UDynamicMaterialModel> InModelWeak)
{
	SetCanTick(true);

	ActiveSlotIndex = 0;

	BindCommands();
	
	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SAssignNew(Container, SBox)
		[
			CreateMainLayout()
		]
	];

	SetMaterialModel(InModelWeak.Get());

	UDynamicMaterialEditorSettings::Get()->OnSettingsChanged.AddSP(this, &SDMEditor::OnSettingsChanged);

	static bool bAddedEnginePreExitDelegate = false;

	if (!bAddedEnginePreExitDelegate)
	{
		bAddedEnginePreExitDelegate = true;

		FCoreDelegates::OnEnginePreExit.AddLambda(
			[]
			{
				PropertyHandleMap.Reset();
			}
		);
	}
}

void SDMEditor::PrivateRegisterAttributes(struct FSlateAttributeDescriptor::FInitializer&)
{
	
}

SDMEditor::~SDMEditor()
{
	if (FDynamicMaterialModule::AreUObjectsSafe())
	{
		if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
		{
			if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
			{
				ModelEditorOnlyData->GetOnMaterialBuiltDelegate().RemoveAll(this);
				ModelEditorOnlyData->GetOnValueListUpdateDelegate().RemoveAll(this);
				ModelEditorOnlyData->GetOnSlotListUpdateDelegate().RemoveAll(this);
			}
		}
	}
}

void SDMEditor::ClearEditor()
{
	SetMaterialModel(nullptr);
}

void SDMEditor::SetActiveSlotIndex(int InSlotIndex)
{
	if (InSlotIndex != ActiveSlotIndex && SlotWidgets.IsValidIndex(InSlotIndex))
	{
		ActiveSlotIndex = InSlotIndex;
		SlotWidgets[InSlotIndex]->ClearSelection();
		RefreshSlotsList();
	}
}

FDMPropertyHandle SDMEditor::GetPropertyHandle(const SWidget* InOwner, UDMMaterialComponent* InComponent, const FName& InPropertyName)
{
	TArray<FDMPropertyHandle>& PropertyHandles = PropertyHandleMap.FindOrAdd(InOwner);

	for (const FDMPropertyHandle& ExistingHandle : PropertyHandles)
	{
		if (ExistingHandle.PropertyHandle && ExistingHandle.PropertyHandle->GetProperty()->GetFName() == InPropertyName)
		{
			TArray<UObject*> Outers;
			ExistingHandle.PropertyHandle->GetOuterObjects(Outers);

			if (Outers.IsEmpty() == false && Outers[0] == InComponent)
			{
				return ExistingHandle;
			}
		}
	}

	if (TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator = UE::DynamicMaterialEditor::Private::SearchForGenerator(PropertyHandles, InComponent))
	{
		FDMPropertyHandle PropertyHandle;
		PropertyHandle.PropertyRowGenerator = PropertyRowGenerator;

		if (TSharedPtr<IDetailTreeNode> DetailTreeNode = UE::DynamicMaterialEditor::Private::SearchGeneratorForNode(PropertyRowGenerator.ToSharedRef(), InPropertyName))
		{
			PropertyHandle.DetailTreeNode = DetailTreeNode;
			PropertyHandle.PropertyHandle = DetailTreeNode->CreatePropertyHandle();
			return PropertyHandle;
		}

		return PropertyHandle;
	}

	FDMPropertyHandle NewHandle = CreatePropertyHandle(InOwner, InComponent, InPropertyName);
	PropertyHandles.Add(NewHandle);

	return NewHandle;
}

void SDMEditor::ClearPropertyHandles(const SWidget* InOwner)
{
	PropertyHandleMap.Remove(InOwner);
}

FDMPropertyHandle SDMEditor::CreatePropertyHandle(const void* InOwner, UDMMaterialComponent* InComponent,
	const FName& InPropertyName)
{
	FDMPropertyHandle PropertyHandle;

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FPropertyRowGeneratorArgs RowGeneratorArgs;
	RowGeneratorArgs.NotifyHook = InComponent;

	PropertyHandle.PropertyRowGenerator = PropertyEditor.CreatePropertyRowGenerator(RowGeneratorArgs);
	PropertyHandle.PropertyRowGenerator->SetObjects({InComponent});

	if (const TSharedPtr<IDetailTreeNode> FoundTreeNode = UE::DynamicMaterialEditor::Private::SearchGeneratorForNode(
		PropertyHandle.PropertyRowGenerator.ToSharedRef(), InPropertyName))
	{
		PropertyHandle.DetailTreeNode = FoundTreeNode;
		PropertyHandle.PropertyHandle = FoundTreeNode->CreatePropertyHandle();
		return PropertyHandle;
	}

	return PropertyHandle;
}

void SDMEditor::SetMaterialModel(UDynamicMaterialModel* InMaterialModel)
{
	MaterialModelWeak = InMaterialModel;

	SlotsContainer.Reset();
	SlotWidgets.Empty();

	Toolbar->SetMaterialModel(InMaterialModel);

	if (!InMaterialModel)
	{
		Container->SetContent(SDMEditor::GetEmptyContent());
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(InMaterialModel);

	if (EditorOnlyData && EditorOnlyData->NeedsWizard())
	{
		Container->SetContent(SNew(SDMMaterialWizard, SharedThis(this)));
		EditorOnlyData->OnWizardComplete();
	}
	else
	{
		Container->SetContent(CreateMainLayout());
	}

	if (EditorOnlyData)
	{
		EditorOnlyData->GetOnMaterialBuiltDelegate().AddSP(this, &SDMEditor::OnMaterialBuilt);
		EditorOnlyData->GetOnValueListUpdateDelegate().AddSP(this, &SDMEditor::OnValuesUpdated);
		EditorOnlyData->GetOnSlotListUpdateDelegate().AddSP(this, &SDMEditor::OnSlotsUpdated);
	}
}

void SDMEditor::SetMaterialObjectProperty(const FDMObjectMaterialProperty& InObjectProperty)
{
	UDynamicMaterialModel* MaterialModel = InObjectProperty.GetMaterialModel();

	if (IsValid(MaterialModel))
	{
		ObjectProperty = InObjectProperty;
		SetMaterialModel(MaterialModel);
	}
	else
	{
		ObjectProperty.Reset();
	}
}

void SDMEditor::SetMaterialActor(AActor* InActor)
{
	if (UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get())
	{
		if (!Settings->bFollowSelection)
		{
			return;
		}
	}

	if (!Container.IsValid())
	{
		return;
	}

	if (!IsValid(InActor))
	{
		Container->SetContent(SDMEditor::GetEmptyContent());
		return;
	}

	Toolbar->SetMaterialActor(InActor);

	Container->SetContent(
		SNew(SBox)
		.HAlign(HAlign_Center)
		.Padding(5.0f, 5.0f, 5.0f, 5.0f)
		[
			CreateActorMaterialSlotSelector(InActor)
		]
	);
}

TSharedRef<SWidget> SDMEditor::CreateActorMaterialSlotSelector(const AActor* InActor)
{
	TArray<TSharedPtr<FDMObjectMaterialProperty>> MaterialProperties = Toolbar->GetMaterialProperties();
	if (MaterialProperties.IsEmpty())
	{
		return 
			SNew(STextBlock)
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
			.Text(LOCTEXT("NoMaterialSlot", "\n\nThe selected actor contains no primitive components with material slots."));
	}

	TSharedRef<SVerticalBox> ListOuter = 
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 20.0f, 0.0f, 20.0f)
		[
			SNew(STextBlock)
			.TextStyle(FDynamicMaterialEditorStyle::Get(), "ActorNameBig")
			.Text(Toolbar.Get(), &SDMToolBar::GetSlotActorDisplayName)
		];

	const UObject* CurrentOuter = nullptr;

	for (const TSharedPtr<FDMObjectMaterialProperty>& MaterialSlot : MaterialProperties)
	{
		if (!MaterialSlot.IsValid())
		{
			continue;
		}

		// Only show material slots on the selector
		if (MaterialSlot->Property)
		{
			continue;
		}

		const UObject* Outer = MaterialSlot->OuterWeak.Get();
		if (!IsValid(Outer))
		{
			continue;
		}

		UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(MaterialSlot->OuterWeak.Get());

		if (!PrimComponent)
		{
			continue;
		}

		if (Outer != CurrentOuter)
		{
			ListOuter->AddSlot()
				.AutoHeight()
				.Padding(0.f, CurrentOuter == nullptr ? 0.f : 10.f, 0.f, 5.f)
				[
					SNew(STextBlock)
					.TextStyle(FDynamicMaterialEditorStyle::Get(), "ComponentNameBig")
					.Text(FText::FromString(Outer->GetName()))
				];

			CurrentOuter = Outer;
		}

		TWeakPtr<FDMObjectMaterialProperty> MaterialSlotWeak = MaterialSlot;

		constexpr int32 ThumbnailSize = 48;
		TSharedRef<FAssetThumbnail> Thumbnail = MakeShared<FAssetThumbnail>(PrimComponent->GetMaterial(MaterialSlot->Index), ThumbnailSize, ThumbnailSize, UThumbnailManager::Get().GetSharedThumbnailPool());

		FAssetThumbnailConfig ThumbnailConfig;
		ThumbnailConfig.GenericThumbnailSize = ThumbnailSize;

		ListOuter->AddSlot()
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Left)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 5.f, 5.f, 5.f)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					Thumbnail->MakeThumbnailWidget(ThumbnailConfig)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 5.f, 0.5, 5.f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 5.f, 0.f, 5.f)
					[
						SNew(STextBlock)
						.Text(MaterialSlot->GetPropertyName(true))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 5.f)
					[
						SNew(SButton)
						.ContentPadding(FMargin(2.f, 2.f, 2.f, 2.f))
						.OnClicked(this, &SDMEditor::OnCreateMaterialButtonClicked, MaterialSlotWeak)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CreateMaterial", "Create Material"))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
				]
			];
	}

	return SNew(SScrollBox)
		.Orientation(EOrientation::Orient_Vertical)
		+ SScrollBox::Slot()
		[
			ListOuter
		];
}

TSharedRef<SWidget> SDMEditor::GetEmptyContent()
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.Padding(5.0f, 5.0f, 5.0f, 5.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoActiveMaterial", "No active Material Designer Instance."))
		];
}

void SDMEditor::BindCommands()
{
	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(
		FDynamicMaterialEditorCommands::Get().AddDefaultLayer,
		FExecuteAction::CreateSP(this, &SDMEditor::AddNewLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanAddNewLayer)
	);

	CommandList->MapAction(
		FDynamicMaterialEditorCommands::Get().InsertDefaultLayerAbove,
		FExecuteAction::CreateSP(this, &SDMEditor::InsertNewLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanInsertNewLayer)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &SDMEditor::CopySelectedLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanCopySelectedLayer)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &SDMEditor::CutSelectedLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanCutSelectedLayer)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &SDMEditor::PasteLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanPasteLayer)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateSP(this, &SDMEditor::DuplicateSelectedLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanDuplicateSelectedLayer)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SDMEditor::DeleteSelectedLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanDeleteSelectedLayer)
	);
}

TSharedRef<SWidget> SDMEditor::CreateMainLayout()
{
	return 
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SAssignNew(Toolbar, SDMToolBar)
			.MaterialModel(MaterialModelWeak.Get())
			.OnSlotChanged(this, &SDMEditor::OnToolBarPropertyChanged)
			.OnGetSettingsMenu(this, &SDMEditor::MakeToolBarSettingsMenu)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(GlobalOpacityContainer, SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				CreateGlobalOpacityWidget()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			UE::DynamicMaterialEditor::bGlobalValuesEnabled && MaterialModelWeak.IsValid()
				? CreateParametersArea()
				: SNullWidget::NullWidget
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		.Padding(5.f, 0.f)
		[
			SAssignNew(SlotPickerContainer, SBox)
			[
				CreateSlotPickerWidget()
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(SlotsContainer, SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				CreateSlotsWidget()
			]
		];
}

TSharedRef<SWidget> SDMEditor::CreateGlobalOpacityWidget()
{
	UDMMaterialValueFloat1* OpacityValue = nullptr;

	if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
	{
		OpacityValue = MaterialModel->GetGlobalOpacityValue();
	}

	if (!OpacityValue)
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SDMPropertyEdit> GlobalOpacityWidget = SNew(SDMPropertyEditOpacity, SharedThis(this), OpacityValue);
	GlobalOpacityWidget->SetEnabled(TAttribute<bool>::CreateSP(this, &SDMEditor::IsGlobalOpacityEnabled));

	TSharedRef<SWidget> GlobalOpacityButtons = SDMComponentEdit::CreateExtensionButtons(SharedThis(this), OpacityValue, UDMMaterialValue::ValueName, true, FSimpleDelegate());
	GlobalOpacityButtons->SetEnabled(TAttribute<bool>::CreateSP(this, &SDMEditor::IsGlobalOpacityEnabled));

	TSharedPtr<SWidget> RowLabel;

	TSharedRef<SWidget> Row =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(10.0f, 5.0f)
		[
			SAssignNew(RowLabel, SBox)
			.Padding(0.f, 0.f, 5.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GlobalOpacity", "Global Opacity"))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		[
			GlobalOpacityWidget
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			GlobalOpacityButtons
		];

	RowLabel->SetOnMouseButtonDown(FPointerEventHandler::CreateStatic(&SDMPropertyEdit::CreateRightClickDetailsMenu, GlobalOpacityWidget.ToWeakPtr()));

	return Row;
}

TSharedRef<SWidget> SDMEditor::CreateParametersArea()
{
	return 
		SNew(SExpandableArea)
		.InitiallyCollapsed(false)
		.HeaderPadding(FMargin(3.0f, 5.0f, 3.0f, 5.0f))
		.HeaderContent()
		[
			SNew(SBox)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MaterialParameters", "Material Parameters"))
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
		]
		.BodyContent()
		[
			SAssignNew(ParametersWidget, SDMMaterialParameters, MaterialModelWeak)
		];
}

TSharedRef<SWidget> SDMEditor::CreateSlotPickerWidget()
{
	TSharedRef<SWrapBox> SlotSelector = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D(5, 5))
		.Orientation(EOrientation::Orient_Horizontal);

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak);

	if (!ModelEditorOnlyData)
	{
		return SlotSelector;
	}

	const FDMMaterialChannelListPreset* Preset = GetDefault<UDynamicMaterialEditorSettings>()->ChannelPresets.Find(ModelEditorOnlyData->GetChannelListPreset());
	bool bHasBaseColorSlot = !!ModelEditorOnlyData->GetSlotForMaterialProperty(EDMMaterialPropertyType::BaseColor);

	UEnum* MaterialPropertyEnum = StaticEnum<EDMMaterialPropertyType>();

	for (uint8 PropertyIndex = static_cast<uint8>(EDMMaterialPropertyType::None) + 1;
		PropertyIndex < static_cast<uint8>(EDMMaterialPropertyType::Any);
		++PropertyIndex)
	{
		EDMMaterialPropertyType Property = static_cast<EDMMaterialPropertyType>(PropertyIndex);

		// Filter RGB modes
		if (bHasBaseColorSlot)
		{
			if (Property == EDMMaterialPropertyType::EmissiveColor)
			{
				continue;
			}
		}
		else
		{
			if (Property == EDMMaterialPropertyType::BaseColor)
			{
				continue;
			}
		}

		// Filter opacity modes
		if (Property == EDMMaterialPropertyType::OpacityMask)
		{
			if (ModelEditorOnlyData->GetBlendMode() != BLEND_Masked)
			{
				continue;
			}
		}
		else
		{
			if (ModelEditorOnlyData->GetBlendMode() == BLEND_Masked)
			{
				continue;
			}
		}

		const bool bIsPropertyEnabled = Preset
			? Preset->IsPropertyEnabled(Property)
			: !!ModelEditorOnlyData->GetSlotForMaterialProperty(Property);

		if (bIsPropertyEnabled)
		{
			const int32 EnumIndex = MaterialPropertyEnum->GetIndexByValue(static_cast<int64>(Property));

			constexpr const TCHAR* ShortNameName = TEXT("ShortName");
			const FString ShortName = MaterialPropertyEnum->GetMetaData(ShortNameName, EnumIndex);

			SlotSelector->AddSlot()
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "DetailsView.SectionButton")
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.IsEnabled(this, &SDMEditor::IsPropertyValidForModel, Property)
					.IsChecked(this, &SDMEditor::GetSlotCheckState, Property)
					.OnCheckStateChanged(this, &SDMEditor::OnSlotCheckStateChanged, Property)
					.Padding(FVector2D(5.f, 3.f))
					.ToolTipText(this, &SDMEditor::GetToolTipForProperty, Property)
					.Content()
					[
						SNew(STextBlock)
						.Text(!ShortName.IsEmpty() ? FText::FromString(ShortName) : MaterialPropertyEnum->GetDisplayNameTextByValue(static_cast<int64>(Property)))
					]
				];
		}
	}

	return SlotSelector;
}

TSharedPtr<SDMSlot> SDMEditor::GetSlotWidget(UDMMaterialSlot* Slot) const
{
	if (ensure(IsValid(Slot)))
	{
		for (const TSharedRef<SDMSlot>& SlotWidget : SlotWidgets)
		{
			if (SlotWidget->GetSlot() == Slot)
			{
				return SlotWidget;
			}
		}
	}

	return nullptr;
}

void SDMEditor::RefreshGlobalOpacitySlider()
{
	if (GlobalOpacityContainer.IsValid())
	{
		GlobalOpacityContainer->SetContent(SNullWidget::NullWidget);
		GlobalOpacityContainer->SetContent(CreateGlobalOpacityWidget());
	}
}

void SDMEditor::RefreshParametersList()
{
	if (ParametersWidget.IsValid())
	{
		ParametersWidget->RefreshWidgets();
	}
}

void SDMEditor::RefreshSlotPickerList()
{
	if (SlotPickerContainer.IsValid())
	{
		SlotPickerContainer->SetContent(SNullWidget::NullWidget);
		SlotPickerContainer->SetContent(CreateSlotPickerWidget());
	}
}

void SDMEditor::RefreshSlotsList()
{
	if (SlotsContainer.IsValid())
	{
		SlotsContainer->SetContent(SNullWidget::NullWidget);
		SlotsContainer->SetContent(CreateSlotsWidget());
	}
}

void SDMEditor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!MaterialModelWeak.IsValid())
	{
		return;
	}

	bool bValidModel = false;

	if (ObjectProperty.IsValid())
	{
		bValidModel = IsValid(ObjectProperty.GetMaterialModel());
	}
	else if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
	{
		auto CheckValidity = [MaterialModel]()
		{
			if (UWorld* World = MaterialModel->GetWorld())
			{
				if (UDMWorldSubsystem* WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>())
				{
					// ExecuteIfBound doesn't work with return values
					if (WorldSubsystem->GetIsValidDelegate().IsBound()
						&& WorldSubsystem->GetIsValidDelegate().Execute(MaterialModel) == false)
					{
						return false;
					}
				}
			}

			UActorComponent* ComponentOuter = MaterialModel->GetTypedOuter<UActorComponent>();
			if (ComponentOuter && !IsValid(ComponentOuter))
			{
				return false;
			}

			AActor* ActorOuter = MaterialModel->GetTypedOuter<AActor>();
			if (ActorOuter && !IsValid(ActorOuter))
			{
				return false;
			}

			UPackage* PackageOuter = MaterialModel->GetPackage();
			if (PackageOuter && !IsValid(PackageOuter))
			{
				return false;
			}

			return true;
		};

		bValidModel = CheckValidity();
	}

	if (!bValidModel)
	{
		ClearEditor();
	}
	else if (FDynamicMaterialModule::AreUObjectsSafe())
	{
		if (Toolbar.IsValid() && Toolbar->GetMaterialModel() != MaterialModelWeak)
		{
			Toolbar->SetMaterialModel(MaterialModelWeak.Get());
		}
		else
		{
			bool bHasInvalidSlotWidget = false;

			for (const TSharedRef<SDMSlot>& Slot : SlotWidgets)
			{
				if (!Slot->CheckValidity())
				{
					bHasInvalidSlotWidget = true;
					break;
				}
			}
			if (bHasInvalidSlotWidget)
			{
				RefreshSlotsList();
			}
		}
	}
}

void SDMEditor::OnMaterialBuilt(UDynamicMaterialModel* InMaterialModel)
{
}

void SDMEditor::OnValuesUpdated(UDynamicMaterialModel* InMaterialModel)
{
	RefreshParametersList();
}

void SDMEditor::OnSlotsUpdated(UDynamicMaterialModel* InMaterialModel)
{
	RefreshSlotsList();
}

bool SDMEditor::IsGlobalOpacityEnabled() const
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		return ModelEditorOnlyData->GetBlendMode() == BLEND_Translucent || ModelEditorOnlyData->GetBlendMode() == BLEND_Masked;
	}

	return false;
}

bool SDMEditor::IsPropertyValidForModel(EDMMaterialPropertyType InProperty) const
{
	if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
	{
		if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
		{
			if (UDMMaterialProperty* Property = EditorOnlyData->GetMaterialProperty(InProperty))
			{
				return Property->IsValidForModel(*EditorOnlyData);
			}
		}
	}

	return false;
}

ECheckBoxState SDMEditor::GetSlotCheckState(EDMMaterialPropertyType InProperty)  const
{
	if (SlotWidgets.IsValidIndex(ActiveSlotIndex))
	{
		if (UDMMaterialSlot* Slot = SlotWidgets[ActiveSlotIndex]->GetSlot())
		{
			if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = Slot->GetMaterialModelEditorOnlyData())
			{
				return EditorOnlyData->GetSlotForMaterialProperty(InProperty) == Slot
					? ECheckBoxState::Checked
					: ECheckBoxState::Unchecked;
			}
		}
	}

	return ECheckBoxState::Undetermined;
}

FText SDMEditor::GetToolTipForProperty(EDMMaterialPropertyType InProperty) const
{
	if (!IsPropertyValidForModel(InProperty))
	{
		return LOCTEXT("SlotNotCompatible", "This slot is not compatible with the material settings.");
	}

	return FText::GetEmpty();
}

void SDMEditor::OnSlotCheckStateChanged(ECheckBoxState InCheckState, EDMMaterialPropertyType InProperty)
{
	if (InCheckState == ECheckBoxState::Checked)
	{
		for (int32 SlotIndex = 0; SlotIndex < SlotWidgets.Num(); ++SlotIndex)
		{
			if (UDMMaterialSlot* Slot = SlotWidgets[SlotIndex]->GetSlot())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = Slot->GetMaterialModelEditorOnlyData())
				{
					if (EditorOnlyData->GetSlotForMaterialProperty(InProperty) == Slot)
					{
						SetActiveSlotIndex(SlotIndex);
					}
				}
			}
		}
	}
}

FReply SDMEditor::OnCreateMaterialButtonClicked(TWeakPtr<FDMObjectMaterialProperty> InMaterialProperty)
{
	if (TSharedPtr<FDMObjectMaterialProperty> MaterialProperty = InMaterialProperty.Pin())
	{
		UDynamicMaterialModel* NewModel = UDMBlueprintFunctionLibrary::CreateDynamicMaterialInObject(*MaterialProperty);

		if (NewModel)
		{
			if (Toolbar.IsValid())
			{
				Toolbar->SetMaterialModel(NewModel);
			}
		}
	}

	return FReply::Handled();
}

void SDMEditor::OnToolBarPropertyChanged(TSharedPtr<FDMObjectMaterialProperty> InNewSelectedProperty)
{
	if (InNewSelectedProperty.IsValid())
	{
		SetMaterialModel(InNewSelectedProperty->GetMaterialModel());
	}
}

TSharedRef<SWidget> SDMEditor::MakeToolBarSettingsMenu()
{
	return FDMToolBarMenus::MakeEditorLayoutMenu(SharedThis(this));
}

void SDMEditor::OnSettingsChanged(const FPropertyChangedEvent& InPropertyChangedEvent)
{
	SetMaterialModel(MaterialModelWeak.Get());
}

bool SDMEditor::CanAddNewLayer() const
{
	return SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& MaterialModelWeak.IsValid();
}

void SDMEditor::AddNewLayer()
{
	if (SlotWidgets.IsValidIndex(ActiveSlotIndex))
	{
		SlotWidgets[ActiveSlotIndex]->AddNewLayer_Expression(
			TSubclassOf<UDMMaterialStageExpression>(UDMMaterialStageExpressionTextureSample::StaticClass()),
			EDMMaterialLayerStage::All
		);

		SlotWidgets[ActiveSlotIndex]->InvalidateMainWidget();
	}
}

bool SDMEditor::CanInsertNewLayer() const
{
	return SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& SlotWidgets[ActiveSlotIndex]->GetSelectedLayerIndices().Num() == 1
		&& MaterialModelWeak.IsValid();
}

void SDMEditor::InsertNewLayer()
{
	if (SlotWidgets.IsValidIndex(ActiveSlotIndex))
	{
		if (UDMMaterialLayerObject* SelectedLayer = SlotWidgets[ActiveSlotIndex]->GetSelectedLayer())
		{
			if (UDMMaterialSlot* Slot = SelectedLayer->GetSlot())
			{
				// Added here because stuff is done after the layer is added
				FDMScopedUITransaction Transaction(LOCTEXT("InsertNewLayer", "Material Designer Insert Layer"));
				Slot->Modify();

				SlotWidgets[ActiveSlotIndex]->AddNewLayer_Expression(
					TSubclassOf<UDMMaterialStageExpression>(UDMMaterialStageExpressionTextureSample::StaticClass()),
					EDMMaterialLayerStage::All
				);

				Slot->MoveLayerAfter(Slot->GetLayers().Last(), SelectedLayer);

				SlotWidgets[ActiveSlotIndex]->InvalidateMainWidget();
			}
		}
	}
}

bool SDMEditor::CanCopySelectedLayer() const
{
	return SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& SlotWidgets[ActiveSlotIndex]->GetSelectedLayerIndices().Num() == 1
		&& MaterialModelWeak.IsValid();
}

void SDMEditor::CopySelectedLayer()
{
	if (SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& SlotWidgets[ActiveSlotIndex]->GetSelectedLayerIndices().Num() == 1)
	{
		if (const UDMMaterialLayerObject* Layer = SlotWidgets[ActiveSlotIndex]->GetSelectedLayer())
		{
			FPlatformApplicationMisc::ClipboardCopy(*Layer->SerializeToString());
		}
	}
}

bool SDMEditor::CanCutSelectedLayer() const
{
	return CanCopySelectedLayer() && CanDeleteSelectedLayer();
}

void SDMEditor::CutSelectedLayer()
{
	CopySelectedLayer();
	DeleteSelectedLayer();
}

bool SDMEditor::CanPasteLayer() const
{
	if (!MaterialModelWeak.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return !ClipboardContent.IsEmpty();
}

void SDMEditor::PasteLayer()
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		if (UDMMaterialSlot* Slot = ModelEditorOnlyData->GetSlot(ActiveSlotIndex))
		{
			FString SerializeString;
			FPlatformApplicationMisc::ClipboardPaste(SerializeString);

			if (UDMMaterialLayerObject* PastedLayer = UDMMaterialLayerObject::DeserializeFromString(Slot, SerializeString))
			{
				FDMScopedUITransaction Transaction(LOCTEXT("PasteLayer", "Material Designer Paste Layer"));
				Slot->Modify();
				Slot->PasteLayer(PastedLayer);

				SlotWidgets[ActiveSlotIndex]->InvalidateMainWidget();
			}
		}
	}
}

bool SDMEditor::CanDuplicateSelectedLayer() const
{
	// There's no "can add" check, so only copy is tested.
	return CanCopySelectedLayer();
}

void SDMEditor::DuplicateSelectedLayer()
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	// Added here to set the transaction description
	FDMScopedUITransaction Transaction(LOCTEXT("DuplicateLayer", "Material Designer Duplicate Layer"));

	CopySelectedLayer();
	PasteLayer();

	FPlatformApplicationMisc::ClipboardCopy(*PastedText);
}

bool SDMEditor::CanDeleteSelectedLayer() const
{
	if (SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& SlotWidgets[ActiveSlotIndex]->GetSelectedLayerIndices().IsEmpty() == false)
	{
		return SlotWidgets[ActiveSlotIndex]->GetLayerRowsButtonsCanRemove();
	}

	return false;
}

void SDMEditor::DeleteSelectedLayer()
{
	if (SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& SlotWidgets[ActiveSlotIndex]->GetSelectedLayerIndices().IsEmpty() == false)
	{
		SlotWidgets[ActiveSlotIndex]->OnLayerRowButtonsRemoveClicked();
	}
}

FReply SDMEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList.IsValid())
	{
		if (CommandList->ProcessCommandBindings(InKeyEvent))
		{
			return FReply::Handled();
		}
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

void SDMEditor::PostUndo(bool bSuccess)
{
	OnUndo();
}

void SDMEditor::PostRedo(bool bSuccess)
{
	OnUndo();
}

TSharedRef<SWidget> SDMEditor::CreateSlotsWidget()
{
	SlotWidgets.Empty();

	auto CreateEmptySlotsContent = []() -> TSharedRef<SBox>
	{
		return 
			SNew(SBox)
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(10.f, 5.f, 10.f, 5.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SlotsContent", "Slots Content"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	};
	
	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak);

	if (!ModelEditorOnlyData)
	{
		return CreateEmptySlotsContent();
	}

	const TArray<UDMMaterialSlot*>& Slots = ModelEditorOnlyData->GetSlots();

	if (Slots.IsEmpty())
	{
		return CreateEmptySlotsContent();
	}

	SlotWidgets.Reserve(Slots.Num());

	for (int32 SlotIdx = 0; SlotIdx < Slots.Num(); ++SlotIdx)
	{
		UDMMaterialSlot* Slot = Slots[SlotIdx];

		TSharedRef<SDMSlot> SlotWidget =
			SNew(SDMSlot, SharedThis(this), Slot)
			.SlotPreviewSize_Lambda([]() { return UDynamicMaterialEditorSettings::Get()->SlotPreviewSize; })
			.LayerPreviewSize_Lambda([]() { return UDynamicMaterialEditorSettings::Get()->LayerPreviewSize; });

		SlotWidgets.Add(SlotWidget);
	}

	if (!SlotWidgets.IsValidIndex(ActiveSlotIndex))
	{
		if (SlotWidgets.IsEmpty())
		{
			return SNullWidget::NullWidget;
		}

		ActiveSlotIndex = 0;
	}

	return SlotWidgets[ActiveSlotIndex];
}

void SDMEditor::OnUndo()
{
	UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get();

	if (!IsValid(MaterialModel))
	{
		return;
	}

	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		if (ModelEditorOnlyData->GetBlendMode() == BLEND_Opaque)
		{
			SetActiveSlotIndex(0);
		}
	}
}

#undef LOCTEXT_NAMESPACE
