// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/MaterialTrackEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Components/DecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Sections/MovieSceneComponentMaterialParameterSection.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Sections/ComponentMaterialParameterSection.h"
#include "Sections/ParameterSection.h"
#include "SequencerUtilities.h"
#include "Modules/ModuleManager.h"
#include "MaterialEditorModule.h"
#include "Engine/Selection.h"
#include "ISequencerModule.h"


#define LOCTEXT_NAMESPACE "MaterialTrackEditor"


FMaterialTrackEditor::FMaterialTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer )
{
}


TSharedRef<ISequencerSection> FMaterialTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	UMovieSceneComponentMaterialParameterSection* ComponentMaterialParameterSection = Cast<UMovieSceneComponentMaterialParameterSection>(&SectionObject);
	UMovieSceneParameterSection* ParameterSection = Cast<UMovieSceneParameterSection>(&SectionObject);
	checkf( ComponentMaterialParameterSection != nullptr || ParameterSection != nullptr, TEXT("Unsupported section type.") );

	if (ComponentMaterialParameterSection)
	{
		return MakeShareable(new FComponentMaterialParameterSection(*ComponentMaterialParameterSection));
	}
	else
	{
		return MakeShareable(new FParameterSection(*ParameterSection));
	}
}


TSharedPtr<SWidget> FMaterialTrackEditor::BuildOutlinerEditWidget( const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params )
{
	UMovieSceneMaterialTrack* MaterialTrack = Cast<UMovieSceneMaterialTrack>(Track);
	FOnGetContent MenuContent = FOnGetContent::CreateSP(this, &FMaterialTrackEditor::OnGetAddMenuContent, ObjectBinding, MaterialTrack, Params.TrackInsertRowIndex);

	return FSequencerUtilities::MakeAddButton(LOCTEXT( "AddParameterButton", "Parameter" ), MenuContent, Params.NodeIsHovered, GetSequencer());
}


TSharedRef<SWidget> FMaterialTrackEditor::OnGetAddMenuContent( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack, int32 TrackInsertRowIndex )
{
	// IF this is supported, allow creating other sections with different blend types, and put
	// the material parameters after a separator. Otherwise, just show the parameters menu.
	const FMovieSceneBlendTypeField SupportedBlendTypes = MaterialTrack->GetSupportedBlendTypes();
	if (SupportedBlendTypes.Num() > 1)
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		TWeakPtr<ISequencer> WeakSequencer = GetSequencer();
		FSequencerUtilities::PopulateMenu_CreateNewSection(MenuBuilder, TrackInsertRowIndex, MaterialTrack, WeakSequencer);

		MenuBuilder.AddSeparator();

		OnBuildAddParameterMenu(MenuBuilder, ObjectBinding, MaterialTrack);

		return MenuBuilder.MakeWidget();
	}
	else
	{
		return OnGetAddParameterMenuContent(ObjectBinding, MaterialTrack);
	}
}


struct FParameterInfoAndAction
{
	FMaterialParameterInfo ParameterInfo;
	FText ParameterDisplayName;
	FUIAction Action;

	FParameterInfoAndAction(const FMaterialParameterInfo& InParameterInfo, FText InParameterDisplayName, FUIAction InAction )
	{
		ParameterInfo = InParameterInfo;
		ParameterDisplayName = InParameterDisplayName;
		Action = InAction;
	}

	bool operator<(FParameterInfoAndAction const& Other) const
	{
		if (ParameterInfo.Index == Other.ParameterInfo.Index)
		{
			if (ParameterInfo.Association == Other.ParameterInfo.Association)
			{
				return ParameterInfo.Name.LexicalLess(Other.ParameterInfo.Name);
			}
			return ParameterInfo.Association < Other.ParameterInfo.Association;
		}
		return ParameterInfo.Index < Other.ParameterInfo.Index;
	}
};


TSharedRef<SWidget> FMaterialTrackEditor::OnGetAddParameterMenuContent( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack )
{
	FMenuBuilder AddParameterMenuBuilder( true, nullptr );
	OnBuildAddParameterMenu(AddParameterMenuBuilder, ObjectBinding, MaterialTrack);
	return AddParameterMenuBuilder.MakeWidget();
}


