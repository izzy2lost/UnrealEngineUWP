// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMEditor.h"
#include "AssetThumbnail.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialValues/DMMaterialValueFloat.h"
#include "Components/PrimitiveComponent.h"
#include "DetailLayoutBuilder.h"
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
#include "Materials/MaterialInterface.h"
#include "Menus/DMToolBarMenus.h"
#include "Misc/CoreDelegates.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "PropertyHandle.h"
#include "Slate/Properties/SDMMaterialParameters.h"
#include "Slate/SDMComponentEdit.h"
#include "Slate/SDMMaterialWizard.h"
#include "Slate/SDMSlot.h"
#include "Slate/SDMToolBar.h"
#include "SlateOptMacros.h"
#include "Material/DynamicMaterialInstance.h"
#include "Styling/StyleColors.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Utils/DMBlueprintFunctionLibrary.h"
#include "Utils/DMPrivate.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMEditor"

TMap<SDMEditor::FExpansionItem, bool> SDMEditor::ExpansionStates;

namespace UE::DynamicMaterialEditor::Private
{
	TSharedPtr<IDetailTreeNode> SearchNodesForProperty(const TArray<TSharedRef<IDetailTreeNode>>& InNodes, FName InPropertyName)
	{
		for (const TSharedRef<IDetailTreeNode>& ChildNode : InNodes)
		{
			switch (ChildNode->GetNodeType())
			{
				case EDetailNodeType::Category:
				{
					TArray<TSharedRef<IDetailTreeNode>> CategoryChildNodes;
					ChildNode->GetChildren(CategoryChildNodes);

					if (TSharedPtr<IDetailTreeNode> FoundNode = SearchNodesForProperty(CategoryChildNodes, InPropertyName))
					{
						return FoundNode;
					}

					break;
				}

				case EDetailNodeType::Item:
					if (ChildNode->GetNodeName() == InPropertyName)
					{
						return ChildNode;
					}
					break;

				default:
					// Do nothing
					break;
			}
		}

		return nullptr;
	}

	TSharedPtr<IDetailTreeNode> SearchGeneratorForNode(const TSharedRef<IPropertyRowGenerator>& InGenerator, FName InPropertyName)
	{
		return SearchNodesForProperty(InGenerator->GetRootTreeNodes(), InPropertyName);
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

	void AddPropertyMetaData(UObject* InObject, FName InPropertyName, FDMPropertyHandle& InPropertyHandle)
	{
		FProperty* Property = nullptr;

		if (InPropertyHandle.PropertyHandle.IsValid())
		{
			Property = InPropertyHandle.PropertyHandle->GetProperty();

			if (UDMMaterialValueFloat* FloatValue = Cast<UDMMaterialValueFloat>(InObject))
			{
				if (FloatValue->HasValueRange())
				{
					const FName UIMin = FName("UIMin");
					const FName UIMax = FName("UIMax");
					const FName ClampMin = FName("ClampMin");
					const FName ClampMax = FName("ClampMax");

					InPropertyHandle.PropertyHandle->SetInstanceMetaData(UIMin, FString::SanitizeFloat(FloatValue->GetValueRange().Min));
					InPropertyHandle.PropertyHandle->SetInstanceMetaData(ClampMin, FString::SanitizeFloat(FloatValue->GetValueRange().Min));
					InPropertyHandle.PropertyHandle->SetInstanceMetaData(UIMax, FString::SanitizeFloat(FloatValue->GetValueRange().Max));
					InPropertyHandle.PropertyHandle->SetInstanceMetaData(ClampMax, FString::SanitizeFloat(FloatValue->GetValueRange().Max));
				}
			}
		}
		else
		{
			Property = InObject->GetClass()->FindPropertyByName(InPropertyName);
		}

		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			uint8 ComponentCount = 1;

			if (StructProperty->Struct == TBaseStructure<FVector2D>::Get()
				|| StructProperty->Struct == TVariantStructure<FVector2f>::Get())
			{
				ComponentCount = 2;
			}

			if (StructProperty->Struct == TBaseStructure<FVector>::Get()
				|| StructProperty->Struct == TVariantStructure<FVector3f>::Get()
				|| StructProperty->Struct == TBaseStructure<FRotator>::Get())
			{
				ComponentCount = 3;
			}

			// FLinearColor doesn't need the extra space
			if (StructProperty->Struct == TBaseStructure<FVector4>::Get()
				|| StructProperty->Struct == TVariantStructure<FVector4f>::Get())
			{
				ComponentCount = 4;
			}

			switch (ComponentCount)
			{
				case 0:
				case 1:
					break;

				case 2:
					InPropertyHandle.MaxWidth = 200.f;
					break;

					// 3 and above
				default:
					InPropertyHandle.MaxWidth = 275.f;
					break;
			}
		}
	}

