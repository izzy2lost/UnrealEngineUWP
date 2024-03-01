// Copyright Epic Games, Inc. All Rights Reserved.

#if IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE

#include "ColorViewerWidget.h"

#include "ColorViewerCommands.h"
#include "ColorViewerStyle.h"
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
		                                     FToolBarExtensionDelegate::CreateSP(this, &SColorViewerWidget::AddRandomColorButton));
		ToolbarExtender->AddToolBarExtension("ToolbarRight", EExtensionHook::After, CommandList,
		                                     FToolBarExtensionDelegate::CreateSP(this, &SColorViewerWidget::AddToneMappingButtons));

		// Fill the widget with the image viewport.
		ChildSlot
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
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

	void SColorViewerWidget::AddRandomColorButton(FToolBarBuilder& ToolbarBuilder) const
	{
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

	void SColorViewerWidget::BindCommands()
	{
		const FColorViewerCommands& Commands = FColorViewerCommands::Get();

		CommandList = MakeShared<FUICommandList>();

		CommandList->MapAction(
			Commands.RandomizeColor,
			FExecuteAction::CreateSP(ColorViewer.ToSharedRef(), &FColorViewer::RandomizeColor)
		);

		CommandList->MapAction(
			Commands.ToneMappingRGB,
			FExecuteAction::CreateSP(ColorViewer.ToSharedRef(), &FColorViewer::SetToneMapping, FToneMapping::EMode::RGB),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this]
			{
				return ColorViewer->GetToneMapping() == FToneMapping::EMode::RGB;
			})
		);

		CommandList->MapAction(
			Commands.ToneMappingLum,
			FExecuteAction::CreateSP(ColorViewer.ToSharedRef(), &FColorViewer::SetToneMapping, FToneMapping::EMode::Lum),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this]
			{
				return ColorViewer->GetToneMapping() == FToneMapping::EMode::Lum;
			})
		);
	}
}

#undef LOCTEXT_NAMESPACE

#endif // IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE
