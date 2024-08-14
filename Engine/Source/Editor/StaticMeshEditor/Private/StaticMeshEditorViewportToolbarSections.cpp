// Copyright Epic Games, Inc. All Rights Reserved.

#include "StaticMeshEditorViewportToolbarSections.h"

#include "SStaticMeshEditorViewport.h"
#include "StaticMeshViewportLODCommands.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ViewportToolbar/UnrealEdViewportToolbarContext.h"

#define LOCTEXT_NAMESPACE "StaticMeshEditorViewportToolbarSections"

FText UE::StaticMeshEditor::GetLODMenuLabel(const TSharedPtr<SStaticMeshEditorViewport>& InStaticMeshEditorViewport)
{
	FText Label = LOCTEXT("LODMenu_AutoLabel", "LOD Auto");

	if (InStaticMeshEditorViewport)
	{
		int32 LODSelectionType = InStaticMeshEditorViewport->GetLODSelection();

		if (LODSelectionType > 0)
		{
			FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODSelectionType - 1);
			Label = FText::FromString(TitleLabel);
		}
	}

	return Label;
}

FToolMenuEntry UE::StaticMeshEditor::CreateLODSubmenu()
{
	return FToolMenuEntry::InitDynamicEntry(
		"DynamicLODOptions",
		FNewToolMenuSectionDelegate::CreateLambda(
			[](FToolMenuSection& InDynamicSection) -> void
			{
				if (UUnrealEdViewportToolbarContext* const EditorViewportContext =
						InDynamicSection.FindContext<UUnrealEdViewportToolbarContext>())
				{
					TWeakPtr<SStaticMeshEditorViewport> StaticMeshEditorViewportWeak =
						StaticCastSharedPtr<SStaticMeshEditorViewport>(EditorViewportContext->Viewport.Pin());

					// Label updates based on currently selected LOD
					const TAttribute<FText> Label = TAttribute<FText>::CreateLambda(
						[StaticMeshEditorViewportWeak]()
						{
							if (TSharedPtr<SStaticMeshEditorViewport> Viewport = StaticMeshEditorViewportWeak.Pin())
							{
								return GetLODMenuLabel(Viewport);
							}

							return LOCTEXT("LODSubmenuLabel", "LOD");
						}
					);

					InDynamicSection.AddSubMenu(
						"LOD",
						Label,
						LOCTEXT("LODSubmenuTooltip", ""),
						FNewToolMenuDelegate::CreateLambda(
							[StaticMeshEditorViewportWeak](UToolMenu* Submenu) -> void
							{
								if (TSharedPtr<SStaticMeshEditorViewport> Viewport = StaticMeshEditorViewportWeak.Pin())
								{
									FToolMenuSection& UnnamedSection =
										Submenu->FindOrAddSection("", LOCTEXT("UnnamedLabel", ""));
									TSharedRef<SWidget> LODMenuWidget =
										UE::StaticMeshEditor::GenerateLODMenuWidget(Viewport);
									FToolMenuEntry LODSubmenu = FToolMenuEntry::InitWidget("LOD", LODMenuWidget, FText());

									UnnamedSection.AddEntry(LODSubmenu);
								}
							}
						)
					);
				}
			}
		)
	);
}

TSharedRef<SWidget> UE::StaticMeshEditor::GenerateLODMenuWidget(const TSharedPtr<SStaticMeshEditorViewport>& InStaticMeshEditorViewport
)
{
	if (!InStaticMeshEditorViewport)
	{
		return SNullWidget::NullWidget;
	}

	const FStaticMeshViewportLODCommands& Actions = FStaticMeshViewportLODCommands::Get();

	TSharedPtr<FExtender> MenuExtender = InStaticMeshEditorViewport->GetExtenders();

	constexpr bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(
		bInShouldCloseWindowAfterMenuSelection, InStaticMeshEditorViewport->GetCommandList(), MenuExtender
	);

	InMenuBuilder.PushCommandList(InStaticMeshEditorViewport->GetCommandList().ToSharedRef());
	if (MenuExtender.IsValid())
	{
		InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());
	}

	{
		// LOD Models
		InMenuBuilder.BeginSection("StaticMeshViewportPreviewLODs", LOCTEXT("ShowLOD_PreviewLabel", "Preview LODs"));
		{
			InMenuBuilder.AddMenuEntry(Actions.LODAuto);
			InMenuBuilder.AddMenuEntry(Actions.LOD0);

			int32 LODCount = InStaticMeshEditorViewport->GetLODModelCount();
			for (int32 LODId = 1; LODId < LODCount; ++LODId)
			{
				FString TitleLabel = FString::Printf(TEXT(" LOD %d"), LODId);

				FUIAction Action(
					FExecuteAction::CreateSP(
						InStaticMeshEditorViewport.ToSharedRef(), &SStaticMeshEditorViewport::OnSetLODModel, LODId + 1
					),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(
						InStaticMeshEditorViewport.ToSharedRef(), &SStaticMeshEditorViewport::IsLODModelSelected, LODId + 1
					)
				);

				InMenuBuilder.AddMenuEntry(
					FText::FromString(TitleLabel), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::RadioButton
				);
			}
		}
		InMenuBuilder.EndSection();
	}

	InMenuBuilder.PopCommandList();
	if (MenuExtender.IsValid())
	{
		InMenuBuilder.PopExtender();
	}

	return InMenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