	bool CheckMaterialModelValidity(UDynamicMaterialModel* InMaterialModel)
		{
			if (UWorld* World = InMaterialModel->GetWorld())
			{
				if (UDMWorldSubsystem* WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>())
				{
					if (!WorldSubsystem->ExecuteIsValidDelegate(InMaterialModel))
					{
						return false;
					}
				}
			}

			UActorComponent* ComponentOuter = InMaterialModel->GetTypedOuter<UActorComponent>();
			if (ComponentOuter && !IsValid(ComponentOuter))
			{
				return false;
			}

			AActor* ActorOuter = InMaterialModel->GetTypedOuter<AActor>();
			if (ActorOuter && !IsValid(ActorOuter))
			{
				return false;
			}

			UPackage* PackageOuter = InMaterialModel->GetPackage();
			if (PackageOuter && !IsValid(PackageOuter))
			{
				return false;
			}

			return true;
		};
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

	SetEmptyLayout();

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
	MaterialModelWeak.Reset();
	ObjectProperty.Reset();

	SlotPickerContainer.Reset();
	SlotContainer.Reset();
	ActiveSlotWidget.Reset();
	ComponentEditContainer.Reset();
	SplitterContainer.Reset();

	bInvalidateComponentEditWidget = false;
}

void SDMEditor::ResetEditor()
{
	ClearEditor();
	SetEmptyLayout();
}

void SDMEditor::SetActiveSlotIndex(int InSlotIndex)
{
	if (InSlotIndex == INDEX_NONE)
	{
		ActiveSlotIndex = INDEX_NONE;
		RefreshSlotWidget();

		EditedComponent.Reset();
		RefreshComponentEditWidget();
	}
	else if (InSlotIndex != ActiveSlotIndex)
	{
		if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
		{
			if (ModelEditorOnlyData->GetSlots().IsValidIndex(InSlotIndex))
			{
				ActiveSlotIndex = InSlotIndex;
				RefreshSlotWidget();

				EditedComponent.Reset();
				RefreshComponentEditWidget();
			}
		}
	}
}

FDMPropertyHandle SDMEditor::GetPropertyHandle(const SWidget* InOwningWidget, UObject* InObject, const FName& InPropertyName)
{
	TArray<FDMPropertyHandle>& PropertyHandles = PropertyHandleMap.FindOrAdd(InOwningWidget);

	for (const FDMPropertyHandle& ExistingHandle : PropertyHandles)
	{
		if (ExistingHandle.PropertyHandle && ExistingHandle.PropertyHandle->GetProperty()->GetFName() == InPropertyName)
		{
			TArray<UObject*> Outers;
			ExistingHandle.PropertyHandle->GetOuterObjects(Outers);

			if (Outers.IsEmpty() == false && Outers[0] == InObject)
			{
				return ExistingHandle;
			}
		}
	}

	using namespace UE::DynamicMaterialEditor::Private;

	if (TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator = SearchForGenerator(PropertyHandles, InObject))
	{
		FDMPropertyHandle PropertyHandle;
		PropertyHandle.PropertyRowGenerator = PropertyRowGenerator;

		if (TSharedPtr<IDetailTreeNode> DetailTreeNode = SearchGeneratorForNode(PropertyRowGenerator.ToSharedRef(), InPropertyName))
		{
			PropertyHandle.DetailTreeNode = DetailTreeNode;
			PropertyHandle.PropertyHandle = DetailTreeNode->CreatePropertyHandle();

			AddPropertyMetaData(InObject, InPropertyName, PropertyHandle);

			return PropertyHandle;
		}

		return PropertyHandle;
	}

	FDMPropertyHandle NewHandle = CreatePropertyHandle(InOwningWidget, InObject, InPropertyName);

	if (!NewHandle.PropertyHandle.IsValid() && NewHandle.DetailTreeNode.IsValid())
	{
		NewHandle.PropertyHandle = NewHandle.DetailTreeNode->CreatePropertyHandle();
	}

	AddPropertyMetaData(InObject, InPropertyName, NewHandle);

	PropertyHandles.Add(NewHandle);

	return NewHandle;
}

