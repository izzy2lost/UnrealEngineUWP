// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDViewportToolbar.h"

#include "ChaosVDCommands.h"
#include "ChaosVDEditorSettings.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "PropertyEditorModule.h"
#include "DetailsViewArgs.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SChaosVDEditorViewportViewMenu.h"
#include "Widgets/SChaosVDPlaybackViewport.h"
#include "Widgets/SChaosVDVisualizationControls.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void SChaosVDViewportToolbar::Construct(const FArguments& InArgs, TSharedPtr<ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InInfoProvider);
}

TSharedRef<SEditorViewportViewMenu> SChaosVDViewportToolbar::MakeViewMenu()
{
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();
	return SNew(SChaosVDEditorViewportViewMenu, ViewportRef, SharedThis(this));
}

void SChaosVDViewportToolbar::ExtendOptionsMenu(FMenuBuilder& OptionsMenuBuilder) const
{
	constexpr bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder CVDOptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, GetInfoProvider().GetViewportWidget()->GetCommandList());

	CVDOptionsMenuBuilder.BeginSection("CVDViewportViewportOptions", LOCTEXT("OptionsMenuHeader", "Utils"));
	
	CVDOptionsMenuBuilder.AddWidget(GenerateGoToLocationWidget(), LOCTEXT("GoToLocation", "Go to Location"));
	CVDOptionsMenuBuilder.EndSection();

	OptionsMenuBuilder = CVDOptionsMenuBuilder;
}

TSharedRef<SWidget> SChaosVDViewportToolbar::GenerateGoToLocationWidget() const
{
	return SNew(SBox)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				.WidthOverride(100.0f)
				[
					SNew (SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
					.Padding(FMargin(1.0f))
					[
						SNew(SEditableText)
						.ToolTipText(LOCTEXT("GoToLocationTooltip", "Location to teleport to."))
						.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
						.OnTextCommitted(FOnTextCommitted::CreateSP(this, &SChaosVDViewportToolbar::HandleGoToLocationCommited))
					]
				]
			];
}

TSharedRef<SWidget> SChaosVDViewportToolbar::GenerateShowMenu() const
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedRef<IDetailsView> DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsPanel->SetObject(GetMutableDefault<UChaosVDEditorSettings>());

	TSharedRef<SChaosVDPlaybackViewport> ViewportRef = StaticCastSharedRef<SChaosVDPlaybackViewport>(GetInfoProvider().GetViewportWidget());

	return SNew(SChaosVDVisualizationControls, ViewportRef->GetObservedController(), ViewportRef);
}

void SChaosVDViewportToolbar::HandleGoToLocationCommited(const FText& InLocationAsText, ETextCommit::Type Type) const
{
	if (Type != ETextCommit::OnEnter)
	{
		return;
	}

	FVector Location;
	Location.InitFromString(InLocationAsText.ToString());

	TSharedRef<SChaosVDPlaybackViewport> ViewportRef = StaticCastSharedRef<SChaosVDPlaybackViewport>(GetInfoProvider().GetViewportWidget());

	ViewportRef->GoToLocation(Location);
	
}
#undef LOCTEXT_NAMESPACE
