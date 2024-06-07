// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/SDMToolBar.h"
#include "ContentBrowserModule.h"
#include "DMWorldSubsystem.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineAnalytics.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "Material/DynamicMaterialInstance.h"
#include "Model/DynamicMaterialModel.h"
#include "PackageTools.h"
#include "SDMEditor.h"
#include "Selection.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "UObject/Package.h"
#include "Utils/DMBlueprintFunctionLibrary.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMToolBar"

void SDMToolBar::Construct(const FArguments& InArgs, const TSharedRef<SDMEditor>& InEditor)
{
	SetCanTick(true);

	EditorWeak = InEditor;
	MaterialActorWeak = InArgs._MaterialActor;
	MaterialModelWeak = InArgs._MaterialModel;
	OnSlotChanged = InArgs._OnSlotChanged;
	OnGetSettingsMenu = InArgs._OnGetSettingsMenu;
	
	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	[
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.BorderImage(FDynamicMaterialEditorStyle::GetBrush("Border.Bottom"))
		.BorderBackgroundColor(FLinearColor(1, 1, 1, 0.05f))
		[
			CreateToolBarEntries()
		]
	];

	SetMaterialModel(MaterialModelWeak.Get());
	SetMaterialActor(MaterialActorWeak.Get());
}

TSharedRef<SWidget> SDMToolBar::CreateToolBarEntries()
{
	return 
		SNew(SHorizontalBox)
		
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SNew(SWrapBox)
			.Orientation(Orient_Horizontal)
			.UseAllottedSize(true)
			.HAlign(HAlign_Left)
			.InnerSlotPadding(FVector2D(5.0f))
			+ SWrapBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SDMToolBar::GetActorVisibility)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 5.f, 0.f)
				[
					SNew(STextBlock)
					.TextStyle(FDynamicMaterialEditorStyle::Get(), "ActorName")
					.Text(LOCTEXT("Actor", "Actor"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.TextStyle(FDynamicMaterialEditorStyle::Get(), "ActorName")
					.Text(this, &SDMToolBar::GetActorName)
				]
			]
			+ SWrapBox::Slot()
			.FillEmptySpace(false)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(SlotSelectorContainer, SBox)
				.Visibility(this, &SDMToolBar::GetActorVisibility)
				[
					CreateSlotsComboBoxWidget()
				]
			]
			+ SWrapBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SDMToolBar::GetAssetVisibility)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 5.f, 0.f)
				[
					SNew(STextBlock)
					.TextStyle(FDynamicMaterialEditorStyle::Get(), "ActorName")
					.Text(LOCTEXT("Asset", "Asset"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.TextStyle(FDynamicMaterialEditorStyle::Get(), "ActorName")
					.Text(this, &SDMToolBar::GetAssetName)
					.ToolTipText(this, &SDMToolBar::GetAssetToolTip)
				]
			]
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SAssignNew(BrowseButton, SButton)
			.Visibility(EVisibility::Collapsed)
			.ContentPadding(GetLargeIconToolBarButtonContentPadding())
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
			.ToolTipText(LOCTEXT("MaterialDesignerBrowseTooltip", "Browse to the selected asset in the content browser."))
			.OnClicked(this, &SDMToolBar::OnBrowseClicked)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(TEXT("Icons.BrowseContent")))
				.DesiredSizeOverride(GetLargeIconToolBarButtonSize())
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SAssignNew(UseButton, SButton)
			.Visibility(EVisibility::Collapsed)
			.ContentPadding(GetLargeIconToolBarButtonContentPadding())
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
			.ToolTipText(LOCTEXT("MaterialDesignerUseTooltip", "Replace the material in this slot with the one selected in the content browser."))
			.OnClicked(this, &SDMToolBar::OnUseClicked)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(TEXT("Icons.Use")))
				.DesiredSizeOverride(GetLargeIconToolBarButtonSize())
			]
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SAssignNew(SaveButton, SButton)
			.IsEnabled(this, &SDMToolBar::CanSave)
			.ContentPadding(GetLargeIconToolBarButtonContentPadding())
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
			.ToolTipText(LOCTEXT("MaterialDesignerBrowseTooltip", "Save the Material Designer asset\n\nCaution: If this asset lives inside an actor, the actor/level will be saved."))
			.OnClicked(this, &SDMToolBar::OnSaveClicked)
			[
				SNew(SImage)
				.Image(this, &SDMToolBar::GetSaveIcon)
				.DesiredSizeOverride(GetLargeIconToolBarButtonSize())
			]
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.ContentPadding(GetDefaultToolBarButtonContentPadding())
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
			.ToolTipText(LOCTEXT("MaterialDesignerFollowSelectionTooltip", "Toggles whether the Material Designer display will change when selecting new objects and actors."))
			.OnClicked(this, &SDMToolBar::OnFollowSelectionButtonClicked)
			[
				SNew(SImage)
				.Image(this, &SDMToolBar::GetFollowSelectionBrush)
				.DesiredSizeOverride(GetDefaultToolBarButtonSize())
				.ColorAndOpacity(this, &SDMToolBar::GetFollowSelectionColor)
			]
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.ContentPadding(GetLargeIconToolBarButtonContentPadding())
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
			.ToolTipText(LOCTEXT("ExportMaterialInstance", "Export Material Designer Instance"))
			.Visibility(this, &SDMToolBar::GetExportMaterialInstanceButtonVisibility)
			.OnClicked(this, &SDMToolBar::OnExportMaterialInstanceButtonClicked)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush(TEXT("Icons.Toolbar.Export")))
				.DesiredSizeOverride(GetLargeIconToolBarButtonSize())
			]
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SComboButton)
			.HasDownArrow(false)
			.IsFocusable(true)
			.ContentPadding(GetDefaultToolBarButtonContentPadding())
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
			.ToolTipText(LOCTEXT("MaterialDesignerSettingsTooltip", "Material Designer Settings"))
			.OnGetMenuContent(OnGetSettingsMenu)
			.ButtonContent()
			[
				SNew(SImage)
				.Image(FDynamicMaterialEditorStyle::GetBrush("Icons.Menu.Dropdown"))
				.DesiredSizeOverride(GetDefaultToolBarButtonSize())
			]
		];
}

