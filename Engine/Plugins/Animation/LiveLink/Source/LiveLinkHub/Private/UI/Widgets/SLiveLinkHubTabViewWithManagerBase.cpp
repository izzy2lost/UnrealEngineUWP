// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLiveLinkHubTabViewWithManagerBase.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UI/Widgets/SLiveLinkHubTabViewBase.h"

#define LOCTEXT_NAMESPACE "LiveLinkHub.SLiveLinkHubTabViewWithManagerBase"

void SLiveLinkHubTabViewWithManagerBase::Construct(const FArguments& InArgs)
{
	check(InArgs._ConstructUnderWindow && InArgs._ConstructUnderMajorTab);
	SLiveLinkHubTabViewBase::Construct(
		SLiveLinkHubTabViewBase::FArguments()
		.Content()
		[
			InArgs._OverlayTabs.IsBound() ? InArgs._OverlayTabs.Execute(CreateTabs(InArgs)) : CreateTabs(InArgs)
		]
	);
}

TSharedRef<SWidget> SLiveLinkHubTabViewWithManagerBase::CreateTabs(const FArguments& InArgs)
{
	TabManager = FGlobalTabmanager::Get()->NewTabManager(InArgs._ConstructUnderMajorTab.ToSharedRef());
	TabManager->SetMainTab(InArgs._ConstructUnderMajorTab.ToSharedRef());
	
	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(InArgs._LayoutName);
	InArgs._CreateTabs.ExecuteIfBound(TabManager.ToSharedRef(), Layout);
	Layout = FLayoutSaveRestore::LoadFromConfig(TEXT("LiveLinkHubLayout"), Layout);

	TabManager->SetOnPersistLayout(
		FTabManager::FOnPersistLayout::CreateStatic(
			[](const TSharedRef<FTabManager::FLayout>& InLayout)
			{
				if (InLayout->GetPrimaryArea().Pin().IsValid())
				{
					FLayoutSaveRestore::SaveToConfig(TEXT("LiveLinkHubLayout"), InLayout);
				}
			}
		)
	);

	TSharedRef<SWidget> Result = TabManager->RestoreFrom(Layout, InArgs._ConstructUnderWindow).ToSharedRef();
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList>());
	FillInDefaultMenuItems(MenuBarBuilder);
	InArgs._CreateMenuBar.ExecuteIfBound(MenuBarBuilder);
	
	const TSharedRef<SWidget> MenuWidget = MenuBarBuilder.MakeWidget();
	TabManager->SetAllowWindowMenuBar(true);
	TabManager->SetMenuMultiBox(MenuBarBuilder.GetMultiBox(), MenuWidget);
	
	return Result;
}

void SLiveLinkHubTabViewWithManagerBase::FillInDefaultMenuItems(FMenuBarBuilder MenuBarBuilder)
{
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("WindowMenuLabel", "Window"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateSP(this, &SLiveLinkHubTabViewWithManagerBase::FillWindowMenu),
		"Window"
	);
}

void SLiveLinkHubTabViewWithManagerBase::FillWindowMenu(FMenuBuilder& MenuBuilder)
{
	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
}

#undef LOCTEXT_NAMESPACE