void FMaterialTrackEditor::OnBuildAddParameterMenu( FMenuBuilder& MenuBuilder, FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack )
{
	UMaterial* Material = GetMaterialForTrack( ObjectBinding, MaterialTrack );
	if ( Material != nullptr )
	{
		UMaterialInterface* MaterialInterface = GetMaterialInterfaceForTrack(ObjectBinding, MaterialTrack);
		
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( MaterialInterface );	
		TArray<FMaterialParameterInfo> VisibleExpressions;

		IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
		bool bCollectedVisibleParameters = false;
		if (MaterialEditorModule && MaterialInstance)
		{
			MaterialEditorModule->GetVisibleMaterialParameters(Material, MaterialInstance, VisibleExpressions);
			bCollectedVisibleParameters = true;
		}

		TArray<FParameterInfoAndAction> ParameterInfosAndActions;

		// Collect scalar parameters.
		TArray<FMaterialParameterInfo> ScalarParameterInfos;
		TArray<FGuid> ScalarParameterGuids;
		MaterialInterface->GetAllScalarParameterInfo(ScalarParameterInfos, ScalarParameterGuids );
		// In case we need to grab layer names.
		FMaterialLayersFunctions Layers;
		MaterialInterface->GetMaterialLayers(Layers);

		auto GetMaterialParameterLayerName = [&Layers](const FMaterialParameterInfo& InParameterInfo)
		{
			FString LayerName;
			if (Layers.EditorOnly.LayerNames.IsValidIndex(InParameterInfo.Index))
			{
				LayerName = Layers.GetLayerName(InParameterInfo.Index).ToString();
			}
			return LayerName;
		};
		auto GetMaterialParameterAssetName = [&Layers](const FMaterialParameterInfo& InParameterInfo)
		{
			FString AssetName;
			if (InParameterInfo.Association == EMaterialParameterAssociation::LayerParameter && Layers.Layers.IsValidIndex(InParameterInfo.Index))
			{
				AssetName = Layers.Layers[InParameterInfo.Index]->GetName();
			}
			else if (InParameterInfo.Association == EMaterialParameterAssociation::BlendParameter && Layers.Blends.IsValidIndex(InParameterInfo.Index))
			{
				AssetName = Layers.Blends[InParameterInfo.Index]->GetName();
			}
			return AssetName;
		};

		auto GetMaterialParameterDisplayName = [](const FMaterialParameterInfo& InParameterInfo, const FString& InLayerName, const FString& InAssetName)
		{
			FText DisplayName = FText::FromName(InParameterInfo.Name);
			if (!InLayerName.IsEmpty() && !InAssetName.IsEmpty())
			{
				DisplayName = FText::Format(LOCTEXT("MaterialParameterDisplayName", "{0} ({1}.{2})"), DisplayName, FText::FromString(InLayerName), FText::FromString(InAssetName));
			}
			return DisplayName;
		};

		for (int32 ScalarParameterIndex = 0; ScalarParameterIndex < ScalarParameterInfos.Num(); ++ScalarParameterIndex)
		{
			FMaterialParameterInfo ScalarParameterInfo = ScalarParameterInfos[ScalarParameterIndex];
			if (!bCollectedVisibleParameters || VisibleExpressions.Contains(ScalarParameterInfo))
			{
				FString LayerName = GetMaterialParameterLayerName(ScalarParameterInfo);
				FString AssetName = GetMaterialParameterAssetName(ScalarParameterInfo);
				FText ParameterDisplayName = GetMaterialParameterDisplayName(ScalarParameterInfo, LayerName, AssetName);
				FUIAction AddParameterMenuAction( FExecuteAction::CreateSP( this, &FMaterialTrackEditor::AddScalarParameter, ObjectBinding, MaterialTrack, ScalarParameterInfo, LayerName, AssetName) );
				FParameterInfoAndAction InfoAndAction(ScalarParameterInfo, ParameterDisplayName, AddParameterMenuAction );
				ParameterInfosAndActions.Add(InfoAndAction);
			}
		}

		// Collect color parameters.
		TArray<FMaterialParameterInfo> ColorParameterInfos;
		TArray<FGuid> ColorParameterGuids;
		MaterialInterface->GetAllVectorParameterInfo(ColorParameterInfos, ColorParameterGuids );
		for (int32 ColorParameterIndex = 0; ColorParameterIndex < ColorParameterInfos.Num(); ++ColorParameterIndex)
		{
			FMaterialParameterInfo ColorParameterInfo = ColorParameterInfos[ColorParameterIndex];
			if (!bCollectedVisibleParameters || VisibleExpressions.Contains(ColorParameterInfo))
			{
				FString LayerName = GetMaterialParameterLayerName(ColorParameterInfo);
				FString AssetName = GetMaterialParameterAssetName(ColorParameterInfo);
				FText ParameterDisplayName = GetMaterialParameterDisplayName(ColorParameterInfo, LayerName, AssetName);
				FUIAction AddParameterMenuAction( FExecuteAction::CreateSP( this, &FMaterialTrackEditor::AddColorParameter, ObjectBinding, MaterialTrack, ColorParameterInfo, LayerName, AssetName ) );
				FParameterInfoAndAction InfoAndAction(ColorParameterInfo, ParameterDisplayName, AddParameterMenuAction );
				ParameterInfosAndActions.Add(InfoAndAction);
			}
		}

		// Sort and generate menu.
		ParameterInfosAndActions.Sort();

		for (FParameterInfoAndAction InfoAndAction : ParameterInfosAndActions)
		{
			MenuBuilder.AddMenuEntry(InfoAndAction.ParameterDisplayName, FText(), FSlateIcon(), InfoAndAction.Action );
		}
	}
}


