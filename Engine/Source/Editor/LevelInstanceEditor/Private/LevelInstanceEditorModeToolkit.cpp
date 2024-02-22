// Copyright Epic Games, Inc. All Rights Reserved.
#include "LevelInstanceEditorModeToolkit.h"
#include "LevelInstanceEditorMode.h"
#include "Internationalization/Internationalization.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Tools/UEdMode.h"
#include "Toolkits/IToolkitHost.h"
#include "Styling/SlateIconFinder.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/ILevelInstanceEditorModule.h"
#include "Engine/World.h"

class FAssetEditorModeUILayer;

#define LOCTEXT_NAMESPACE "LevelInstanceEditorModeToolkit"

FLevelInstanceEditorModeToolkit::FLevelInstanceEditorModeToolkit()
{
}

FLevelInstanceEditorModeToolkit::~FLevelInstanceEditorModeToolkit()
{
	if(IsHosted() && ViewportOverlayWidget.IsValid())
	{
		GetToolkitHost()->RemoveViewportOverlayWidget(ViewportOverlayWidget.ToSharedRef());
	}
}

void FLevelInstanceEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);

	ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(InitToolkitHost->GetWorld());
	check(LevelInstanceSubsystem);

	// ViewportOverlay
	SAssignNew(ViewportOverlayWidget, SHorizontalBox)
	+SHorizontalBox::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(0.0f, 0.0f, 0.f, 15.f))
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
		.Padding(8.f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(SImage)
				.Image(FSlateIconFinder::FindIconBrushForClass(ALevelInstance::StaticClass()))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(STextBlock)
				.Text_Lambda([LevelInstanceSubsystem]()
				{
					return LevelInstanceSubsystem->GetToolKitDisplayText();
				})
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(2.0, 0.f, 0.f, 0.f))
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.TextStyle(FAppStyle::Get(), "DialogButtonText")
				.Text(LOCTEXT("ExitEdit", "Exit"))
				.ToolTipText_Lambda([LevelInstanceSubsystem]()
				{
					return LevelInstanceSubsystem->GetToolKitExitToolTip();
				})
				.HAlign(HAlign_Center)
				.OnClicked_Lambda([LevelInstanceSubsystem]() 
				{ 
					LevelInstanceSubsystem->ToolKitExit();
					return FReply::Handled(); 
				})
			]
		]
	];
	
	GetToolkitHost()->AddViewportOverlayWidget(ViewportOverlayWidget.ToSharedRef());
}

FName FLevelInstanceEditorModeToolkit::GetToolkitFName() const
{
	return FName("LevelInstanceEditorModeToolkit");
}

FText FLevelInstanceEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitDisplayName", "Level Instance Editor Mode");
}

void FLevelInstanceEditorModeToolkit::RequestModeUITabs()
{
	// No Tabs
}

#undef LOCTEXT_NAMESPACE