void SDMEditor::ClearPropertyHandles(const SWidget* InOwningWidget)
{
	PropertyHandleMap.Remove(InOwningWidget);
}

FDMPropertyHandle SDMEditor::CreatePropertyHandle(const void* InOwningWidget, UObject* InObject, const FName& InPropertyName)
{
	FDMPropertyHandle PropertyHandle;

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FPropertyRowGeneratorArgs RowGeneratorArgs;

	if (UDMMaterialComponent* Component = Cast<UDMMaterialComponent>(InObject))
	{
		RowGeneratorArgs.NotifyHook = Component;
	}

	PropertyHandle.PropertyRowGenerator = PropertyEditor.CreatePropertyRowGenerator(RowGeneratorArgs);
	PropertyHandle.PropertyRowGenerator->SetObjects({InObject});

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
	if (InMaterialModel && !UE::DynamicMaterialEditor::Private::CheckMaterialModelValidity(InMaterialModel))
	{
		InMaterialModel = nullptr;
	}

	if (MaterialModelWeak.Get() == InMaterialModel)
	{
		return;
	}

	ClearEditor();

	MaterialModelWeak = InMaterialModel;

	Toolbar->SetMaterialModel(InMaterialModel);

	SetEditedComponent(nullptr);

	if (!InMaterialModel)
	{
		SetEmptyLayout();
		return;
	}

	bHasActiveLayout = true;

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(InMaterialModel);

	if (EditorOnlyData && EditorOnlyData->NeedsWizard())
	{
		Container->SetContent(SNew(SDMMaterialWizard, SharedThis(this)));
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
	if (!InObjectProperty.IsValid())
	{
		ResetEditor();
		return;
	}

	UDynamicMaterialModel* MaterialModel = InObjectProperty.GetMaterialModel();

	if (!IsValid(MaterialModel))
	{
		ResetEditor();
		return;
	}

	SetMaterialModel(MaterialModel);
	ObjectProperty = InObjectProperty;

	if (AActor* Actor = ObjectProperty.GetTypedOuter<AActor>())
	{
		SetMaterialActor(Actor);
	}
}

AActor* SDMEditor::GetMaterialActor() const
{
	if (Toolbar.IsValid())
	{
		return Toolbar->GetMaterialActor();
	}

	return nullptr;
}

void SDMEditor::SetMaterialActor(AActor* InActor)
{
	if (!IsValid(InActor))
	{
		return;
	}

	if (!GetMaterialModel())
	{
		Container->SetContent(
			SNew(SBox)
			.HAlign(HAlign_Center)
			.Padding(5.0f, 5.0f, 5.0f, 5.0f)
			[
				CreateActorMaterialSlotSelector(InActor)
			]
		);
	}


	if (Toolbar->GetMaterialActor() != InActor)
	{
		Toolbar->SetMaterialActor(InActor);
	}
}

void SDMEditor::SetEmptyLayout()
{
	bHasActiveLayout = false;
	Container->SetContent(SDMEditor::GetEmptyContent());
}

void SDMEditor::OnMaterialModelSelected(UDynamicMaterialModel* InMaterialModel)
{
	if (!IsValid(InMaterialModel))
	{
		return;
	}

	// Only check this setting if we have an active material model.
	if (GetMaterialModel())
	{
		UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();

		if (!Settings || !Settings->bFollowSelection)
		{
			return;
		}
	}

	SetMaterialModel(InMaterialModel);
}

void SDMEditor::OnMaterialInstanceSelected(UDynamicMaterialInstance* InMaterialInstance)
{
	if (!IsValid(InMaterialInstance))
	{
		return;
	}

	OnMaterialModelSelected(InMaterialInstance->GetMaterialModel());
}

void SDMEditor::OnActorSelected(AActor* InActor)
{
	// Only check this setting if we have an active material model.
	if (GetMaterialModel())
	{
		UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();

		if (!Settings || !Settings->bFollowSelection)
		{
			return;
		}
	}

	if (!Container.IsValid())
	{
		return;
	}

	ClearEditor();

	if (!IsValid(InActor))
	{
		SetEmptyLayout();
		return;
	}

	TArray<FDMObjectMaterialProperty> ActorProperties = UDMBlueprintFunctionLibrary::GetActorMaterialProperties(InActor);

	for (int32 MaterialPropertyIdx = 0; MaterialPropertyIdx < ActorProperties.Num(); ++MaterialPropertyIdx)
	{
		const FDMObjectMaterialProperty& MaterialProperty = ActorProperties[MaterialPropertyIdx];
		UDynamicMaterialModel* Model = MaterialProperty.GetMaterialModel();

		if (UDynamicMaterialModel* MaterialModel = Model)
		{
			SetMaterialObjectProperty(MaterialProperty);
		}
	}

	if (!ActorProperties.IsEmpty() && !GetMaterialModel())
	{
		SetMaterialObjectProperty(ActorProperties[0]);
	}

	SetMaterialActor(InActor);
}

TSharedPtr<SDMSlot> SDMEditor::GetActiveSlotWidget() const
{
	return ActiveSlotWidget;
}

TSharedRef<SWidget> SDMEditor::CreateActorMaterialSlotSelector(AActor* InActor)
{
	TArray<FDMObjectMaterialProperty> MaterialProperties = UDMBlueprintFunctionLibrary::GetActorMaterialProperties(InActor);

	if (MaterialProperties.IsEmpty())
	{
		return 
			SNew(STextBlock)
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
			.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
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

	for (const FDMObjectMaterialProperty& MaterialSlot : MaterialProperties)
	{
		if (!MaterialSlot.IsValid())
		{
			continue;
		}

		// Only show material slots on the selector
		if (MaterialSlot.Property)
		{
			continue;
		}

		const UObject* Outer = MaterialSlot.OuterWeak.Get();
		if (!IsValid(Outer))
		{
			continue;
		}

		UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(MaterialSlot.OuterWeak.Get());

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

		constexpr int32 ThumbnailSize = 48;

		TSharedRef<FAssetThumbnail> Thumbnail = MakeShared<FAssetThumbnail>(
			PrimComponent->GetMaterial(MaterialSlot.Index),
			ThumbnailSize,
			ThumbnailSize, 
			GetThumbnailPool()
		);

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
						.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
						.Text(MaterialSlot.GetPropertyName(true))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 5.f)
					[
						SNew(SButton)
						.ContentPadding(FMargin(2.f, 2.f, 2.f, 2.f))
						.OnClicked(this, &SDMEditor::OnCreateMaterialButtonClicked, MaterialSlot)
						[
							SNew(STextBlock)
							.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
							.Text(LOCTEXT("CreateMaterial", "Create Material"))
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
			.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
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
	const float SplitterValue = UDynamicMaterialEditorSettings::Get()->SplitterLocation;

	return 
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SAssignNew(Toolbar, SDMToolBar, SharedThis(this))
			.MaterialModel(MaterialModelWeak.Get())
			.OnSlotChanged(this, &SDMEditor::OnToolBarPropertyChanged)
			.OnGetSettingsMenu(this, &SDMEditor::MakeToolBarSettingsMenu)
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
			SAssignNew(SplitterContainer, SSplitter)
			.Style(FAppStyle::Get(), "DetailsView.Splitter")
			.Orientation(Orient_Vertical)
			.ResizeMode(ESplitterResizeMode::Fill)
			.PhysicalSplitterHandleSize(3.0f)
			.HitDetectionSplitterHandleSize(4.0f)
			.OnSplitterFinishedResizing(this, &SDMEditor::OnSplitterResized)

			+ SSplitter::Slot()
			.Expose(LayerViewSplitterSlot)
			.Resizable(true)
			.SizeRule(SSplitter::ESizeRule::FractionOfParent)
			.MinSize(50)
			.Value(SplitterValue)
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FDynamicMaterialEditorStyle::GetBrush("LayerView.Background"))
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(SlotContainer, SScrollBox)
					+ SScrollBox::Slot()
					.FillSize(1.f)
					[
						CreateSlotWidget()
					]
				]
			]

			+ SSplitter::Slot()
			.Expose(ExtraSpaceSplitterSlot)
			.Resizable(true)
			.SizeRule(SSplitter::ESizeRule::FractionOfParent)
			.MinSize(50)
			.Value(SplitterValue)
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FDynamicMaterialEditorStyle::GetBrush("LayerView.Background"))
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(ComponentEditContainer, SScrollBox)
					+ SScrollBox::Slot()
					.AutoSize()
					[
						CreateComponentEditWidget()
					]
				]
			]
		];
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
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("MaterialParameters", "Material Parameters"))
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

	SlotSelector->AddSlot()
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "DetailsView.SectionButton")
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.IsChecked(this, &SDMEditor::GetSlotCheckState, EDMMaterialPropertyType::None)
			.OnCheckStateChanged(this, &SDMEditor::OnSlotCheckStateChanged, EDMMaterialPropertyType::None)
			.Padding(FVector2D(5.f, 3.f))
			.ToolTipText(LOCTEXT("GlobalSettingsToolTip", "Global Material Settings"))
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
				.Text(LOCTEXT("GlobalSettings", "Global"))
			]
		];

	const FDMMaterialChannelListPreset* Preset = GetDefault<UDynamicMaterialEditorSettings>()->GetPresetByName(ModelEditorOnlyData->GetChannelListPreset());
	const bool bHasBaseColorSlot = !!ModelEditorOnlyData->GetSlotForMaterialProperty(EDMMaterialPropertyType::BaseColor);

	UEnum* MaterialPropertyEnum = StaticEnum<EDMMaterialPropertyType>();

	for (const TPair<EDMMaterialPropertyType, UDMMaterialProperty*>& Property : ModelEditorOnlyData->GetMaterialProperties())
	{
		if (Property.Key == EDMMaterialPropertyType::BaseColor && ModelEditorOnlyData->GetShadingModel() == EDMMaterialShadingModel::Unlit)
		{
			continue;
		}

		if (Property.Key == EDMMaterialPropertyType::EmissiveColor && ModelEditorOnlyData->GetShadingModel() == EDMMaterialShadingModel::DefaultLit)
		{
			continue;
		}		

		// Always create opacity, not opacity mask - Will be sorted out by the button itself.
		if (Property.Key == EDMMaterialPropertyType::OpacityMask)
		{
			continue;
		}

		if (Property.Key == EDMMaterialPropertyType::Opacity && ModelEditorOnlyData->GetBlendMode() == BLEND_Opaque)
		{
			continue;
		}

		const bool bIsPropertyEnabled = Preset
			? Preset->IsPropertyEnabled(Property.Key)
			: !!ModelEditorOnlyData->GetSlotForMaterialProperty(Property.Key);

		if (bIsPropertyEnabled)
		{
			const int32 EnumIndex = MaterialPropertyEnum->GetIndexByValue(static_cast<int64>(Property.Key));

			constexpr const TCHAR* ShortNameName = TEXT("ShortName");
			const FString ShortName = MaterialPropertyEnum->GetMetaData(ShortNameName, EnumIndex);

			SlotSelector->AddSlot()
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "DetailsView.SectionButton")
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.IsEnabled(this, &SDMEditor::IsPropertyValidForModel, Property.Key)
					.IsChecked(this, &SDMEditor::GetSlotCheckState, Property.Key)
					.OnCheckStateChanged(this, &SDMEditor::OnSlotCheckStateChanged, Property.Key)
					.Padding(FVector2D(5.f, 3.f))
					.ToolTipText(this, &SDMEditor::GetToolTipForProperty, Property.Key)
					.Content()
					[
						SNew(STextBlock)
						.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
						.Text(!ShortName.IsEmpty() ? FText::FromString(ShortName) : MaterialPropertyEnum->GetDisplayNameTextByValue(static_cast<int64>(Property.Key)))
					]
				];
		}
	}

	return SlotSelector;
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