UMaterial* FMaterialTrackEditor::GetMaterialForTrack( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack )
{
	UMaterialInterface* MaterialInterface = GetMaterialInterfaceForTrack( ObjectBinding, MaterialTrack );
	if ( MaterialInterface != nullptr )
	{
		UMaterial* Material = Cast<UMaterial>( MaterialInterface );
		if ( Material != nullptr )
		{
			return Material;
		}
		else
		{
			UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( MaterialInterface );
			if ( MaterialInstance != nullptr )
			{
				return MaterialInstance->GetMaterial();
			}
		}
	}
	return nullptr;
}


void FMaterialTrackEditor::AddScalarParameter( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack, FMaterialParameterInfo ParameterInfo, FString InLayerName, FString InAssetName)
{
	FFrameNumber KeyTime = GetTimeForKey();

	UMaterialInterface* Material = GetMaterialInterfaceForTrack(ObjectBinding, MaterialTrack);
	if (Material != nullptr)
	{
		const FScopedTransaction Transaction( LOCTEXT( "AddScalarParameter", "Add scalar parameter" ) );
		float ParameterValue;
		Material->GetScalarParameterValue(ParameterInfo, ParameterValue);
		MaterialTrack->Modify();
		MaterialTrack->AddScalarParameterKey(ParameterInfo, KeyTime, ParameterValue, InLayerName, InAssetName);
	}
	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}


void FMaterialTrackEditor::AddColorParameter( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack, FMaterialParameterInfo ParameterInfo, FString InLayerName, FString InAssetName)
{
	FFrameNumber KeyTime = GetTimeForKey();

	UMaterialInterface* Material = GetMaterialInterfaceForTrack( ObjectBinding, MaterialTrack );
	if ( Material != nullptr )
	{
		const FScopedTransaction Transaction( LOCTEXT( "AddVectorParameter", "Add vector parameter" ) );
		FLinearColor ParameterValue;
		Material->GetVectorParameterValue(ParameterInfo, ParameterValue );
		MaterialTrack->Modify();
		MaterialTrack->AddColorParameterKey(ParameterInfo, KeyTime, ParameterValue, InLayerName, InAssetName);
	}
	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}


FComponentMaterialTrackEditor::FComponentMaterialTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMaterialTrackEditor( InSequencer )
{
}


TSharedRef<ISequencerTrackEditor> FComponentMaterialTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable( new FComponentMaterialTrackEditor( OwningSequencer ) );
}


bool FComponentMaterialTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneComponentMaterialTrack::StaticClass();
}

bool FComponentMaterialTrackEditor::GetDefaultExpansionState(UMovieSceneTrack* InTrack) const
{
	return true;
}


UMaterialInterface* FComponentMaterialTrackEditor::GetMaterialInterfaceForTrack( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack )
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr.IsValid())
	{
		return nullptr;
	}

	UMovieSceneComponentMaterialTrack* ComponentMaterialTrack = Cast<UMovieSceneComponentMaterialTrack>( MaterialTrack );
	if (!ComponentMaterialTrack)
	{
		return nullptr;
	}

	UObject* Object = GetSequencer()->FindSpawnedObjectOrTemplate(ObjectBinding);
	if (!Object)
	{
		return nullptr;
	}

	if (UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Object))
	{
		return Component->GetMaterial( ComponentMaterialTrack->GetMaterialIndex() );
	}
	else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(Object))
	{
		return DecalComponent->GetDecalMaterial();
	}

	return nullptr;
}

