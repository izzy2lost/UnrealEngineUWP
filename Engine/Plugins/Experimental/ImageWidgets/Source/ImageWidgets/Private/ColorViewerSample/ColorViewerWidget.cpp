// Copyright Epic Games, Inc. All Rights Reserved.

#if IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE

#include "ColorViewerWidget.h"

#include "ColorViewerCommands.h"
#include "ColorViewerStyle.h"
#include "Algo/AllOf.h"
#include "Algo/NoneOf.h"
#include "Algo/Sort.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "ColorViewerWidget"

namespace UE::ImageWidgets::Sample
{
	void SColorViewerWidget::Construct(const FArguments&)
	{
		ColorViewer = MakeShared<FColorViewer>();

		BindCommands();

		// Create toolbar extensions for a button to randomize the displayed color as well as the tone mapping controls.
		const TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();
		ToolbarExtender->AddToolBarExtension("ToolbarCenter", EExtensionHook::Before, CommandList,
		                                     FToolBarExtensionDelegate::CreateSP(this, &SColorViewerWidget::AddColorButtons));
		ToolbarExtender->AddToolBarExtension("ToolbarRight", EExtensionHook::After, CommandList,
		                                     FToolBarExtensionDelegate::CreateSP(this, &SColorViewerWidget::AddToneMappingButtons));

		// Fill the widget with the image viewport.
		ChildSlot
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(Splitter, SSplitter)
					.PhysicalSplitterHandleSize(2.0f)
					+ SSplitter::Slot()
						.Value(0.0f)
						[
							SAssignNew(Catalog, ImageWidgets::SImageCatalog)
								.PinnedItemsHeading(LOCTEXT("PinnedColors", "Pinned Colors"))
								.ItemsHeading(LOCTEXT("Colors", "Colors"))
								.OnItemSelected_Lambda([&ColorViewer = ColorViewer, &Viewport = Viewport](const FGuid& ImageGuid)
								{
									if (ColorViewer)
									{
										ColorViewer->OnImageSelected(ImageGuid);
									}
									if (Viewport)
									{
										Viewport->RequestRedraw();
									}
								})
								.OnGetContextMenu(this, &SColorViewerWidget::GetContextMenu)
						]
					+ SSplitter::Slot()
						.Value(1.0f)
						[
							SAssignNew(Viewport, ImageWidgets::SImageViewport, ColorViewer.ToSharedRef())
								.ToolbarExtender(ToolbarExtender)
								.DrawSettings(SImageViewport::FDrawSettings{
									.ClearColor = FLinearColor::Black,
									.bBorderEnabled = true,
									.BorderThickness = 1.0,
									.BorderColor = FVector3f(0.2f),
									.bBackgroundColorEnabled = false,
									.bBackgroundCheckerEnabled = false
								})
								.bABComparisonEnabled(true)
						]
			];
	}

	FReply SColorViewerWidget::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
	{
		// Capture all key binds that are handled by the widget's commands. 
		if (CommandList->ProcessCommandBindings(KeyEvent))
		{
			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	void SColorViewerWidget::AddColorButtons(FToolBarBuilder& ToolbarBuilder) const
	{
		const FSlateIcon AddColorIcon(FAppStyle::GetAppStyleSetName(), "FontEditor.Button_Add");
		ToolbarBuilder.AddToolBarButton(FColorViewerCommands::Get().AddColor, NAME_None, TAttribute<FText>(), TAttribute<FText>(),
										TAttribute<FSlateIcon>(AddColorIcon));

		const FSlateIcon RandomizeColorIcon(FAppStyle::GetAppStyleSetName(), "FontEditor.Update");
		ToolbarBuilder.AddToolBarButton(FColorViewerCommands::Get().RandomizeColor, NAME_None, TAttribute<FText>(), TAttribute<FText>(),
		                                TAttribute<FSlateIcon>(RandomizeColorIcon));
	}

	void SColorViewerWidget::AddToneMappingButtons(FToolBarBuilder& ToolbarBuilder) const
	{
		ToolbarBuilder.AddSeparator();

		ToolbarBuilder.BeginBlockGroup();
		{
			const FName& StyleSetName = FColorViewerStyle::Get().GetStyleSetName();
			const FColorViewerCommands& Commands = FColorViewerCommands::Get();

			const FSlateIcon RGBIcon(StyleSetName, "ToneMappingRGB");
			const FSlateIcon LumIcon(StyleSetName, "ToneMappingLum");

			ToolbarBuilder.AddToolBarButton(Commands.ToneMappingRGB, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(RGBIcon));
			ToolbarBuilder.AddToolBarButton(Commands.ToneMappingLum, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(LumIcon));
		}
		ToolbarBuilder.EndBlockGroup();
	}

	TTuple<FText, FText, FText> GetColorItemMetaData(const FColorViewer& ColorViewer, const FColorViewer::FColorItem* ColorItem)
	{
		const FText Name = ColorViewer.GetImageName(ColorItem->Guid);

		const FText Info = FText::Format(
			LOCTEXT("ColorEntryInfoLabel", "{0}"), FText::AsTime(ColorItem->DateTime, EDateTimeStyle::Short, FText::GetInvariantTimeZone()));

		const FText ToolTip = FText::Format(
			LOCTEXT("ColorEntryToolTip", "R {0}, G {1}, B {2}"), {ColorItem->Color.R, ColorItem->Color.G, ColorItem->Color.B});

		return {Name, Info, ToolTip};
	}

	void SColorViewerWidget::AddColor()
	{
		if (const FColorViewer::FColorItem* ColorItem = ColorViewer->AddColor())
		{
			auto [Name, Info, ToolTip] = GetColorItemMetaData(*ColorViewer, ColorItem);
			const FLinearColor ToneMappedColor = ColorViewer->GetDefaultToneMappedColor(ColorItem->Color);

			Catalog->AddItem(MakeShared<FImageCatalogItemData>(ColorItem->Guid, FSlateColorBrush(ToneMappedColor), Name, Info, ToolTip));
			Catalog->SelectItem(ColorItem->Guid);

			if (bCatalogCollapsedOnInit && Catalog->NumTotalItems() > 1 && Splitter->SlotAt(0).GetSizeValue() <= 0.0f)
			{
				Splitter->SlotAt(0).SetSizeValue(0.2f);
				bCatalogCollapsedOnInit = false;
			}
		}
	}

	void SColorViewerWidget::RandomizeColor()
	{
		if (const FColorViewer::FColorItem* ColorItem = ColorViewer->RandomizeColor())
		{
			const auto [Name, Info, ToolTip] = GetColorItemMetaData(*ColorViewer, ColorItem);
			const FLinearColor ToneMappedColor = ColorViewer->GetDefaultToneMappedColor(ColorItem->Color);

			Catalog->UpdateItem({ColorItem->Guid, FSlateColorBrush(ToneMappedColor), Name, Info, ToolTip});
		}
	}

	TSharedPtr<SWidget> SColorViewerWidget::GetContextMenu(const TArray<FGuid>& Guids) const
	{
		checkSlow(!Guids.IsEmpty());
		checkSlow(Algo::AllOf(Guids, [&Catalog = Catalog](const FGuid& Guid) { return Catalog->ItemIsPinned(Guid); })
			|| Algo::NoneOf(Guids, [&Catalog = Catalog] (const FGuid& Guid) { return Catalog->ItemIsPinned(Guid); }));

		FMenuBuilder MenuBuilder(true, nullptr);

		if (Catalog->ItemIsPinned(Guids[0]))
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("UnpinItems", "Unpin"),
				LOCTEXT("UnpinItemsTooltip", "Removes the selected item(s) from the list of pinned items."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Unpinned"),
				FUIAction(FExecuteAction::CreateLambda(
					[Guids, Catalog = Catalog]
					{
						if (Catalog)
						{
							for (const FGuid& Guid : Guids)
							{
								Catalog->UnpinItem(Guid);
							}
							Catalog->SelectItem(Guids[0]);
						}
					})));
		}
		else
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("PinItems", "Pin"),
				LOCTEXT("PinItemsTooltip", "Adds the selected item(s) to the list of pinned items."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Pinned"),
				FUIAction(
					FExecuteAction::CreateLambda(
						[Guids, Catalog = Catalog]
						{
							if (Catalog)
							{
								for (const FGuid& Guid : Guids)
								{
									Catalog->PinItem(Guid);
								}
								Catalog->SelectItem(Guids[0]);
							}
						})));
		}

#if 0
		MenuBuilder.AddMenuEntry(
			LOCTEXT("RenameItem", "Rename"),
			LOCTEXT("RenameItemTooltip", "Renames a single selected item."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Rename"),
			FUIAction(
				FExecuteAction::CreateLambda([]{}),
				FCanExecuteAction::CreateLambda([bSingleItem = Guids.Num() == 1] { return bSingleItem; })));
#endif

		MenuBuilder.AddSeparator();

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteItems", "Delete"), LOCTEXT("DeleteItemsTooltip", "Deletes the selected item(s) from the catalog."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
			FUIAction(FExecuteAction::CreateLambda(
				[Guids, Catalog = Catalog, ColorViewer = ColorViewer]
				{
					if (Catalog && ColorViewer)
					{
						for (const FGuid& Guid : Guids)
						{
							Catalog->RemoveItem(Guid);
							ColorViewer->RemoveColor(Guid);
						}
					}
				})));

		return MenuBuilder.MakeWidget();
	}

	void SColorViewerWidget::BindCommands()
	{
		const FColorViewerCommands& Commands = FColorViewerCommands::Get();

		CommandList = MakeShared<FUICommandList>();

		CommandList->MapAction(
			Commands.AddColor,
			FExecuteAction::CreateSP(this, &SColorViewerWidget::AddColor)
		);

		CommandList->MapAction(
			Commands.RandomizeColor,
			FExecuteAction::CreateSP(this, &SColorViewerWidget::RandomizeColor)
		);

		CommandList->MapAction(
			Commands.ToneMappingRGB,
			FExecuteAction::CreateSP(ColorViewer.ToSharedRef(), &FColorViewer::SetToneMapping, FToneMapping::EMode::RGB),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([ColorViewer = ColorViewer]
			{
				return ColorViewer->GetToneMapping() == FToneMapping::EMode::RGB;
			})
		);

		CommandList->MapAction(
			Commands.ToneMappingLum,
			FExecuteAction::CreateSP(ColorViewer.ToSharedRef(), &FColorViewer::SetToneMapping, FToneMapping::EMode::Lum),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([ColorViewer = ColorViewer]
			{
				return ColorViewer->GetToneMapping() == FToneMapping::EMode::Lum;
			})
		);
	}
}

#undef LOCTEXT_NAMESPACE

#endif // IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE
