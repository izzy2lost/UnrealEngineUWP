// Copyright Epic Games, Inc. All Rights Reserved.

#include "STG_Palette.h"
#include "STG_GraphActionMenu.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SOverlay.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "EdGraph/TG_EdGraphSchema.h"

#include "EditorWidgetsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "TG_Editor.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "TG_Style.h"

#define LOCTEXT_NAMESPACE "TGPalette"

void STG_PaletteItem::Construct(const FArguments& InArgs, FCreateWidgetForActionData* const InCreateData)
{
	check(InCreateData->Action.IsValid());

	TSharedPtr<FEdGraphSchemaAction> GraphAction = InCreateData->Action;
	ActionPtr = InCreateData->Action;

	const FSlateBrush* IconBrush = GetIconBrush();
	FSlateColor IconColor = FSlateColor::UseForeground();
	FText IconToolTip = GraphAction->GetTooltipDescription();
	bool bIsReadOnly = false;

	TSharedRef<SWidget> IconWidget = CreateIconWidget( IconToolTip, IconBrush, IconColor );
	TSharedRef<SWidget> NameSlotWidget = CreateTextSlotWidget(InCreateData, bIsReadOnly );
	InlineRenameWidget->SetOverflowPolicy(ETextOverflowPolicy::Ellipsis);

	auto NameAreaBackground = FAppStyle::Get().GetBrush(FName("ContentBrowser.AssetTileItem.NameAreaBackground"));
	const int IconWidth = 70;
	const int IconHeight = 80;
	// Create the actual widget
	this->ChildSlot
	.Padding(FMargin(0.0f, 0.0f, 4.0f, 4.0f))
	[
		// Drop shadow border
		SNew(SBorder)
		.Padding(FMargin(0.0f, 0.0f, 5.0f, 5.0f))
		.BorderImage( FAppStyle::Get().GetBrush(FName("ContentBrowser.AssetTileItem.DropShadow")))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Padding(1)
				.BorderImage(this,&STG_PaletteItem::GetBorderImage)
				[
					SNew(SVerticalBox)
					// Thumbnail
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
					// The remainder of the space is reserved for the name.
						SNew(SBox)
						.Padding(0)
						.WidthOverride(IconWidth)
						.HeightOverride(IconHeight)
						[
							//ItemContents
							IconWidget
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FSlateColor::UseForeground())
						.Padding(FMargin(0,2,0,0))
					]

					+ SVerticalBox::Slot()
					[
						SNew(SBorder)
						.Padding(FMargin(2.0f, 3.0f))
						.BorderImage(NameAreaBackground)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.Padding(2.0f, 2.0f, 0.0f, 0.0f)
							.VAlign(VAlign_Top)
							.HAlign( HAlign_Left)
							[
								SNew(SBox)
								.MaxDesiredHeight(14)
								[
									NameSlotWidget
								]
							]
						]
					]
				]
			]
		]
	];
}

const FSlateBrush* STG_PaletteItem::GetIconBrush()
{
	FString ActionName = ActionPtr.Pin()->GetMenuDescription().ToString();
	FString BrushName = "TG_Editor.Palette." + ActionName;
	if (FTG_Style::Get().HasKey(FName(BrushName)))
	{
		const FSlateBrush* IconBrush = FTG_Style::Get().GetBrush(FName(BrushName));
		return IconBrush;

	}
	return FTG_Style::Get().GetBrush(FName("TG_Editor.Palette.Default"));
}

const FSlateBrush* STG_PaletteItem::GetBorderImage() const
{
	const bool bIsSelected = false;
	const bool bIsHoveredOrDraggedOver = IsHovered();
	if (bIsSelected && bIsHoveredOrDraggedOver)
	{
		static const FName SelectedHover("ContentBrowser.AssetTileItem.SelectedHoverBorder");
		return FAppStyle::Get().GetBrush(SelectedHover);
	}
	else if (bIsSelected)
	{
		static const FName Selected("ContentBrowser.AssetTileItem.SelectedBorder");
		return FAppStyle::Get().GetBrush(Selected);
	}
	else if (bIsHoveredOrDraggedOver)
	{
		static const FName Hovered("ContentBrowser.AssetTileItem.HoverBorder");
		return FAppStyle::Get().GetBrush(Hovered);
	}

	return FAppStyle::Get().GetBrush("ContentBrowser.AssetTileItem.ThumbnailAreaBackground");//FStyleDefaults::GetNoBrush();
}

TSharedRef<SWidget> STG_PaletteItem::CreateHotkeyDisplayWidget(const TSharedPtr<const FInputChord> HotkeyChord)
{
	FText HotkeyText;
	if (HotkeyChord.IsValid())
	{
		HotkeyText = HotkeyChord->GetInputText();
	}
	return SNew(STextBlock)
		.Text(HotkeyText);
}

