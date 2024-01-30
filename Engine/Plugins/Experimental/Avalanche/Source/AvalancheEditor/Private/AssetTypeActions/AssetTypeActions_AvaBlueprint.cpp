// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_AvaBlueprint.h"
#include "AvaBlueprint.h"
#include "AvaEditorFunctionLibrary.h"
#include "Misc/MessageDialog.h"
#include "SBlueprintDiff.h"
#include "Styling/SlateIconFinder.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_AvaBlueprint"

FText FAssetTypeActions_AvaBlueprint::GetName() const
{
	return LOCTEXT("AssetTypeActions_AvaBlueprint", "Motion Design Blueprint");
}

UClass* FAssetTypeActions_AvaBlueprint::GetSupportedClass() const
{
	return UAvalancheBlueprint::StaticClass();
}

void FAssetTypeActions_AvaBlueprint::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	const TArray<TWeakObjectPtr<UAvalancheBlueprint>> AvalancheBlueprints = GetTypedWeakObjectPtrs<UAvalancheBlueprint>(InObjects);

	Section.AddMenuEntry(TEXT("AvaBlueprint_ExportToWorld")
		, LOCTEXT("AvaBlueprint_ExportToLevelLabel", "Export to World")
		, LOCTEXT("AvaBlueprint_ExportToLevelTooltip", "Creates a World Asset for this Motion Design Scene populated with the Motion Design Scene Data")
		, FSlateIconFinder::FindIconForClass(UWorld::StaticClass())
		, FUIAction(FExecuteAction::CreateStatic(&FAvaEditorFunctionLibrary::ExportAvaBlueprintsToWorld, AvalancheBlueprints)));
}

void FAssetTypeActions_AvaBlueprint::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	TSharedRef<FSimpleAssetEditor> NewEditor = MakeShared<FSimpleAssetEditor>();
	NewEditor->InitEditor(EToolkitMode::Standalone, nullptr, InObjects, FSimpleAssetEditor::FGetDetailsViewObjects());
	NewEditor->SetPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateLambda([]{ return false; }));
}

void FAssetTypeActions_AvaBlueprint::PerformAssetDiff(UObject* Asset1, UObject* Asset2,
	const FRevisionInfo& OldRevision, const FRevisionInfo& NewRevision) const
{
	UBlueprint* OldBlueprint = CastChecked<UBlueprint>(Asset1);
	UBlueprint* NewBlueprint = CastChecked<UBlueprint>(Asset2);

	// sometimes we're comparing different revisions of one single asset (other
	// times we're comparing two completely separate assets altogether)
	const bool bIsSingleAsset = (NewBlueprint->GetName() == OldBlueprint->GetName());

	FText WindowTitle = LOCTEXT("NamelessAvalancheBlueprintDiff", "Motion Design Blueprint Diff");
	// if we're diffing one asset against itself
	if (bIsSingleAsset)
	{
		// identify the assumed single asset in the window's title
		WindowTitle = FText::Format(LOCTEXT("AvalancheBlueprintDiff", "{0} - Motion Design Blueprint Diff"), FText::FromString(NewBlueprint->GetName()));
	}

	SBlueprintDiff::CreateDiffWindow(WindowTitle, OldBlueprint, NewBlueprint, OldRevision, NewRevision);
}

#undef LOCTEXT_NAMESPACE
