// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaPagePreview.h"
#include "AvaMediaEditorStyle.h"
#include "AvalancheMediaSettings.h"
#include "Brushes/SlateImageBrush.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAvaMediaEditorModule.h"
#include "ISettingsModule.h"
#include "OutputDevices/Slate/SAvaCaptureImage.h"
#include "Playlist/AvaPlaylistCommands.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/AvaRundownEditorSettings.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Playlist/Preview/SAvaPreviewChannelSelector.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SAvaPagePreview"

/**
 * Wrapper to encapsulate a FSlateImageBrush that can be updated with a render target.
 * If the render target becomes null, the brush is updated to be a solid color.
 */
struct SAvaPagePreview::FPreviewBrush
{
	FSlateImageBrush Brush;
	FVector2D RenderTargetSize;

	FPreviewBrush(const FVector2D& InRenderTargetSize)
		: Brush(NAME_None, InRenderTargetSize, FLinearColor::Black)
		, RenderTargetSize(InRenderTargetSize)
	{

	}

	void Update(UTextureRenderTarget2D* InRenderTarget)
	{
		if (InRenderTarget)
		{
			RenderTargetSize = FVector2D(InRenderTarget->SizeX, InRenderTarget->SizeY);

			//If Brush is invalid, or the Brush's Texture Target doesn't match the new Render Target, reset the Brush.
			if (Brush.GetResourceObject() != InRenderTarget)
			{
				Brush = FSlateImageBrush(InRenderTarget, RenderTargetSize);
			}
			//If Sizes mismatch, just resizes rather than recreating the Brush with same underlying Resource
			else if (Brush.GetImageSize() != RenderTargetSize)
			{
				Brush.SetImageSize(RenderTargetSize);
			}
		}
		else if (Brush.GetResourceObject() != nullptr || Brush.GetImageSize() != RenderTargetSize)
		{
			// Preserve the size of the preview brush so the checkerboard remains the same.
			Brush = FSlateImageBrush(NAME_None, RenderTargetSize, FLinearColor::Black);
		}
	}
};

void SAvaPagePreview::Construct(const FArguments& InArgs, const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
{
	PlaylistEditorWeak = InPlaylistEditor;

	const FIntPoint DefaultRenderTargetSize = UAvalancheMediaSettings::Get().PreviewDefaultResolution;
	PreviewBrush = MakeUnique<FPreviewBrush>(FVector2d(DefaultRenderTargetSize.X, DefaultRenderTargetSize.Y));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreatePagePreviewToolBar(InPlaylistEditor->GetToolkitCommands())
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::Both)
			[
				SNew( SOverlay )
				+SOverlay::Slot()
				[
					SNew( SImage )
					.Image( FAvaMediaEditorStyle::Get().GetBrush( "AvalancheMediaEditor.Checkerboard" ) )
					.Visibility(this, &SAvaPagePreview::GetCheckerboardVisibility)
				]
				+SOverlay::Slot()
				[
					SNew(SAvaCaptureImage)
					.ImageArgs(SImage::FArguments()
						.Image(this, &SAvaPagePreview::GetPreviewBrush))
					.ShouldInvertAlpha(true)
					.EnableGammaCorrection(false)
					.EnableBlending(this, &SAvaPagePreview::IsBlendingEnabled)
				]
			]
		]
	];
}

SAvaPagePreview::SAvaPagePreview() = default;
SAvaPagePreview::~SAvaPagePreview() = default;

