// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStaticMeshEditorViewportToolBar.h"
#include "SStaticMeshEditorViewport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/Package.h"
#include "Components/StaticMeshComponent.h"
#include "Styling/AppStyle.h"
#include "Engine/StaticMesh.h"
#include "IStaticMeshEditor.h"
#include "StaticMeshEditorActions.h"
#include "Slate/SceneViewport.h"
#include "ComponentReregisterContext.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "Widgets/Docking/SDockTab.h"
#include "Engine/StaticMeshSocket.h"
#include "SEditorViewportToolBarMenu.h"
#include "StaticMeshViewportLODCommands.h"
#include "PreviewProfileController.h"
#include "StaticMeshEditorViewportToolbarSections.h"

#define LOCTEXT_NAMESPACE "StaticMeshEditorViewportToolbar"

///////////////////////////////////////////////////////////
// SStaticMeshEditorViewportToolbar


void SStaticMeshEditorViewportToolbar::Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments().PreviewProfileController(MakeShared<FPreviewProfileController>()), InInfoProvider);
}

// SCommonEditorViewportToolbarBase interface
TSharedRef<SWidget> SStaticMeshEditorViewportToolbar::GenerateShowMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();

	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		auto Commands = FStaticMeshEditorCommands::Get();

		ShowMenuBuilder.AddMenuEntry(Commands.SetShowNaniteFallback);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowDistanceField);

		ShowMenuBuilder.BeginSection("MeshComponents", LOCTEXT("MeshComponments", "Mesh Components"));
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowSockets);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowVertices);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowVertexColor);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowNormals);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowTangents);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowBinormals);
		ShowMenuBuilder.AddMenuSeparator();

		ShowMenuBuilder.AddMenuEntry(Commands.SetShowPivot);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowGrid);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowBounds);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowSimpleCollision);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowComplexCollision);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowPhysicalMaterialMasks);



		//ShowMenuBuilder.AddMenuSeparator();
		//ShowMenuBuilder.AddMenuEntry(Commands.SetShowMeshEdges);
	}

	return ShowMenuBuilder.MakeWidget();
}

// SCommonEditorViewportToolbarBase interface
void SStaticMeshEditorViewportToolbar::ExtendLeftAlignedToolbarSlots(TSharedPtr<SHorizontalBox> MainBoxPtr, TSharedPtr<SViewportToolBar> ParentToolBarPtr) const 
{
	const FMargin ToolbarSlotPadding(2.0f, 2.0f);

	if (!MainBoxPtr.IsValid())
	{
		return;
	}

	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.Label(this, &SStaticMeshEditorViewportToolbar::GetLODMenuLabel)
			.OnGetMenuContent(this, &SStaticMeshEditorViewportToolbar::GenerateLODMenu)
			.Cursor(EMouseCursor::Default)
			.ParentToolBar(ParentToolBarPtr)
		];
}

FText SStaticMeshEditorViewportToolbar::GetLODMenuLabel() const
{
	TSharedRef<SEditorViewport> BaseViewportRef = GetInfoProvider().GetViewportWidget();
	TSharedRef<SStaticMeshEditorViewport> ViewportRef = StaticCastSharedRef<SStaticMeshEditorViewport, SEditorViewport>(BaseViewportRef);

	return UE::StaticMeshEditor::GetLODMenuLabel(ViewportRef);
}

TSharedRef<SWidget> SStaticMeshEditorViewportToolbar::GenerateLODMenu() const
{
	TSharedRef<SEditorViewport> BaseViewportRef = GetInfoProvider().GetViewportWidget();
	TSharedRef<SStaticMeshEditorViewport> ViewportRef = StaticCastSharedRef<SStaticMeshEditorViewport, SEditorViewport>(BaseViewportRef);

	return UE::StaticMeshEditor::GenerateLODMenuWidget(ViewportRef);
}

#undef LOCTEXT_NAMESPACE