FText STG_PaletteItem::GetItemTooltip() const
{
	return ActionPtr.Pin()->GetTooltipDescription();
}

//////////////////////////////////////////////////////////////////////////

void STG_Palette::Construct(const FArguments& InArgs, TWeakPtr<FTG_Editor> InTGEditorPtr)
{
	TGEditorPtr = InTGEditorPtr;

	// Create the asset discovery indicator
	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::LoadModuleChecked<FEditorWidgetsModule>("EditorWidgets");
	TSharedRef<SWidget> AssetDiscoveryIndicator = EditorWidgetsModule.CreateAssetDiscoveryIndicator(EAssetDiscoveryIndicatorScaleMode::Scale_Vertical);

	CategoryNames.Add(MakeShareable(new FString(TEXT("All"))));
	CategoryNames.Add(MakeShareable(new FString(TEXT("Expressions"))));
	CategoryNames.Add(MakeShareable(new FString(TEXT("Functions"))));

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(2.0f)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			//// Filter UI
			//+SVerticalBox::Slot()
			//.AutoHeight()
			//[
			//	SNew(SHorizontalBox)

			//	// Comment
			//	+SHorizontalBox::Slot()
			//	.VAlign(VAlign_Center)
			//	.AutoWidth()
			//	[
			//		SNew(STextBlock)
			//		.Text(LOCTEXT("Category", "Category: "))
			//	]

			//	// Combo button to select a class
			//	+SHorizontalBox::Slot()
			//		.VAlign(VAlign_Center)
			//		[
			//			SAssignNew(CategoryComboBox, STextComboBox)
			//			.OptionsSource(&CategoryNames)
			//			.OnSelectionChanged(this, &STG_Palette::CategorySelectionChanged)
			//			.InitiallySelectedItem(CategoryNames[0])
			//		]
			//]

			// Content list
			+SVerticalBox::Slot()
				[
					SNew(SOverlay)

					+SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						// Old Expression and Function lists were auto expanded so do the same here for now
						SAssignNew(TGGraphActionMenu, STG_GraphActionMenu)
						.OnActionDragged(this, &STG_Palette::OnActionDragged)
						.OnCreateWidgetForAction(this, &STG_Palette::OnCreateWidgetForAction)
						.OnCollectAllActions(this, &STG_Palette::CollectAllActions)
						.AutoExpandActionMenu(true)
					]

					//+SOverlay::Slot()
					//	.HAlign(HAlign_Fill)
					//	.VAlign(VAlign_Bottom)
					//	.Padding(FMargin(24, 0, 24, 0))
					//	[
					//		// Asset discovery indicator
					//		AssetDiscoveryIndicator
					//	]
				]

		]
	];

	// Register with the Asset Registry to be informed when it is done loading up files.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetAdded().AddSP(this, &STG_Palette::AddAssetFromAssetRegistry);
	AssetRegistryModule.Get().OnAssetRemoved().AddSP(this, &STG_Palette::RemoveAssetFromRegistry);
	AssetRegistryModule.Get().OnAssetRenamed().AddSP(this, &STG_Palette::RenameAssetFromRegistry);
}

TSharedRef<SWidget> STG_Palette::OnCreateWidgetForAction(FCreateWidgetForActionData* const InCreateData)
{
	return	SNew(STG_PaletteItem, InCreateData);
}

void STG_Palette::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	const UTG_EdGraphSchema* Schema = GetDefault<UTG_EdGraphSchema>();

	FGraphActionMenuBuilder ActionMenuBuilder;

	// Determine all possible actions
	Schema->GetPaletteActions(ActionMenuBuilder, GetFilterCategoryName());

	//@TODO: Avoid this copy
	OutAllActions.Append(ActionMenuBuilder);
}

FString STG_Palette::GetFilterCategoryName() const
{
	if (CategoryComboBox.IsValid())
	{
		return *CategoryComboBox->GetSelectedItem();
	}
	else
	{
		return TEXT("All");
	}
}

void STG_Palette::CategorySelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	RefreshActionsList(true);
}

void STG_Palette::AddAssetFromAssetRegistry(const FAssetData& InAddedAssetData)
{
	RefreshAssetInRegistry(InAddedAssetData);
}

void STG_Palette::RemoveAssetFromRegistry(const FAssetData& InAddedAssetData)
{
	RefreshAssetInRegistry(InAddedAssetData);
}

void STG_Palette::RenameAssetFromRegistry(const FAssetData& InAddedAssetData, const FString& InNewName)
{
	RefreshAssetInRegistry(InAddedAssetData);
}

void STG_Palette::RefreshAssetInRegistry(const FAssetData& InAddedAssetData)
{
	
}

void STG_Palette::RefreshActionsList(bool bPreserveExpansion)
{
	TGGraphActionMenu->RefreshAllActions(bPreserveExpansion);
}

#undef LOCTEXT_NAMESPACE