TSharedRef<SWidget> SDMToolBar::CreateToolBarButton(TAttribute<const FSlateBrush*> InImageBrush, const TAttribute<FText>& InTooltipText, FOnClicked InOnClicked)
{
	return 
		SNew(SButton)
		.ContentPadding(GetDefaultToolBarButtonContentPadding())
		.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText(InTooltipText)
		.OnClicked(InOnClicked)
		[
			SNew(SImage)
			.Image(InImageBrush)
			.DesiredSizeOverride(GetDefaultToolBarButtonSize())
		];
}

TSharedRef<SWidget> SDMToolBar::CreateSlotsComboBoxWidget()
{
	if (!MaterialActorWeak.IsValid() || !MaterialModelWeak.IsValid() || ActorMaterialProperties.Num() <= 1)
	{
		return SNullWidget::NullWidget;
	}

	const TSharedPtr<FDMObjectMaterialProperty> InitiallySelectedItem =
		ActorMaterialProperties.IsValidIndex(SelectedMaterialSlotIndex) ? ActorMaterialProperties[SelectedMaterialSlotIndex] : nullptr;

	return 
		SNew(SComboBox<TSharedPtr<FDMObjectMaterialProperty>>)
		.InitiallySelectedItem(InitiallySelectedItem)
		.OptionsSource(&ActorMaterialProperties)
		.OnGenerateWidget(this, &SDMToolBar::GenerateSelectedMaterialSlotRow)
		.OnSelectionChanged(this, &SDMToolBar::OnMaterialSlotChanged)
		[
			SNew(STextBlock)
			.MinDesiredWidth(100.0f)
			.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
			.Text(this, &SDMToolBar::GetSelectedMaterialSlotName)
		];
}

TSharedRef<SWidget> SDMToolBar::GenerateSelectedMaterialSlotRow(TSharedPtr<FDMObjectMaterialProperty> InSelectedSlot) const
{
	if (InSelectedSlot.IsValid())
	{
		return SNew(STextBlock)
			.MinDesiredWidth(100.f)
			.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
			.Text(this, &SDMToolBar::GetSlotDisplayName, InSelectedSlot);
	}
	return SNullWidget::NullWidget;
}