void SDMEditor::RefreshSlotWidget()
{
	if (SlotContainer.IsValid())
	{
		SlotContainer->ClearChildren();
		SlotContainer->AddSlot()
			.FillSize(1.f)
			[
				CreateSlotWidget()
			];
	}
}

void SDMEditor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get();

	if (!IsValid(MaterialModel))
	{
		if (bHasActiveLayout)
		{
			ResetEditor();
		}

		return;
	}

	if (!bHasActiveLayout)
	{
		return;
	}

	if (ObjectProperty.IsValid())
	{
		UDynamicMaterialModel* MaterialModelProperty = ObjectProperty.GetMaterialModel();

		if (!IsValid(MaterialModelProperty))
		{
			MaterialModelProperty = nullptr;
		}

		if (MaterialModel != MaterialModelProperty)
		{
			if (MaterialModelProperty)
			{
				// Re-set the property
				SetMaterialObjectProperty(ObjectProperty);
				return;
			}
			else
			{
				ResetEditor();
				return;
			}
		}
	}
	else if (!UE::DynamicMaterialEditor::Private::CheckMaterialModelValidity(MaterialModel))
	{
		ResetEditor();
		return;
	}

	if (Toolbar.IsValid())
	{
		if (Toolbar->GetMaterialModel() != MaterialModel)
		{
			Toolbar->SetMaterialModel(MaterialModel);
		}

		if (ObjectProperty.IsValid())
		{
			AActor* Actor = ObjectProperty.GetTypedOuter<AActor>();

			if (Toolbar->GetMaterialActor() != Actor)
			{
				Toolbar->SetMaterialActor(Actor);
			}
		}
		else if (Toolbar->GetMaterialActor())
		{
			Toolbar->SetMaterialActor(nullptr);
		}
	}

	if (ActiveSlotWidget.IsValid() && !ActiveSlotWidget->CheckValidity())
	{
		RefreshSlotWidget();
	}

	if (bInvalidateComponentEditWidget)
	{
		RefreshComponentEditWidget();
	}
}

