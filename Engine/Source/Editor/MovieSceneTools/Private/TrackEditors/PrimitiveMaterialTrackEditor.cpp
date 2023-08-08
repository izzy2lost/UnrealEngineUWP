// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PrimitiveMaterialTrackEditor.h"
#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "ISequencerModule.h"
#include "Components/PrimitiveComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Algo/Find.h"
#include "Components/MeshComponent.h"


#define LOCTEXT_NAMESPACE "PrimitiveMaterialTrackEditor"


FPrimitiveMaterialTrackEditor::FPrimitiveMaterialTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FKeyframeTrackEditor(InSequencer)
{}

TSharedRef<ISequencerTrackEditor> FPrimitiveMaterialTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShared<FPrimitiveMaterialTrackEditor>(OwningSequencer);
}

void FPrimitiveMaterialTrackEditor::ExtendObjectBindingTrackMenu(TSharedRef<FExtender> Extender, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(UPrimitiveComponent::StaticClass()))
	{
		Extender->AddMenuExtension(SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection, EExtensionHook::Before, nullptr, FMenuExtensionDelegate::CreateSP(this, &FPrimitiveMaterialTrackEditor::ConstructObjectBindingTrackMenu, ObjectBindings));
	}
}

void FPrimitiveMaterialTrackEditor::ConstructObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings)
{
	auto GetMaterialInfoForTrack = [](UMovieSceneTrack* InTrack)
	{
		UMovieScenePrimitiveMaterialTrack* MaterialTrack = Cast<UMovieScenePrimitiveMaterialTrack>(InTrack);
		return MaterialTrack ? MaterialTrack->GetMaterialInfo() : FComponentMaterialInfo();
	};

	UObject* Object = GetSequencer()->FindSpawnedObjectOrTemplate(ObjectBindings[0]);
	if (!Object)
	{
		return;
	}

	USceneComponent* SceneComponent = Cast<USceneComponent>(Object);
	if (!SceneComponent)
	{
		return;
	}	

	const UMovieScene* MovieScene = GetFocusedMovieScene();
	const FMovieSceneBinding* Binding = Algo::FindBy(MovieScene->GetBindings(), ObjectBindings[0], &FMovieSceneBinding::GetObjectGuid);

	if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(SceneComponent))
	{
		int32 NumMaterials = PrimitiveComponent->GetNumMaterials();
		TArray<FName> MaterialSlotNames = PrimitiveComponent->GetMaterialSlotNames();
		UMeshComponent* MeshComponent = Cast<UMeshComponent>(SceneComponent);
		if (NumMaterials > 0 || MeshComponent)
		{
			MenuBuilder.BeginSection(NAME_None, LOCTEXT("MaterialSwitcherTitle", "Material Switchers"));
			{
				for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; MaterialIndex++)
				{
					FName MaterialSlotName = MaterialSlotNames.IsValidIndex(MaterialIndex) ? MaterialSlotNames[MaterialIndex] : FName();
					FComponentMaterialInfo MaterialInfo{ MaterialSlotName, MaterialIndex, EComponentMaterialType::IndexedMaterial };

					const bool bAlreadyExists = Algo::FindBy(Binding->GetTracks(), MaterialInfo, GetMaterialInfoForTrack) != nullptr;
					if (bAlreadyExists)
					{
						continue;
					}
					FUIAction AddMaterialSwitcherAction(FExecuteAction::CreateSP(this, &FPrimitiveMaterialTrackEditor::CreateTrackForElement, ObjectBindings, MaterialInfo));
					FText MaterialSwitcherLabel = !MaterialSlotName.IsNone() ?
						FText::Format(LOCTEXT("MaterialSlot_Format", "Material Slot {0} Switcher"), FText::FromName(MaterialSlotName)) :
						FText::Format(LOCTEXT("MaterialID_Format", "Material Element {0} Switcher"), FText::AsNumber(MaterialIndex));
					FText MaterialSwitcherTooltip = !MaterialSlotName.IsNone() ?
						FText::Format(LOCTEXT("MaterialSlotTooltip_Format", "Add material switcher for slot {0}, index {1}"), FText::FromName(MaterialSlotName), FText::AsNumber(MaterialIndex)) :
						FText::Format(LOCTEXT("MaterialIDTooltip_Format", "Add material switcher for element {0}"), FText::AsNumber(MaterialIndex));
					MenuBuilder.AddMenuEntry(MaterialSwitcherLabel, MaterialSwitcherTooltip, FSlateIcon(), AddMaterialSwitcherAction);
				}
				if (MeshComponent)
				{
					FComponentMaterialInfo MaterialInfo{ FName(), 0, EComponentMaterialType::OverlayMaterial };
					const bool bAlreadyExists = Algo::FindBy(Binding->GetTracks(), MaterialInfo, GetMaterialInfoForTrack) != nullptr;
					if (!bAlreadyExists)
					{
						FUIAction AddMaterialSwitcherAction(FExecuteAction::CreateSP(this, &FPrimitiveMaterialTrackEditor::CreateTrackForElement, ObjectBindings, MaterialInfo));
						FText OverlayMaterialSwitcherLabel = LOCTEXT("OverlayMaterialSwitcher_Format", "Overlay Material Switcher");
						FText OverlayMaterialSwitcherTooltip = LOCTEXT("OverlayMaterialSwitcherTooltip_Format", "Add overlay material switcher");
						MenuBuilder.AddMenuEntry(OverlayMaterialSwitcherLabel, OverlayMaterialSwitcherTooltip, FSlateIcon(), AddMaterialSwitcherAction);
					}
				}
			}
			MenuBuilder.EndSection();
		}
	}
	else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(SceneComponent))
	{
		MenuBuilder.BeginSection("Materials", LOCTEXT("MaterialSection", "Material Parameters"));
		{
			FComponentMaterialInfo MaterialInfo{ FName(), 0, EComponentMaterialType::DecalMaterial };
			const bool bAlreadyExists = Algo::FindBy(Binding->GetTracks(), MaterialInfo, GetMaterialInfoForTrack) != nullptr;
			if (!bAlreadyExists)
			{
				FUIAction AddMaterialSwitcherAction(FExecuteAction::CreateSP(this, &FPrimitiveMaterialTrackEditor::CreateTrackForElement, ObjectBindings, MaterialInfo));
				FText DecalMaterialSwitcherLabel = LOCTEXT("DecalMaterialSwitcher_Format", "Decal Material Switcher");
				FText DecalMaterialSwitcherTooltip = LOCTEXT("DecalMaterialSwitcherTooltip_Format", "Add decal material switcher");
				MenuBuilder.AddMenuEntry(DecalMaterialSwitcherLabel, DecalMaterialSwitcherTooltip, FSlateIcon(), AddMaterialSwitcherAction);
			}
		}
		MenuBuilder.EndSection();
	}
}