FText SDMToolBar::GetSlotDisplayName(TSharedPtr<FDMObjectMaterialProperty> InSlot) const
{
	return InSlot->GetPropertyName(false);
}

FText SDMToolBar::GetSelectedMaterialSlotName() const
{
	if (ActorMaterialProperties.IsValidIndex(SelectedMaterialSlotIndex) && ActorMaterialProperties[SelectedMaterialSlotIndex].IsValid())
	{
		return GetSlotDisplayName(ActorMaterialProperties[SelectedMaterialSlotIndex]);
	}
	return FText::GetEmpty();
}

void SDMToolBar::OnMaterialSlotChanged(TSharedPtr<FDMObjectMaterialProperty> InSelectedSlot, ESelectInfo::Type InSelectInfoType)
{
	if (!InSelectedSlot.IsValid())
	{
		return;
	}

	UDynamicMaterialModel* SelectedMaterialModel = InSelectedSlot->GetMaterialModel();

	if (IsValid(SelectedMaterialModel))
	{
		SetMaterialModel(SelectedMaterialModel);
	}
	else if (InSelectedSlot->OuterWeak.IsValid())
	{
		UDMBlueprintFunctionLibrary::CreateDynamicMaterialInObject(*InSelectedSlot.Get());
	}

	AActor* SlotActor = InSelectedSlot->GetTypedOuter<AActor>();

	if (GetMaterialActor() != SlotActor)
	{
		SetMaterialActor(SlotActor);
	}

	OnSlotChanged.ExecuteIfBound(InSelectedSlot);
}

EVisibility SDMToolBar::GetSlotsComboBoxWidgetVisibiltiy() const
{
	return ActorMaterialProperties.Num() > 1 ? EVisibility::Visible : EVisibility::Collapsed;
}

void SDMToolBar::SetMaterialProperties(const TArray<TSharedPtr<FDMObjectMaterialProperty>>& InActorMaterialProperties)
{
	ActorMaterialProperties = InActorMaterialProperties;
	SelectedMaterialSlotIndex = 0;
}

void SDMToolBar::SetMaterialModel(UDynamicMaterialModel* InModel)
{
	if (!IsValid(InModel))
	{
		return;
	}

	MaterialModelWeak = InModel;

	bool bIsAsset = false;

	if (InModel->IsAsset())
	{
		bIsAsset = true;
	}
	else if (UDynamicMaterialInstance* MaterialInstance = InModel->GetDynamicMaterialInstance())
	{
		if (MaterialInstance->IsAsset())
		{
			bIsAsset = true;
		}
	}

	BrowseButton->SetVisibility(bIsAsset ? EVisibility::Visible : EVisibility::Collapsed);

	// If this is a valid actor material slot, this will be updated by a SetMaterialActor call.
	UseButton->SetVisibility(EVisibility::Collapsed);

	ActorMaterialProperties.Empty(0);
	SelectedMaterialSlotIndex = INDEX_NONE;
}

void SDMToolBar::SetMaterialActor(AActor* InActor, const int32 InActiveSlotIndex)
{
	MaterialActorWeak = InActor;
	ActorMaterialProperties.Empty();
	SelectedMaterialSlotIndex = 0;

	UseButton->SetVisibility(MaterialActorWeak.IsValid() ? EVisibility::Visible : EVisibility::Collapsed);

	if (IsValid(InActor))
	{
		TArray<FDMObjectMaterialProperty> ActorProperties = UDMBlueprintFunctionLibrary::GetActorMaterialProperties(InActor);
		UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get();

		for (int32 MaterialPropertyIdx = 0; MaterialPropertyIdx < ActorProperties.Num(); ++MaterialPropertyIdx)
		{
			const FDMObjectMaterialProperty& MaterialProperty = ActorProperties[MaterialPropertyIdx];

			ActorMaterialProperties.Add(MakeShared<FDMObjectMaterialProperty>(MaterialProperty));

			if (MaterialProperty.GetMaterialModel() == MaterialModel)
			{
				SelectedMaterialSlotIndex = MaterialPropertyIdx;
			}
		}
	}

	SlotSelectorContainer->SetContent(CreateSlotsComboBoxWidget());
}

