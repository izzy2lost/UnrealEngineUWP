// Copyright Epic Games, Inc. All Rights Reserved.


#include "SEditorViewportViewMenu.h"
#include "SEditorViewportViewMenuContext.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "RenderResource.h"
#include "SEditorViewport.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"

#define LOCTEXT_NAMESPACE "EditorViewportViewMenu"

const FName SEditorViewportViewMenu::BaseMenuName("UnrealEd.ViewportToolbar.View");

void SEditorViewportViewMenu::Construct( const FArguments& InArgs, TSharedRef<SEditorViewport> InViewport, TSharedRef<class SViewportToolBar> InParentToolBar )
{
	Viewport = InViewport;
	MenuName = BaseMenuName;
	MenuExtenders = InArgs._MenuExtenders;

	SEditorViewportToolbarMenu::Construct
	(
		SEditorViewportToolbarMenu::FArguments()
			.ParentToolBar( InParentToolBar)
			.Cursor( EMouseCursor::Default )
			.Label(this, &SEditorViewportViewMenu::GetViewMenuLabel)
			.LabelIcon(this, &SEditorViewportViewMenu::GetViewMenuLabelIcon)
			.OnGetMenuContent( this, &SEditorViewportViewMenu::GenerateViewMenuContent )
	);
}

FText SEditorViewportViewMenu::GetViewMenuLabel() const
{
	return UE::UnrealEd::GetViewModesSubmenuLabel(Viewport);
}

const FSlateBrush* SEditorViewportViewMenu::GetViewMenuLabelIcon() const
{

	TSharedPtr< SEditorViewport > PinnedViewport = Viewport.Pin();
	if( PinnedViewport.IsValid() )
	{
		const TSharedPtr<FEditorViewportClient> ViewportClient = PinnedViewport->GetViewportClient();
		check(ViewportClient.IsValid());
		const EViewModeIndex ViewMode = ViewportClient->GetViewMode();

		return UViewModeUtils::GetViewModeDisplayIcon(ViewMode);
	}

	return FStyleDefaults::GetNoBrush();
}

void SEditorViewportViewMenu::RegisterMenus() const
{
	if (!UToolMenus::Get()->IsMenuRegistered(BaseMenuName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(BaseMenuName);
		Menu->AddDynamicSection("BaseSection", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			if (UEditorViewportViewMenuContext* Context = InMenu->FindContext<UEditorViewportViewMenuContext>())
			{
				Context->EditorViewportViewMenu.Pin()->FillViewMenu(InMenu);
			}
		}));
	}
}

TSharedRef<SWidget> SEditorViewportViewMenu::GenerateViewMenuContent() const
{
	RegisterMenus();

	UEditorViewportViewMenuContext* ContextObject = NewObject<UEditorViewportViewMenuContext>();
	ContextObject->EditorViewportViewMenu = SharedThis(this);

	FToolMenuContext MenuContext(Viewport.Pin()->GetCommandList(), MenuExtenders, ContextObject);
	return UToolMenus::Get()->GenerateWidget(MenuName, MenuContext);
}

void SEditorViewportViewMenu::FillViewMenu(UToolMenu* Menu) const
{
	UE::UnrealEd::IsViewModeSupportedDelegate IsViewModeSupported = UE::UnrealEd::IsViewModeSupportedDelegate::CreateLambda(
		[WeakToolBar = ParentToolBar](EViewModeIndex ViewModeIndex) -> bool {
			if (TSharedPtr<SViewportToolBar> ToolBar = WeakToolBar.Pin())
			{
				return ToolBar->IsViewModeSupported(ViewModeIndex);
			}
			return true;
		});

	UE::UnrealEd::FillViewMenu(Menu, Viewport.Pin().ToSharedRef(), IsViewModeSupported);
}

#undef LOCTEXT_NAMESPACE