TSharedRef<SWidget> SAvaPagePreview::CreatePagePreviewToolBar(const TSharedRef<FUICommandList>& InCommandList)
{
	FSlimHorizontalToolBarBuilder ToolBarBuilder(InCommandList, FMultiBoxCustomization::None);
	ToolBarBuilder.SetStyle(&FAvaMediaEditorStyle::Get(), "AvalancheMediaEditor.ToolBar");

	const FAvaPlaylistCommands& PlaylistCommands = FAvaPlaylistCommands::Get();

	ToolBarBuilder.BeginSection(TEXT("ShowControl"));
	{
		ToolBarBuilder.AddToolBarButton(PlaylistCommands.PreviewFrame);

		ToolBarBuilder.BeginStyleOverride("AvalancheMediaEditor.ToolBarGreenButtonOverride");
		{
			ToolBarBuilder.AddToolBarButton(PlaylistCommands.PreviewPlay);
		}
		ToolBarBuilder.EndStyleOverride();

		ToolBarBuilder.AddToolBarButton(PlaylistCommands.PreviewContinue);
		ToolBarBuilder.AddToolBarButton(PlaylistCommands.PreviewStop);
		ToolBarBuilder.AddToolBarButton(PlaylistCommands.PreviewPlayNext);

		ToolBarBuilder.BeginStyleOverride("AvalancheMediaEditor.ToolBarRedButtonOverride");
		{
			ToolBarBuilder.AddToolBarButton(PlaylistCommands.TakeToProgram);
		}
		ToolBarBuilder.EndStyleOverride();

		if (TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
		{
			ToolBarBuilder.AddWidget(PlaylistEditor->MakeReadPageWidget());	
		}
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection("Settings");
	{
		ToolBarBuilder.AddWidget(SNew(SSpacer), NAME_None, false, HAlign_Right);

		ToolBarBuilder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateSP(this, &SAvaPagePreview::HandleCheckerboardActionExecute),
					FCanExecuteAction::CreateLambda([]	{ return true;}))
				, NAME_None
				, FText()
				, LOCTEXT("ToggleAlpha_ToolTip", "Toggle alpha preview (checker board).")
				, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Checkerboard")
			);

		ToolBarBuilder.BeginStyleOverride("AvalancheMediaEditor.CalloutToolbar");
		{
			ToolBarBuilder.AddComboButton(
				FUIAction(),
				FOnGetContent::CreateSP(this, &SAvaPagePreview::OnGenerateSettingsMenu),
				LOCTEXT("SettingsMenu", "Preview Settings"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings")
			);
		}
		ToolBarBuilder.EndStyleOverride();
	}
	ToolBarBuilder.EndSection();	

	return ToolBarBuilder.MakeWidget();
}

TSharedRef<SWidget> SAvaPagePreview::OnGenerateSettingsMenu()
{
	TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();
	TSharedPtr<FUICommandList> ToolkitCommands = PlaylistEditor.IsValid() ? PlaylistEditor->GetToolkitCommands() : TSharedPtr<FUICommandList>();
	
	FMenuBuilder MenuBuilder(true, ToolkitCommands);
	MenuBuilder.BeginSection("ResolutionSection", LOCTEXT("ResolutionSectionHeader", "Resolution Options"));
	{
		// Add a bunch of standard resolutions.
		AddResolutionMenuEntry(MenuBuilder, LOCTEXT("Res_3840x2160", "3840 x 2160"), FIntPoint(3840,2160));
		AddResolutionMenuEntry(MenuBuilder, LOCTEXT("Res_1920x1080", "1920 x 1080"), FIntPoint(1920,1080));
		AddResolutionMenuEntry(MenuBuilder, LOCTEXT("Res_960x540", "960 x 540"), FIntPoint(960,540));
		AddResolutionMenuEntry(MenuBuilder, LOCTEXT("Res_480x270", "480 x 270"), FIntPoint(480,270));
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddWidget(SNew(SAvaPreviewChannelSelector), LOCTEXT("PreviewChannelSelectorLabel", "Channel"));
	
	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Settings", "Settings"),
		LOCTEXT("Settings_Tooltip", "Opens the Avalanche Media Settings."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SAvaPagePreview::HandleSettingsActionExecute)
		),
		NAME_None,
		EUserInterfaceActionType::Button);
	
	return MenuBuilder.MakeWidget();
}

UTextureRenderTarget2D* SAvaPagePreview::GetPreviewRenderTarget() const
{
	const TSharedPtr<FAvaPlaylistEditor> Editor = PlaylistEditorWeak.Pin();
	if (Editor.IsValid() && Editor->IsPlaylistValid())
	{
		return Editor->GetPlaylist()->GetPreviewRenderTarget();
	}
	return nullptr;
}