void FPrimitiveMaterialTrackEditor::CreateTrackForElement(TArray<FGuid> ObjectBindingIDs, FComponentMaterialInfo MaterialInfo)
{
	UMovieScene* MovieScene = GetFocusedMovieScene();

	FScopedTransaction Transaction(LOCTEXT("CreateTrack", "Create Material Track"));
	MovieScene->Modify();

	for (FGuid ObjectBindingID : ObjectBindingIDs)
	{
		UMovieScenePrimitiveMaterialTrack* NewTrack = MovieScene->AddTrack<UMovieScenePrimitiveMaterialTrack>(ObjectBindingID);
		NewTrack->SetMaterialInfo(MaterialInfo);
		// Construct display names from MaterialInfo
		FText TrackDisplayName;
		FText TrackTooltipText;
		switch (MaterialInfo.MaterialType)
		{
		case EComponentMaterialType::Empty:
			break;
		case EComponentMaterialType::IndexedMaterial:
			TrackDisplayName = !MaterialInfo.MaterialSlotName.IsNone() ? FText::Format(LOCTEXT("SlotMaterialSwitcherTrackName", "Material Slot: {0}"), FText::FromName(MaterialInfo.MaterialSlotName))
				: FText::Format(LOCTEXT("IndexedMaterialSwitcherTrackName", "Material Element {0}"), FText::AsNumber(MaterialInfo.MaterialSlotIndex));

			TrackTooltipText = !MaterialInfo.MaterialSlotName.IsNone() ? FText::Format(LOCTEXT("SlotMaterialSwitcherTrackTooltip", "Material switcher for {0} at index {1}"), FText::FromName(MaterialInfo.MaterialSlotName), FText::AsNumber(MaterialInfo.MaterialSlotIndex))
				: FText::Format(LOCTEXT("IndexedMaterialSwitcherTrackTooltip", "Material switcher for element at index {0}"), FText::AsNumber(MaterialInfo.MaterialSlotIndex));
			break;
		case EComponentMaterialType::OverlayMaterial:
			TrackDisplayName = LOCTEXT("OverlayMaterialSwitcherTrackName", "Overlay Material");
			TrackTooltipText = LOCTEXT("OverlayMaterialSwitcherTrackTooltip", "Material switcher for overlay material");
			break;
		case EComponentMaterialType::DecalMaterial:
			TrackDisplayName = LOCTEXT("DecalMaterialSwitcherTrackName", "Decal Material");
			TrackTooltipText = LOCTEXT("DecalMaterialSwitcherTrackTooltip", "Material switcher for decal material");
			break;
		default:
			break;

		}
		if (!TrackDisplayName.IsEmpty())
		{
			NewTrack->SetDisplayName(TrackDisplayName);
		}
		if (!TrackTooltipText.IsEmpty())
		{
			NewTrack->SetDisplayNameTooltipText(TrackTooltipText);
		}

		NewTrack->AddSection(*NewTrack->CreateNewSection());
	}

	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

#undef LOCTEXT_NAMESPACE