void FComponentMaterialTrackEditor::ExtendObjectBindingTrackMenu(TSharedRef<FExtender> Extender, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(UPrimitiveComponent::StaticClass()) || ObjectClass->IsChildOf(UDecalComponent::StaticClass()))
	{
		Extender->AddMenuExtension(SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection, EExtensionHook::Before, nullptr, FMenuExtensionDelegate::CreateSP(this, &FComponentMaterialTrackEditor::ConstructObjectBindingTrackMenu, ObjectBindings));
	}
}

void FComponentMaterialTrackEditor::ConstructObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings)
{
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

	if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(SceneComponent))
	{
		int32 NumMaterials = PrimitiveComponent->GetNumMaterials();
		if (NumMaterials > 0)
		{
			MenuBuilder.BeginSection("Materials", LOCTEXT("MaterialSection", "Material Parameters"));
			{
				for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; MaterialIndex++)
				{
					FUIAction AddComponentMaterialAction(FExecuteAction::CreateRaw(this, &FComponentMaterialTrackEditor::HandleAddComponentMaterialActionExecute, SceneComponent, MaterialIndex));
					FText AddComponentMaterialLabel = FText::Format(LOCTEXT("ComponentMaterialIndexLabelFormat", "Element {0}"), FText::AsNumber(MaterialIndex));
					FText AddComponentMaterialToolTip = FText::Format(LOCTEXT("ComponentMaterialIndexToolTipFormat", "Add material element {0}"), FText::AsNumber(MaterialIndex));
					MenuBuilder.AddMenuEntry(AddComponentMaterialLabel, AddComponentMaterialToolTip, FSlateIcon(), AddComponentMaterialAction);
				}
			}
			MenuBuilder.EndSection();
		}
	}
	else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(SceneComponent))
	{
		if (UMaterialInterface* DecalMaterial = DecalComponent->GetDecalMaterial())
		{
			MenuBuilder.BeginSection("Materials", LOCTEXT("MaterialSection", "Material Parameters"));
			{
				FUIAction AddComponentMaterialAction(FExecuteAction::CreateRaw(this, &FComponentMaterialTrackEditor::HandleAddComponentMaterialActionExecute, SceneComponent, 0));
				FText AddDecalMaterialToolTip = FText::Format(LOCTEXT("AddDecalMaterialToolTipFormat", "Add decal material {0}"), FText::FromString(DecalMaterial->GetName()));
				MenuBuilder.AddMenuEntry(FText::FromString(DecalMaterial->GetName()), AddDecalMaterialToolTip, FSlateIcon(), AddComponentMaterialAction);
			}
			MenuBuilder.EndSection();
		}
	}
}

void FComponentMaterialTrackEditor::HandleAddComponentMaterialActionExecute(USceneComponent* Component, int32 MaterialIndex)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	UMovieScene* MovieScene = SequencerPtr->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (MovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddComponentMaterialTrack", "Add component material track"));

	MovieScene->Modify();

	FString ComponentName = Component->GetName();

	TArray<UActorComponent*> ActorComponents;
	ActorComponents.Add(Component);

	USelection* SelectedActors = GEditor->GetSelectedActors();
	if (SelectedActors && SelectedActors->Num() > 0)
	{
		for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			AActor* Actor = CastChecked<AActor>(*Iter);

			TArray<UActorComponent*> OutActorComponents;
			Actor->GetComponents(OutActorComponents);
			for (UActorComponent* ActorComponent : OutActorComponents)
			{
				if (ActorComponent->GetName() == ComponentName)
				{
					ActorComponents.AddUnique(ActorComponent);
				}
			}
		}
	}

	for (UActorComponent* ActorComponent : ActorComponents)
	{
		FGuid ObjectHandle = SequencerPtr->GetHandleToObject(ActorComponent);
		FName IndexName(*FString::FromInt(MaterialIndex));
		if (MovieScene->FindTrack(UMovieSceneComponentMaterialTrack::StaticClass(), ObjectHandle, IndexName) == nullptr)
		{
			UMovieSceneComponentMaterialTrack* MaterialTrack = MovieScene->AddTrack<UMovieSceneComponentMaterialTrack>(ObjectHandle);
			MaterialTrack->Modify();
			MaterialTrack->SetMaterialIndex(MaterialIndex);
		}
	}

	SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}



#undef LOCTEXT_NAMESPACE
