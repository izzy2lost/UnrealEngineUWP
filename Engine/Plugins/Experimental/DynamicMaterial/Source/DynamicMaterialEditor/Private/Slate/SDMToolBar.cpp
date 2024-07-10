// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/SDMToolBar.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "DMWorldSubsystem.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineAnalytics.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelDynamic.h"
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

	OnMaterialModelChanged();
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
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SAssignNew(SaveButtonWidget, SButton)
					.IsEnabled(false)
					.ContentPadding(GetLargeIconToolBarButtonContentPadding())
					.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
					.ToolTipText(LOCTEXT("MaterialDesignerSaveTooltip", "Save the Material Designer asset\n\nCaution: If this asset lives inside an actor, the actor/level will be saved."))
					.OnClicked(this, &SDMToolBar::OnSaveClicked)
					[
						SNew(SImage)
						.Image(this, &SDMToolBar::GetSaveIcon)
						.DesiredSizeOverride(GetLargeIconToolBarButtonSize())
					]
				]		
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(SButton)
					.ContentPadding(GetLargeIconToolBarButtonContentPadding())
					.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
					.ToolTipText(LOCTEXT("ExportMaterialInstance", "Save As"))
					.OnClicked(this, &SDMToolBar::OnExportMaterialInstanceButtonClicked)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("AssetEditor.SaveAssetAs"))
						.DesiredSizeOverride(GetLargeIconToolBarButtonSize())
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 0.f, 0.f)
				[
					SAssignNew(OpenParentButton, SButton)
					.Visibility(EVisibility::Collapsed)
					.ContentPadding(GetLargeIconToolBarButtonContentPadding())
					.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
					.ToolTipText(LOCTEXT("MaterialDesignerOpenParentTooltip", "Open the parent of this Material Designer Dynamic."))
					.OnClicked(this, &SDMToolBar::OnOpenParentClicked)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(TEXT("Icons.Blueprints")))
						.DesiredSizeOverride(GetLargeIconToolBarButtonSize())
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 0.f, 0.f)
				[
					SAssignNew(ConvertToEditableButton, SButton)
					.Visibility(EVisibility::Collapsed)
					.ContentPadding(GetLargeIconToolBarButtonContentPadding())
					.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
					.ToolTipText(LOCTEXT("MaterialDesignerConvertToEditableTooltip", "Convert this Material Designer Dyanmic to a fully editable material (and create a new shader)."))
					.OnClicked(this, &SDMToolBar::OnConvertToEditableClicked)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(TEXT("Icons.Edit")))
						.DesiredSizeOverride(GetLargeIconToolBarButtonSize())
					]
				]
			]

			+ SWrapBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(AssetRowWidget, SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
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
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 0.f, 0.f)
				[
					SAssignNew(AssetNameWidget, STextBlock)
					.TextStyle(FDynamicMaterialEditorStyle::Get(), "ActorName")
				]
			]

			+ SWrapBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(ActorRowWidget, SHorizontalBox)
				.Visibility(EVisibility::Collapsed)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 5.f, 0.f)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("ClassIcon.Actor"))
					.DesiredSizeOverride(GetLargeIconToolBarButtonSize())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 0.f, 0.f)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SAssignNew(ActorNameWidget, STextBlock)
					.TextStyle(FDynamicMaterialEditorStyle::Get(), "ActorName")
				]	
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(SlotSelectorContainer, SBox)
					[
						CreateSlotsComboBoxWidget()
					]
				]	
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
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
	UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

	if (!MaterialActorWeak.IsValid() || !IsValid(MaterialModelBase))
	{
		return SNullWidget::NullWidget;
	}

	const TSharedPtr<FDMObjectMaterialProperty> InitiallySelectedItem =
		ActorMaterialProperties.IsValidIndex(SelectedMaterialSlotIndex) ? ActorMaterialProperties[SelectedMaterialSlotIndex] : nullptr;

	return 
		SNew(SComboBox<TSharedPtr<FDMObjectMaterialProperty>>)
		.IsEnabled(ActorMaterialProperties.Num() > 1)
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

	UDynamicMaterialModelBase* SelectedMaterialModelBase = InSelectedSlot->GetMaterialModelBase();

	if (IsValid(SelectedMaterialModelBase))
	{
		OnMaterialModelChanged();
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

void SDMToolBar::SetMaterialProperties(const TArray<TSharedPtr<FDMObjectMaterialProperty>>& InActorMaterialProperties)
{
	ActorMaterialProperties = InActorMaterialProperties;
	SelectedMaterialSlotIndex = 0;
}

void SDMToolBar::OnMaterialModelChanged()
{
	bool bIsAsset = false;
	bool bIsDynamic = false;

	UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

	if (IsValid(MaterialModelBase))
	{
		if (MaterialModelBase->IsAsset())
		{
			bIsAsset = true;
		}
		else if (UDynamicMaterialInstance* MaterialInstance = MaterialModelBase->GetDynamicMaterialInstance())
		{
			if (MaterialInstance->IsAsset())
			{
				bIsAsset = true;
			}
		}

		bIsDynamic = !MaterialModelBase->IsA<UDynamicMaterialModel>();
	}

	SaveButtonWidget->SetEnabled(CanSave());

	if (bIsAsset)
	{
		AssetNameWidget->SetText(GetAssetName());
		AssetNameWidget->SetToolTipText(GetAssetToolTip());
		AssetNameWidget->SetVisibility(EVisibility::Visible);

		AssetRowWidget->SetVisibility(EVisibility::Visible);
	}
	else
	{
		AssetNameWidget->SetText(FText::GetEmpty());
		AssetNameWidget->SetToolTipText(FText::GetEmpty());
		AssetNameWidget->SetVisibility(EVisibility::Collapsed);

		AssetRowWidget->SetVisibility(EVisibility::Collapsed);
	}

	if (bIsDynamic)
	{
		OpenParentButton->SetVisibility(EVisibility::Visible);
		ConvertToEditableButton->SetVisibility(EVisibility::Visible);
	}
	else
	{
		OpenParentButton->SetVisibility(EVisibility::Collapsed);
		ConvertToEditableButton->SetVisibility(EVisibility::Collapsed);
	}

	ActorMaterialProperties.Empty(0);
	SelectedMaterialSlotIndex = INDEX_NONE;
}

void SDMToolBar::SetMaterialActor(AActor* InActor, const int32 InActiveSlotIndex)
{
	MaterialActorWeak = InActor;
	ActorMaterialProperties.Empty();
	SelectedMaterialSlotIndex = 0;

	if (IsValid(InActor))
	{
		ActorNameWidget->SetText(GetActorName());
		ActorRowWidget->SetVisibility(EVisibility::Visible);

		TArray<FDMObjectMaterialProperty> ActorProperties = UDMBlueprintFunctionLibrary::GetActorMaterialProperties(InActor);
		UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

		for (int32 MaterialPropertyIdx = 0; MaterialPropertyIdx < ActorProperties.Num(); ++MaterialPropertyIdx)
		{
			const FDMObjectMaterialProperty& MaterialProperty = ActorProperties[MaterialPropertyIdx];

			ActorMaterialProperties.Add(MakeShared<FDMObjectMaterialProperty>(MaterialProperty));

			if (MaterialProperty.GetMaterialModelBase() == MaterialModelBase)
			{
				SelectedMaterialSlotIndex = MaterialPropertyIdx;
			}
		}
	}
	else
	{
		ActorNameWidget->SetText(FText::GetEmpty());
		ActorRowWidget->SetVisibility(EVisibility::Collapsed);
	}

	SlotSelectorContainer->SetContent(CreateSlotsComboBoxWidget());
}

UDynamicMaterialModelBase* SDMToolBar::GetMaterialModelBase() const
{
	if (TSharedPtr<SDMEditor> Editor = EditorWeak.Pin())
	{
		return Editor->GetMaterialModelBase();
	}

	return nullptr;
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

FReply SDMToolBar::OnExportMaterialInstanceButtonClicked()
{
	UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

	if (!MaterialModelBase)
	{
		return FReply::Handled();
	}

	UDynamicMaterialInstance* MaterialInstance = MaterialModelBase->GetDynamicMaterialInstance();

	if (!MaterialInstance)
	{
		return FReply::Handled();
	}
	
	FString CurrentName = MaterialInstance->GetName();

	if (MaterialModelBase->IsA<UDynamicMaterialModel>())
	{
		CurrentName = CurrentName.StartsWith(TEXT("MDI_"))
			? CurrentName
			: (TEXT("MDI_") + CurrentName);
	}
	else
	{
		CurrentName = CurrentName.StartsWith(TEXT("MDD_"))
			? CurrentName
			: (TEXT("MDD_") + CurrentName);
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString PackageName, AssetName;
	AssetTools.CreateUniqueAssetName(CurrentName, TEXT(""), PackageName, AssetName);

	IContentBrowserSingleton& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	const FContentBrowserItemPath CurrentPath = ContentBrowserModule.GetCurrentPath();
	const FString PathStr = CurrentPath.HasInternalPath() ? CurrentPath.GetInternalPathString() : "/Game";

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = PathStr;
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
	SaveAssetDialogConfig.DefaultAssetName = AssetName;

	const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (!SaveObjectPath.IsEmpty())
	{
		UDMBlueprintFunctionLibrary::ExportMaterialInstance(MaterialInstance->GetMaterialModelBase(), SaveObjectPath);

		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportedMaterialInstance"));
		}
	}

	return FReply::Handled();
}

FReply SDMToolBar::OnBrowseClicked()
{
	UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

	if (!IsValid(MaterialModelBase))
	{
		return FReply::Handled();
	}

	UObject* Asset = nullptr;

	if (MaterialModelBase->IsAsset())
	{
		Asset = MaterialModelBase;
	}
	else if (UDynamicMaterialInstance* MaterialInstance = MaterialModelBase->GetDynamicMaterialInstance())
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

	UDynamicMaterialModelBase* CurrentModelBase = GetMaterialModelBase();
	UDMWorldSubsystem* DMSubsystem = nullptr;

	if (CurrentModelBase)
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
		if (!DMSubsystem->ExecuteIsValidDelegate(CurrentModelBase))
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

FText SDMToolBar::GetAssetName() const
{
	if (UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase())
	{
		if (UDynamicMaterialInstance* MaterialInstance = MaterialModelBase->GetDynamicMaterialInstance())
		{
			if (MaterialInstance->IsAsset())
			{
				return FText::FromString(MaterialInstance->GetName());
			}
		}
		else if (MaterialModelBase->IsAsset())
		{
			return FText::FromString(MaterialModelBase->GetName());
		}
	}

	return FText::GetEmpty();
}

FText SDMToolBar::GetAssetToolTip() const
{
	if (UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase())
	{
		if (UDynamicMaterialInstance* MaterialInstance = MaterialModelBase->GetDynamicMaterialInstance())
		{
			if (MaterialInstance->IsAsset())
			{
				return FText::FromString(MaterialInstance->GetPathName());
			}
		}
		else if (MaterialModelBase->IsAsset())
		{
			return FText::FromString(MaterialModelBase->GetPathName());
		}
	}

	return FText::GetEmpty();
}

bool SDMToolBar::CanSave() const
{
	return !!GetSaveablePackage(GetMaterialModelBase());
}

const FSlateBrush* SDMToolBar::GetSaveIcon() const
{
	if (UPackage* Package = GetSaveablePackage(GetMaterialModelBase()))
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
	if (UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase())
	{
		if (UPackage* Package = GetSaveablePackage(MaterialModelBase))
		{
			TArray<UObject*> AssetsToSave;
			AssetsToSave.Add(MaterialModelBase);
			UPackageTools::SavePackagesForObjects(AssetsToSave);
		}
	}

	return FReply::Handled();
}

FReply SDMToolBar::OnOpenParentClicked()
{
	if (TSharedPtr<SDMEditor> Editor = EditorWeak.Pin())
	{
		if (UDynamicMaterialModelDynamic* DynamicMaterialModel = Cast<UDynamicMaterialModelDynamic>(Editor->GetMaterialModelBase()))
		{
			if (UDynamicMaterialModel* ParentModel = DynamicMaterialModel->ResolveMaterialModel())
			{
				Editor->SetMaterialActor(nullptr);
				Editor->SetMaterialModelBase(ParentModel);
			}
		}
	}

	return FReply::Handled();
}

FReply SDMToolBar::OnConvertToEditableClicked()
{
	UDynamicMaterialModelDynamic* CurrentModelDynamic = Cast<UDynamicMaterialModelDynamic>(GetMaterialModelBase());

	if (!CurrentModelDynamic)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Tried to convert a null or non-dynamic model to editable."));
		return FReply::Handled();
	}

	UDynamicMaterialModel* ParentModel = CurrentModelDynamic->GetParentModel();

	if (!ParentModel)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to find parent model."));
		return FReply::Handled();
	}

	bool bIsAsset = false;

	if (CurrentModelDynamic->IsAsset())
	{
		bIsAsset = true;
	}
	else if (UDynamicMaterialInstance* MaterialInstance = CurrentModelDynamic->GetDynamicMaterialInstance())
	{
		if (MaterialInstance->IsAsset())
		{
			bIsAsset = true;
		}
	}

	UDMWorldSubsystem* DMSubsystem = nullptr;
	AActor* Actor = MaterialActorWeak.Get();
	TSharedPtr<FDMObjectMaterialProperty> CurrentActorProperty = nullptr;

	if (Actor && ActorMaterialProperties.IsValidIndex(SelectedMaterialSlotIndex) && ActorMaterialProperties[SelectedMaterialSlotIndex].IsValid())
	{
		UWorld* World = Actor->GetWorld();

		if (IsValid(World))
		{
			DMSubsystem = World->GetSubsystem<UDMWorldSubsystem>();
		}

		CurrentActorProperty = ActorMaterialProperties[SelectedMaterialSlotIndex];
	}

	// In-actor models/instance must have a world subsystem to query.
	if (!bIsAsset && !DMSubsystem)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Cannot create a new asset for embedded instances without an active world subsystem."));
		return FReply::Handled();
	}

	UDynamicMaterialInstance* OldInstance = CurrentModelDynamic->GetDynamicMaterialInstance();

	// Where should we save it? (Always export to CB)
	FString CurrentName = OldInstance ? OldInstance->GetName() : CurrentModelDynamic->GetName();

	CurrentName = CurrentName.StartsWith(TEXT("MDD_"))
		? (TEXT("MDI_") + CurrentName.RightChop(4))
		: (TEXT("MDI_") + CurrentName);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString PackageName, AssetName;
	AssetTools.CreateUniqueAssetName(CurrentName, TEXT(""), PackageName, AssetName);

	IContentBrowserSingleton& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	const FContentBrowserItemPath CurrentPath = ContentBrowserModule.GetCurrentPath();
	const FString PathStr = CurrentPath.HasInternalPath() ? CurrentPath.GetInternalPathString() : "/Game";

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = PathStr;
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
	SaveAssetDialogConfig.DefaultAssetName = AssetName;

	const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (SaveObjectPath.IsEmpty())
	{
		UE_LOG(LogDynamicMaterialEditor, Warning, TEXT("No path was chosen for saving the new editable asset, cancelling."));
		return FReply::Handled();
	}

	// Create new model
	UDynamicMaterialModel* NewModel = CurrentModelDynamic->ToEditable(GetTransientPackage());

	if (!NewModel)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to convert dynamic asset to editable."));
		return FReply::Handled();
	}

	// Create a package for it
	PackageName = FPaths::GetBaseFilename(*SaveObjectPath, false);
	UPackage* Package = CreatePackage(*PackageName);

	if (!Package)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to create new package for editable asset."));
		return FReply::Handled();
	}

	AssetName = FPaths::GetBaseFilename(*SaveObjectPath, true);

	// Do we need an instance to? Or just a model?
	UDynamicMaterialInstance* NewInstance = nullptr;

	if (OldInstance)
	{
		NewInstance = Cast<UDynamicMaterialInstance>(GetMutableDefault<UDynamicMaterialInstanceFactory>()->FactoryCreateNew(
			UDynamicMaterialInstance::StaticClass(),
			Package,
			*AssetName,
			RF_Transactional | RF_Public | RF_Standalone,
			NewModel,
			nullptr
		));
	}
	else
	{
		NewModel->Rename(*AssetName, Package, UE::DynamicMaterial::RenameFlags);
	}

	FAssetRegistryModule::AssetCreated(NewModel);

	// If it was in an actor, set it on the actor
	if (NewInstance)
	{
		if (CurrentActorProperty.IsValid())
		{
			if (DMSubsystem && DMSubsystem->GetMaterialValueSetterDelegate().IsBound())
			{
				if (DMSubsystem->ExecuteIsValidDelegate(CurrentModelDynamic))
				{
					DMSubsystem->ExecuteMaterialValueSetterDelegate(*CurrentActorProperty, NewInstance);
				}
				else
				{
					UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Asset is not valid for current world, not assigning to actor."));
				}
			}
			else
			{
				ActorMaterialProperties[SelectedMaterialSlotIndex]->SetMaterial(NewInstance);
			}

			if (TSharedPtr<SDMEditor> Editor = EditorWeak.Pin())
			{
				Editor->SetMaterialObjectProperty(*CurrentActorProperty);
			}
		}
		else
		{
			if (TSharedPtr<SDMEditor> Editor = EditorWeak.Pin())
			{
				Editor->SetMaterialModelBase(NewModel);
			}
		}
	}
	else
	{
		if (TSharedPtr<SDMEditor> Editor = EditorWeak.Pin())
		{
			Editor->SetMaterialModelBase(NewModel);
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