const FSlateBrush* SAvaPagePreview::GetPreviewBrush() const
{
	// Update the brush here to ensure the cached image attribute is updated with the correct render target.
	PreviewBrush->Update(GetPreviewRenderTarget());
	return &PreviewBrush->Brush;
}

bool SAvaPagePreview::IsBlendingEnabled() const
{
	const UAvaRundownEditorSettings* Settings = UAvaRundownEditorSettings::Get();
	return Settings ? Settings->bPreviewCheckerBoard : false;
}

EVisibility SAvaPagePreview::GetCheckerboardVisibility() const
{	
	if (const UAvaRundownEditorSettings* Settings = UAvaRundownEditorSettings::Get())
	{
		return Settings->bPreviewCheckerBoard ? EVisibility::Visible : EVisibility::Hidden;
	}
	return EVisibility::Hidden;
}

void SAvaPagePreview::SetPreviewResolution(FIntPoint InResolution) const
{
	// Update settings
	UAvalancheMediaSettings& AvalancheMediaSettings = UAvalancheMediaSettings::GetMutable();
	AvalancheMediaSettings.PreviewDefaultResolution = InResolution;
	AvalancheMediaSettings.SaveConfig();

	// Resize render target
	UTextureRenderTarget2D* RenderTarget = GetPreviewRenderTarget();
	if (RenderTarget)
	{
		RenderTarget->ResizeTarget(InResolution.X, InResolution.Y);
	}
	else
	{
		// The brush needs a resize.
		PreviewBrush->RenderTargetSize = FVector2D(InResolution.X, InResolution.Y);
	}
	
	// Refresh the brush.
	PreviewBrush->Update(RenderTarget);
}

bool SAvaPagePreview::IsPreviewResolution(FIntPoint InResolution) const
{
	return UAvalancheMediaSettings::Get().PreviewDefaultResolution == InResolution;
}

void SAvaPagePreview::AddResolutionMenuEntry(FMenuBuilder& InOutMenuBuilder, const FText& Label, const FIntPoint& InResolution)
{
	InOutMenuBuilder.AddMenuEntry(
			Label,
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAvaPagePreview::SetPreviewResolution, InResolution),
				FCanExecuteAction::CreateLambda([]() { return true; }),
				FIsActionChecked::CreateSP(this, &SAvaPagePreview::IsPreviewResolution, InResolution)
			),
			NAME_None, EUserInterfaceActionType::RadioButton);
}

void SAvaPagePreview::HandleCheckerboardActionExecute() const
{
	UAvaRundownEditorSettings* RundownEditorSettings = UAvaRundownEditorSettings::GetMutable();
	if (!RundownEditorSettings)
	{
		UE_LOG(LogAvaMediaEditor, Error, TEXT("Failed to retrieve Rundown Editor Settings."));
		return;
	}
	
	bool& bShowCheckerBoard = RundownEditorSettings->bPreviewCheckerBoard;
	bShowCheckerBoard = !bShowCheckerBoard;
	RundownEditorSettings->SaveConfig();	// We want this to be persistent.
	if (bShowCheckerBoard)
	{
		const IConsoleVariable* PropagateAlphaCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
		if (PropagateAlphaCVar && PropagateAlphaCVar->GetInt() != 2)
		{
			const FText NotificationText = LOCTEXT("AlphaSupport",
				"An output requested Alpha Support but the required project setting is not enabled!\n"
				"Go to Project Settings > Rendering > PostProcessing > 'Enable Alpha Channel Support in Post Processing' and set it to 'Allow through tonemapper'.");

			FNotificationInfo Info(NotificationText);
			Info.ExpireDuration = 5.0f;	// The message is long, need more time to read it.
			FSlateNotificationManager::Get().AddNotification(Info);

			// Also output to logs.
			UE_LOG(LogAvaMediaEditor, Warning, TEXT("%s"), *NotificationText.ToString());
		}
	}
}

void SAvaPagePreview::HandleSettingsActionExecute() const
{
	// Bring up editor of UAvalancheMediaSettings.
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer("Project", "Motion Design", "Playback & Broadcast");
}

#undef LOCTEXT_NAMESPACE
