// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMediaEditorStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/StarshipCoreStyle.h"
#include "Styling/ToolBarStyle.h"

TSharedPtr<FSlateStyleSet> FAvaMediaEditorStyle::StyleInstance = nullptr;

void FAvaMediaEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FAvaMediaEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FAvaMediaEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AvalancheMediaEditor"));
	return StyleSetName;
}

const FLinearColor& FAvaMediaEditorStyle::GetColor(FName PropertyName, const ANSICHAR* Specifier)
{
	return StyleInstance->GetColor(PropertyName, Specifier);
}

const FSlateBrush* FAvaMediaEditorStyle::GetBrush(FName PropertyName, const ANSICHAR* Specifier)
{
	return StyleInstance->GetBrush(PropertyName, Specifier);
}

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define CORE_FONT(...) FSlateFontInfo(FCoreStyle::GetDefaultFont(), __VA_ARGS__)

const FVector2D Icon12x12(12.0f, 12.0f);
const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon24x24(24.0f, 24.0f);
const FVector2D Icon40x40(40.0f, 40.0f);
const FVector2D Icon64x64(64.0f, 64.0f);

TSharedRef<FSlateStyleSet> FAvaMediaEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>("AvalancheMediaEditor");

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Avalanche"));
	check(Plugin.IsValid());
	if (Plugin.IsValid())
	{
		Style->SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")));
	}

	// Asset Class Icons
	Style->Set("ClassIcon.AvalanchePlayback", new IMAGE_BRUSH("Icons/MediaIcons/AvaPlaybackIcon_16x"     , Icon16x16));
	Style->Set("ClassThumbnail.AvalanchePlayback", new IMAGE_BRUSH("Icons/MediaIcons/AvaPlaybackIcon_64x", Icon64x64));
	Style->Set("ClassIcon.AvalanchePlaylist", new IMAGE_BRUSH("Icons/MediaIcons/AvaRundownIcon_16x"     , Icon16x16));
	Style->Set("ClassThumbnail.AvalanchePlaylist", new IMAGE_BRUSH("Icons/MediaIcons/AvaRundownIcon_64x", Icon64x64));
	Style->Set("ClassIcon.AvaRundownMacroCollection", new IMAGE_BRUSH("Icons/MediaIcons/AvaRundownMacroCollectionIcon_16x"     , Icon16x16));
	Style->Set("ClassThumbnail.AvaRundownMacroCollection", new IMAGE_BRUSH("Icons/MediaIcons/AvaRundownMacroCollectionIcon_64x", Icon64x64));

	//Playlist Commands
	Style->Set("AvaPlaylistCommands.AddPage"         , new IMAGE_BRUSH_SVG("Icons/MediaIcons/AddPage"      , Icon16x16));
	Style->Set("AvaPlaylistCommands.AddTemplate"     , new IMAGE_BRUSH_SVG("Icons/MediaIcons/AddPage"      , Icon16x16));
	Style->Set("AvaPlaylistCommands.CreatePageInstanceFromTemplate"  , new IMAGE_BRUSH_SVG("Icons/MediaIcons/AddPage"      , Icon16x16));
	Style->Set("AvaPlaylistCommands.CreateComboTemplate", new IMAGE_BRUSH_SVG("Icons/MediaIcons/AddPage"   , Icon16x16));
	Style->Set("AvaPlaylistCommands.RemovePage"      , new IMAGE_BRUSH_SVG("Icons/MediaIcons/RemovePage"   , Icon16x16));
	Style->Set("AvaPlaylistCommands.RenumberPage"    , new IMAGE_BRUSH_SVG("Icons/MediaIcons/RenumberPage" , Icon16x16));
	Style->Set("AvaPlaylistCommands.ReimportPage"    , new IMAGE_BRUSH_SVG("Icons/MediaIcons/ReimportPage" , Icon16x16));
	Style->Set("AvaPlaylistCommands.EditPageSource"  , new IMAGE_BRUSH_SVG("Icons/MediaIcons/OpenAsset"	  , Icon16x16));
	Style->Set("AvaPlaylistCommands.Play"            , new IMAGE_BRUSH_SVG("Icons/MediaIcons/Play"         , Icon16x16));
	Style->Set("AvaPlaylistCommands.UpdateValues"    , new IMAGE_BRUSH_SVG("Icons/MediaIcons/UpdateValues" , Icon16x16));
	Style->Set("AvaPlaylistCommands.Stop"            , new IMAGE_BRUSH_SVG("Icons/MediaIcons/Stop"         , Icon16x16));
	Style->Set("AvaPlaylistCommands.ForceStop"       , new IMAGE_BRUSH_SVG("Icons/MediaIcons/Stop"         , Icon16x16));
	Style->Set("AvaPlaylistCommands.Continue"        , new IMAGE_BRUSH_SVG("Icons/MediaIcons/Continue"     , Icon16x16));
	Style->Set("AvaPlaylistCommands.PlayNext"        , new IMAGE_BRUSH_SVG("Icons/MediaIcons/PlayNext"     , Icon16x16));
	Style->Set("AvaPlaylistCommands.PreviewFrame"    , new IMAGE_BRUSH_SVG("Icons/MediaIcons/PreviewFrame" , Icon16x16));
	Style->Set("AvaPlaylistCommands.PreviewPlay"     , new IMAGE_BRUSH_SVG("Icons/MediaIcons/Play"         , Icon16x16));
	Style->Set("AvaPlaylistCommands.PreviewStop"     , new IMAGE_BRUSH_SVG("Icons/MediaIcons/Stop"         , Icon16x16));
	Style->Set("AvaPlaylistCommands.PreviewForceStop", new IMAGE_BRUSH_SVG("Icons/MediaIcons/Stop"         , Icon16x16));
	Style->Set("AvaPlaylistCommands.PreviewContinue" , new IMAGE_BRUSH_SVG("Icons/MediaIcons/Continue"     , Icon16x16));
	Style->Set("AvaPlaylistCommands.PreviewPlayNext" , new IMAGE_BRUSH_SVG("Icons/MediaIcons/PlayNext"     , Icon16x16));
	Style->Set("AvaPlaylistCommands.TakeToProgram"   , new IMAGE_BRUSH_SVG("Icons/MediaIcons/Play"		   , Icon16x16));

	//Broadcast
	Style->Set("AvalancheMediaEditor.OutputIcon"           , new IMAGE_BRUSH("Icons/MediaIcons/MediaOutput"     , Icon20x20));
	Style->Set("AvalancheMediaEditor.BroadcastIcon"        , new IMAGE_BRUSH("Icons/MediaIcons/MediaOutput"     , Icon20x20));
	Style->Set("AvalancheMediaEditor.BroadcastClient"      , new IMAGE_BRUSH("Icons/MediaIcons/BroadcastClient" , Icon64x64));
	Style->Set("AvalancheMediaEditor.BroadcastServer"      , new IMAGE_BRUSH("Icons/MediaIcons/BroadcastServer" , Icon64x64));
	Style->Set("AvalancheMediaEditor.BroadcastClient.Small", new IMAGE_BRUSH("Icons/MediaIcons/BroadcastClient" , Icon20x20));
	Style->Set("AvalancheMediaEditor.BroadcastServer.Small", new IMAGE_BRUSH("Icons/MediaIcons/BroadcastServer" , Icon20x20));
	Style->Set("AvalancheMediaEditor.BroadcastOffline"     , new IMAGE_BRUSH_SVG("Icons/MediaIcons/BroadcastOffline"	, Icon16x16));
	Style->Set("AvalancheMediaEditor.BroadcastError"       , new IMAGE_BRUSH_SVG("Icons/MediaIcons/BroadcastError"		, Icon16x16));
	Style->Set("AvalancheMediaEditor.BroadcastWarning"     , new IMAGE_BRUSH_SVG("Icons/MediaIcons/BroadcastWarning"	, Icon16x16));
	Style->Set("AvalancheMediaEditor.BroadcastLive"        , new IMAGE_BRUSH_SVG("Icons/MediaIcons/BroadcastLive"		, Icon16x16)); 
	Style->Set("AvalancheMediaEditor.BroadcastIdle"        , new IMAGE_BRUSH_SVG("Icons/MediaIcons/BroadcastIdle"		, Icon16x16));

	// Broadcast Channel Types
	Style->Set("AvalancheMediaEditor.ChannelTypeProgram"	, new IMAGE_BRUSH_SVG("Icons/MediaIcons/ChannelTypeProgram"	, Icon16x16)); 
	Style->Set("AvalancheMediaEditor.ChannelTypePreview"	, new IMAGE_BRUSH_SVG("Icons/MediaIcons/ChannelTypePreview"	, Icon16x16));
	
	// Media Status
	/* For when we have SVGs
	Style->Set("AvalancheMediaEditor.MediaAssetStatus", new IMAGE_BRUSH_SVG("Icons/MediaIcons/MediaLoaded", Icon16x16));
	Style->Set("AvalancheMediaEditor.MediaSyncStatus",  new IMAGE_BRUSH_SVG("Icons/MediaIcons/MediaLoaded", Icon16x16));
	Style->Set("AvalancheMediaEditor.MediaPreviewing",  new IMAGE_BRUSH_SVG("Icons/MediaIcons/MediaLoaded", Icon16x16));
	Style->Set("AvalancheMediaEditor.MediaPlaying",     new IMAGE_BRUSH_SVG("Icons/MediaIcons/MediaLoaded", Icon16x16));
	*/
	Style->Set("AvalancheMediaEditor.MediaAssetStatus", new IMAGE_BRUSH("Icons/MediaIcons/MediaStatus",     Icon16x16));
	Style->Set("AvalancheMediaEditor.MediaSyncStatus",  new IMAGE_BRUSH("Icons/MediaIcons/MediaSyncStatus", Icon16x16));
	Style->Set("AvalancheMediaEditor.MediaPreviewing",  new IMAGE_BRUSH("Icons/MediaIcons/MediaPreviewing", Icon16x16));
	Style->Set("AvalancheMediaEditor.MediaPlaying",     new IMAGE_BRUSH("Icons/MediaIcons/MediaPlaying",    Icon16x16));

	// Media Output Status
	Style->Set("AvalancheMediaEditor.MediaOutputOffline"   , new IMAGE_BRUSH_SVG("Icons/MediaIcons/MediaOutputOffline"	, Icon16x16));
	Style->Set("AvalancheMediaEditor.MediaOutputIdle"      , new IMAGE_BRUSH_SVG("Icons/MediaIcons/MediaOutputIdle"		, Icon16x16));
	Style->Set("AvalancheMediaEditor.MediaOutputPreparing" , new IMAGE_BRUSH_SVG("Icons/MediaIcons/MediaOutputPreparing", Icon16x16));
	Style->Set("AvalancheMediaEditor.MediaOutputLive"      , new IMAGE_BRUSH_SVG("Icons/MediaIcons/MediaOutputLive"		, Icon16x16));
	Style->Set("AvalancheMediaEditor.MediaOutputLiveWarn"  , new IMAGE_BRUSH_SVG("Icons/MediaIcons/MediaOutputLiveWarn"	, Icon16x16));
	Style->Set("AvalancheMediaEditor.MediaOutputError"     , new IMAGE_BRUSH_SVG("Icons/MediaIcons/MediaOutputError"	, Icon16x16));

	// Avalanche Preview
	Style->Set("AvalancheMediaEditor.Checkerboard" , new IMAGE_BRUSH("Images/AvaPreviewCheckerboard", Icon16x16, FLinearColor::White, ESlateBrushTileType::Both));

	// Button styles
	const FButtonStyle& AppStyle_SimpleButton = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton");

	Style->Set("AvalancheMediaEditor.BorderlessButton", FButtonStyle(AppStyle_SimpleButton));

	const FButtonStyle& MenuButtonStyleGreen = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("FlatButton.Success");
	Style->Set("AvalancheMediaEditor.ButtonGreen", MenuButtonStyleGreen);
	const FButtonStyle& MenuButtonStyleRed = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("FlatButton.Danger");
	Style->Set("AvalancheMediaEditor.ButtonRed", MenuButtonStyleRed);

	const FToolBarStyle& SlimToolbarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("SlimToolBar");
	Style->Set("AvalancheMediaEditor.ToolBar", SlimToolbarStyle);
	const FButtonStyle& MenuButtonStyle = SlimToolbarStyle.ButtonStyle;

	FToolBarStyle SlimToolbarStyleOverrideRedButton = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("SlimToolBar");
	SlimToolbarStyleOverrideRedButton.SetButtonStyle(Style->GetWidgetStyle<FButtonStyle>("AvalancheMediaEditor.ButtonRed"));
	SlimToolbarStyleOverrideRedButton.ButtonStyle.SetDisabled(MenuButtonStyle.Disabled);
	Style->Set("AvalancheMediaEditor.ToolBarRedButtonOverride", SlimToolbarStyleOverrideRedButton);

	FToolBarStyle SlimToolbarStyleOverrideGreenButton = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("SlimToolBar");
	SlimToolbarStyleOverrideGreenButton.SetButtonStyle(Style->GetWidgetStyle<FButtonStyle>("AvalancheMediaEditor.ButtonGreen"));
	SlimToolbarStyleOverrideGreenButton.ButtonStyle.SetDisabled(MenuButtonStyle.Disabled);
	Style->Set("AvalancheMediaEditor.ToolBarGreenButtonOverride", SlimToolbarStyleOverrideGreenButton);

	const FToolBarStyle& CalloutToolbarStyleOverride = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("CalloutToolbar");
	Style->Set("AvalancheMediaEditor.CalloutToolbar", CalloutToolbarStyleOverride);

	Style->Set("TableView.ActivePageBrush", new FSlateColorBrush(FLinearColor(0.05f, 0.1f, 0.05f, 0.5f)));
	Style->Set("TableView.Hovered.ActivePageBrush", new FSlateColorBrush(FLinearColor(0.05f, 0.2f, 0.05f, 0.5f)));
	Style->Set("TableView.Selected.ActivePageBrush", new FSlateColorBrush(FLinearColor(0.05f, 0.3f, 0.05f, 0.5f)));
	Style->Set("TableView.DisabledPageBrush", new FSlateColorBrush(FLinearColor(0.1f, 0.05f, 0.05f, 0.5f)));
	Style->Set("TableView.Hovered.DisabledPageBrush", new FSlateColorBrush(FLinearColor(0.2f, 0.05f, 0.05f, 0.5f)));
	Style->Set("TableView.Selected.DisabledPageBrush", new FSlateColorBrush(FLinearColor(0.3f, 0.05f, 0.05f, 0.5f)));

	// Asset Colors - in the shades of yellow (hue = 36, saturation = 221)
	Style->Set("AvalancheMediaEditor.AssetColors.Rundown", FLinearColor::MakeFromHSV8(36, 221, 170));
	Style->Set("AvalancheMediaEditor.AssetColors.RundownMacroCollection", FLinearColor::MakeFromHSV8(36, 221, 124));
	Style->Set("AvalancheMediaEditor.AssetColors.Playback", FLinearColor::MakeFromHSV8(36, 221, 105));

	return Style;
}

#undef IMAGE_BRUSH
#undef IMAGE_BRUSH_SVG
#undef BOX_BRUSH
#undef CORE_FONT

const ISlateStyle& FAvaMediaEditorStyle::Get()
{
	return *StyleInstance;
}