void SDMEditor::OnMaterialBuilt(UDynamicMaterialModel* InMaterialModel)
{
	RefreshSlotPickerList();
	RefreshComponentEditWidget();
}

void SDMEditor::OnValuesUpdated(UDynamicMaterialModel* InMaterialModel)
{
	RefreshParametersList();
}

void SDMEditor::OnSlotsUpdated(UDynamicMaterialModel* InMaterialModel)
{
	RefreshSlotWidget();
}

bool SDMEditor::IsPropertyValidForModel(EDMMaterialPropertyType InProperty) const
{
	UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get();

	if (!MaterialModel)
	{
		return false;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

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

	if (InProperty == EDMMaterialPropertyType::BaseColor)
	{
		if (UDMMaterialProperty* Property = EditorOnlyData->GetMaterialProperty(EDMMaterialPropertyType::EmissiveColor))
		{
			return Property->IsValidForModel(*EditorOnlyData);
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

ECheckBoxState SDMEditor::GetSlotCheckState(EDMMaterialPropertyType InProperty)  const
{
	if (InProperty == EDMMaterialPropertyType::None)
	{
		return ActiveSlotIndex == INDEX_NONE
			? ECheckBoxState::Checked
			: ECheckBoxState::Unchecked;
	}

	if (ActiveSlotIndex == INDEX_NONE)
	{
		return ECheckBoxState::Unchecked;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak);

	if (!EditorOnlyData)
	{
		return ECheckBoxState::Undetermined;
	}

	if (!EditorOnlyData->GetSlots().IsValidIndex(ActiveSlotIndex))
	{
		return ECheckBoxState::Unchecked;
	}

	UDMMaterialSlot* PropertySlot = GetSlotForMaterialProperty(InProperty);

	return EditorOnlyData->GetSlots()[ActiveSlotIndex] == PropertySlot
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
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
	if (InCheckState != ECheckBoxState::Checked)
	{
		return;
	}

	if (InProperty == EDMMaterialPropertyType::None)
	{
		SetActiveSlotIndex(INDEX_NONE);
		return;
	}

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak);

	if (!ModelEditorOnlyData)
	{
		SetActiveSlotIndex(INDEX_NONE);
		return;
	}

	UDMMaterialSlot* PropertySlot = GetSlotForMaterialProperty(InProperty);

	const TArray<UDMMaterialSlot*>& Slots = ModelEditorOnlyData->GetSlots();

	if (Slots.IsEmpty())
	{
		SetActiveSlotIndex(INDEX_NONE);
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (Slots[SlotIndex] == PropertySlot)
		{
			SetActiveSlotIndex(SlotIndex);
			return;
		}
	}

	SetActiveSlotIndex(INDEX_NONE);
	return;
}

FReply SDMEditor::OnCreateMaterialButtonClicked(FDMObjectMaterialProperty InMaterialProperty)
{
	UDynamicMaterialModel* NewModel = UDMBlueprintFunctionLibrary::CreateDynamicMaterialInObject(InMaterialProperty);

	if (NewModel)
	{
		if (Toolbar.IsValid())
		{
			Toolbar->SetMaterialModel(NewModel);
		}

		if (AActor* Actor = InMaterialProperty.GetTypedOuter<AActor>())
		{
			if (Toolbar->GetMaterialActor() != Actor)
			{
				Toolbar->SetMaterialActor(Actor);
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
	ClearEditor();
	SetMaterialModel(MaterialModelWeak.Get());
}

bool SDMEditor::CanAddNewLayer() const
{
	return ActiveSlotWidget.IsValid() && !!UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak);
}

void SDMEditor::AddNewLayer()
{
	if (ActiveSlotWidget.IsValid())
	{
		ActiveSlotWidget->AddNewLayer_Expression(
			TSubclassOf<UDMMaterialStageExpression>(UDMMaterialStageExpressionTextureSample::StaticClass()),
			EDMMaterialLayerStage::All
		);

		ActiveSlotWidget->InvalidateMainWidget();
	}
}

bool SDMEditor::CanInsertNewLayer() const
{
	return ActiveSlotWidget.IsValid()
		&& ActiveSlotWidget->GetSelectedLayerIndices().Num() == 1
		&& MaterialModelWeak.IsValid();
}

void SDMEditor::InsertNewLayer()
{
	if (ActiveSlotWidget.IsValid())
	{
		if (UDMMaterialLayerObject* SelectedLayer = ActiveSlotWidget->GetSelectedLayer())
		{
			if (UDMMaterialSlot* Slot = SelectedLayer->GetSlot())
			{
				// Added here because stuff is done after the layer is added
				FDMScopedUITransaction Transaction(LOCTEXT("InsertNewLayer", "Material Designer Insert Layer"));
				Slot->Modify();

				ActiveSlotWidget->AddNewLayer_Expression(
					TSubclassOf<UDMMaterialStageExpression>(UDMMaterialStageExpressionTextureSample::StaticClass()),
					EDMMaterialLayerStage::All
				);

				Slot->MoveLayerAfter(Slot->GetLayers().Last(), SelectedLayer);

				ActiveSlotWidget->InvalidateMainWidget();
			}
		}
	}
}

bool SDMEditor::CanCopySelectedLayer() const
{
	return ActiveSlotWidget.IsValid()
		&& ActiveSlotWidget->GetSelectedLayerIndices().Num() == 1
		&& MaterialModelWeak.IsValid();
}

void SDMEditor::CopySelectedLayer()
{
	if (ActiveSlotWidget.IsValid() && ActiveSlotWidget->GetSelectedLayerIndices().Num() == 1)
	{
		if (const UDMMaterialLayerObject* Layer = ActiveSlotWidget->GetSelectedLayer())
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

				ActiveSlotWidget->InvalidateMainWidget();
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
	if (ActiveSlotWidget.IsValid() && ActiveSlotWidget->GetSelectedLayerIndices().IsEmpty() == false)
	{
		return ActiveSlotWidget->GetLayerRowsButtonsCanRemove();
	}

	return false;
}

void SDMEditor::DeleteSelectedLayer()
{
	if (ActiveSlotWidget.IsValid() && ActiveSlotWidget->GetSelectedLayerIndices().IsEmpty() == false)
	{
		ActiveSlotWidget->OnLayerRowButtonsRemoveClicked();
	}
}

bool SDMEditor::GetExpansionState(UObject* InOwner, FName InName, bool& bOutExpanded)
{
	const FExpansionItem ExpansionItem = {InOwner, InName};

	if (const bool* State = ExpansionStates.Find(ExpansionItem))
	{
		bOutExpanded = *State;
		return true;
	}

	return false;
}

void SDMEditor::SetExpansionState(UObject* InOwner, FName InName, bool bInIsExpanded)
{
	const FExpansionItem ExpansionItem = {InOwner, InName};
	ExpansionStates.FindOrAdd(ExpansionItem) = bInIsExpanded;
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

TSharedRef<SWidget> SDMEditor::CreateSlotWidget()
{
	ActiveSlotWidget.Reset();

	auto CreateEmptySlotsContent = []() -> TSharedRef<SBox>
	{
		return 
			SNew(SBox)
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(10.f, 5.f, 10.f, 5.f)
			[
				SNew(STextBlock)
				.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
				.Text(LOCTEXT("SlotsContent", "Select a Material Property to view the Layer Stack."))
			];
	};

	if (ActiveSlotIndex == INDEX_NONE)
	{
		return CreateEmptySlotsContent();
	}
	
	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak);

	if (!ModelEditorOnlyData)
	{
		return CreateEmptySlotsContent();
	}

	const TArray<UDMMaterialSlot*>& Slots = ModelEditorOnlyData->GetSlots();

	if (Slots.IsEmpty() || !Slots.IsValidIndex(ActiveSlotIndex))
	{
		return CreateEmptySlotsContent();
	}

	ActiveSlotWidget =
		SNew(SDMSlot, SharedThis(this), Slots[ActiveSlotIndex])
		.SlotPreviewSize_Lambda([]() { return UDynamicMaterialEditorSettings::Get()->SlotPreviewSize; })
		.LayerPreviewSize_Lambda([]() { return UDynamicMaterialEditorSettings::Get()->LayerPreviewSize; });

	return ActiveSlotWidget.ToSharedRef();
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

UDMMaterialSlot* SDMEditor::GetSlotForMaterialProperty(EDMMaterialPropertyType InProperty) const
{
	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak);

	if (!ModelEditorOnlyData)
	{
		return nullptr;
	}

	UDMMaterialSlot* PropertySlot = ModelEditorOnlyData->GetSlotForMaterialProperty(InProperty);

	if (!PropertySlot)
	{
		switch (InProperty)
		{
			case EDMMaterialPropertyType::BaseColor:
				PropertySlot = ModelEditorOnlyData->GetSlotForMaterialProperty(EDMMaterialPropertyType::EmissiveColor);
				break;

			case EDMMaterialPropertyType::EmissiveColor:
				PropertySlot = ModelEditorOnlyData->GetSlotForMaterialProperty(EDMMaterialPropertyType::BaseColor);
				break;

			case EDMMaterialPropertyType::Opacity:
				PropertySlot = ModelEditorOnlyData->GetSlotForMaterialProperty(EDMMaterialPropertyType::OpacityMask);
				break;

			case EDMMaterialPropertyType::OpacityMask:
				PropertySlot = ModelEditorOnlyData->GetSlotForMaterialProperty(EDMMaterialPropertyType::Opacity);
				break;
		}
	}

	return PropertySlot;
}

UDMMaterialComponent* SDMEditor::GetEditedComponent() const
{
	return EditedComponent.Get();
}

void SDMEditor::SetEditedComponent(UDMMaterialComponent* InComponent)
{
	if (EditedComponent.IsValid())
	{
		EditedComponent->GetOnUpdate().RemoveAll(this);
	}

	EditedComponent = InComponent;

	if (EditedComponent.IsValid())
	{
		EditedComponent->GetOnUpdate().AddSP(this, &SDMEditor::OnComponentUpdated);
	}

	InvalidateComponentEditWidget();
}

TSharedRef<SWidget> SDMEditor::CreateComponentEditWidget()
{
	return
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(3.0f)
		.BorderImage(FDynamicMaterialEditorStyle::GetBrush("LayerView.Details.Background"))
		[
			SNew(SDMComponentEdit, EditedComponent.Get(), SharedThis(this))
		];
}

void SDMEditor::InvalidateComponentEditWidget()
{
	bInvalidateComponentEditWidget = true;
}

void SDMEditor::RefreshComponentEditWidget()
{
	bInvalidateComponentEditWidget = false;

	if (ComponentEditContainer.IsValid())
	{
		ComponentEditContainer->ClearChildren();
		ComponentEditContainer->AddSlot()
			.AutoSize()
			[
				CreateComponentEditWidget()
			];
	}
}

void SDMEditor::OnSplitterResized() const
{
	UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();

	if (LayerViewSplitterSlot)
	{
		Settings->SplitterLocation = LayerViewSplitterSlot->GetSizeValue();
		Settings->SaveConfig();
	}
}

void SDMEditor::OnComponentUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (InUpdateType == EDMUpdateType::Structure)
	{
		InvalidateComponentEditWidget();
	}
}

#undef LOCTEXT_NAMESPACE