const FSlateBrush* SDMToolBar::GetFollowSelectionBrush() const
{
	static const FSlateBrush* Unlocked = FAppStyle::Get().GetBrush("Icons.Unlock");
	static const FSlateBrush* Locked = FAppStyle::Get().GetBrush("Icons.Lock");

	if (UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get())
	{
		if (!Settings->bFollowSelection)
		{
			return Locked;
		}
	}

	return Unlocked;
}

FSlateColor SDMToolBar::GetFollowSelectionColor() const
{
	// We want the icon to stand out when it's locked.
	static FSlateColor EnabledColor = FSlateColor(EStyleColor::AccentGray);
	static FSlateColor DisabledColor = FSlateColor(EStyleColor::Primary);

	if (UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get())
	{
		if (Settings->bFollowSelection)
		{
			return EnabledColor;
		}
	}

	return DisabledColor;
}

FReply SDMToolBar::OnFollowSelectionButtonClicked()
{
	if (UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get())
	{
		Settings->bFollowSelection = !Settings->bFollowSelection;
		Settings->SaveConfig();
	}

	return FReply::Handled();
}

EVisibility SDMToolBar::GetExportMaterialInstanceButtonVisibility() const
{
	UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get();

	if (!MaterialModel)
	{
		return EVisibility::Collapsed;
	}

	UDynamicMaterialInstance* MaterialInstance = MaterialModel->GetDynamicMaterialInstance();

	if (!MaterialInstance)
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

FReply SDMToolBar::OnExportMaterialInstanceButtonClicked()
{
	UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get();

	if (!MaterialModel)
	{
		return FReply::Handled();
	}

	UDynamicMaterialInstance* MaterialInstance = MaterialModel->GetDynamicMaterialInstance();

	if (!MaterialInstance)
	{
		return FReply::Handled();
	}

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = "/Game";
	SaveAssetDialogConfig.DefaultAssetName = MaterialInstance->GetName();
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (!SaveObjectPath.IsEmpty())
	{
		UDMBlueprintFunctionLibrary::ExportMaterialInstance(MaterialInstance->GetMaterialModel(), SaveObjectPath);

		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportedMaterialInstance"));
		}
	}

	return FReply::Handled();
}

FReply SDMToolBar::OnBrowseClicked()
{
	UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get();

	if (!IsValid(MaterialModel))
	{
		return FReply::Handled();
	}

	UObject* Asset = nullptr;

	if (MaterialModelWeak->IsAsset())
	{
		Asset = MaterialModel;
	}
	else if (UDynamicMaterialInstance* MaterialInstance = MaterialModel->GetDynamicMaterialInstance())
	{
		if (MaterialInstance->IsAsset())
		{
			Asset = MaterialInstance;
		}
	}

	if (!Asset)
	{
		return FReply::Handled();
	}

	TArray<FAssetData> AssetDataList;
	AssetDataList.Add(Asset);
	GEditor->SyncBrowserToObjects(AssetDataList);

	return FReply::Handled();
}

FReply SDMToolBar::OnUseClicked()
{
	if (!ActorMaterialProperties.IsValidIndex(SelectedMaterialSlotIndex))
	{
		return FReply::Handled();
	}

	UDynamicMaterialModel* CurrentModel = MaterialModelWeak.Get();
	UDMWorldSubsystem* DMSubsystem = nullptr;

	if (CurrentModel)
	{
		AActor* Actor = MaterialActorWeak.Get();

		if (!IsValid(Actor))
		{
			return FReply::Handled();
		}

		UWorld* World = Actor->GetWorld();

		if (!IsValid(World))
		{
			return FReply::Handled();
		}

		DMSubsystem = World->GetSubsystem<UDMWorldSubsystem>();

		if (!DMSubsystem)
		{
			return FReply::Handled();
		}
	}

	USelection* Selection = GEditor->GetSelectedObjects();

	if (!Selection)
	{
		return FReply::Handled();
	}

	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	UDynamicMaterialInstance* SelectedInstance = nullptr;

	TArray<UDynamicMaterialInstance*> SelectedInstances;
	Selection->GetSelectedObjects(SelectedInstances);

	for (UDynamicMaterialInstance* SelectedInstanceIter : SelectedInstances)
	{
		if (!IsValid(SelectedInstanceIter) || !SelectedInstanceIter->IsAsset())
		{
			continue;
		}

		SelectedInstance = SelectedInstanceIter;
		break;
	}

	if (!SelectedInstance)
	{
		return FReply::Handled();
	}

	TSharedPtr<FDMObjectMaterialProperty> CurrentActorProperty = ActorMaterialProperties[SelectedMaterialSlotIndex];

	if (DMSubsystem && DMSubsystem->GetMaterialValueSetterDelegate().IsBound())
	{
		if (!DMSubsystem->ExecuteIsValidDelegate(CurrentModel))
		{
			return FReply::Handled();
		}

		DMSubsystem->ExecuteMaterialValueSetterDelegate(*CurrentActorProperty, SelectedInstance);
	}
	else
	{
		ActorMaterialProperties[SelectedMaterialSlotIndex]->SetMaterial(SelectedInstance);
	}

	if (TSharedPtr<SDMEditor> Editor = EditorWeak.Pin())
	{
		Editor->SetMaterialObjectProperty(*CurrentActorProperty);
	}

	return FReply::Handled();
}

UPackage* SDMToolBar::GetSaveablePackage(UObject* InObject)
{
	if (!IsValid(InObject))
	{
		return nullptr;
	}

	UPackage* Package = InObject->GetPackage();

	if (!Package || Package->HasAllFlags(RF_Transient))
	{
		return nullptr;
	}

	return Package;
}

FText SDMToolBar::GetActorName() const
{
	if (const AActor* const SlotActor = GetMaterialActor())
	{
		return FText::FromString(SlotActor->GetActorLabel());
	}

	return FText::GetEmpty();
}

EVisibility SDMToolBar::GetActorVisibility() const
{
	return GetMaterialActor()
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SDMToolBar::GetAssetVisibility() const
{
	if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
	{
		if (UDynamicMaterialInstance* MaterialInstance = MaterialModel->GetDynamicMaterialInstance())
		{
			if (MaterialInstance->IsAsset())
			{
				return EVisibility::Visible;
			}
		}
		else if (MaterialModel->IsAsset())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

FText SDMToolBar::GetAssetName() const
{
	if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
	{
		if (UDynamicMaterialInstance* MaterialInstance = MaterialModel->GetDynamicMaterialInstance())
		{
			if (MaterialInstance->IsAsset())
			{
				return FText::FromString(MaterialInstance->GetName());
			}
		}
		else if (MaterialModel->IsAsset())
		{
			return FText::FromString(MaterialModel->GetName());
		}
	}

	return FText::GetEmpty();
}

FText SDMToolBar::GetAssetToolTip() const
{
	if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
	{
		if (UDynamicMaterialInstance* MaterialInstance = MaterialModel->GetDynamicMaterialInstance())
		{
			if (MaterialInstance->IsAsset())
			{
				return FText::FromString(MaterialInstance->GetPathName());
			}
		}
		else if (MaterialModel->IsAsset())
		{
			return FText::FromString(MaterialModel->GetPathName());
		}
	}

	return FText::GetEmpty();
}

bool SDMToolBar::CanSave() const
{
	return !!GetSaveablePackage(GetMaterialModel());
}

const FSlateBrush* SDMToolBar::GetSaveIcon() const
{
	if (UPackage* Package = GetSaveablePackage(GetMaterialModel()))
	{
		if (Package->IsDirty())
		{
			return FAppStyle::Get().GetBrush("Icons.SaveModified");
		}
	}

	return FAppStyle::Get().GetBrush("Icons.Save");
}

FReply SDMToolBar::OnSaveClicked()
{
	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		if (UPackage* Package = GetSaveablePackage(MaterialModel))
		{
			TArray<UObject*> TexturesToSaveArray;
			TexturesToSaveArray.Add(MaterialModel);
			UPackageTools::SavePackagesForObjects(TexturesToSaveArray);
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
