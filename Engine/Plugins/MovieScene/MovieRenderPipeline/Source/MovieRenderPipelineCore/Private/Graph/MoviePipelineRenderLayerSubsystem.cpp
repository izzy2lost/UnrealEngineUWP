// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MoviePipelineRenderLayerSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "UObject/Package.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

#if WITH_EDITOR
#include "ActorFolderPickingMode.h"
#include "ActorFolderTreeItem.h"
#include "ActorMode.h"
#include "ActorTreeItem.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "ComponentTreeItem.h"
#include "ContentBrowserDataSource.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "EditorActorFolders.h"
#include "Graph/MovieGraphSharedWidgets.h"
#include "IContentBrowserSingleton.h"
#include "ISceneOutliner.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "SClassViewer.h"
#include "Selection.h"
#endif

#define LOCTEXT_NAMESPACE "MovieGraph"

namespace UE::MovieGraph::Private
{
#if WITH_EDITOR
	/**
	 * A filter that can be used in the class viewer that appears in the Add menu. Filters out specified classes, and optionally filters out classes
	 * that do not have a specific base class.
	 */
	class FClassViewerTypeFilter final : public IClassViewerFilter
	{
	public:
		explicit FClassViewerTypeFilter(TArray<UClass*>* InClassesToDisallow, UClass* InRequiredBaseClass = nullptr)
			: ClassesToDisallow(InClassesToDisallow)
			, RequiredBaseClass(InRequiredBaseClass)
		{
			
		}

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
		{
			return
				InClass &&
				!ClassesToDisallow->Contains(InClass) &&
				(RequiredBaseClass ? InClass->IsChildOf(RequiredBaseClass) : true);
		}
		
		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
		{
			return false;
		}

	private:
		/** Classes which should be prevented from showing up in the class viewer. */
		TArray<UClass*>* ClassesToDisallow;

		/** Classes must have this base class to pass the filter. */
		UClass* RequiredBaseClass = nullptr;
	};
#endif
}

void UMoviePipelineMaterialModifier::ApplyModifier(const UWorld* World)
{
	UMaterialInterface* NewMaterial = Material.LoadSynchronous();
	if (!NewMaterial)
	{
		return;
	}

	ModifiedComponents.Empty();
	
	for (const UMovieGraphCollection* Collection : Collections)
	{
		if (!Collection)
		{
			continue;
		}

		for (const AActor* Actor : Collection->Evaluate(World))
		{
			const bool bIncludeFromChildActors = true;
			TArray<UPrimitiveComponent*> PrimitiveComponents;
			Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents, bIncludeFromChildActors);

			for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
			{
				TArray<FMaterialSlotAssignment>& ModifiedMaterials = ModifiedComponents.FindOrAdd(PrimitiveComponent);
				
				for (int32 Index = 0; Index < PrimitiveComponent->GetNumMaterials(); ++Index)
				{
					ModifiedMaterials.Add(FMaterialSlotAssignment(Index, PrimitiveComponent->GetMaterial(Index)));
				
					PrimitiveComponent->SetMaterial(Index, NewMaterial);
				}
			}
		}
	}
}

void UMoviePipelineMaterialModifier::UndoModifier()
{
	for (const FComponentToMaterialMap::ElementType& ModifiedComponent : ModifiedComponents) 
	{
		UPrimitiveComponent* MeshComponent = ModifiedComponent.Key.LoadSynchronous();
		const TArray<FMaterialSlotAssignment>& OldMaterials = ModifiedComponent.Value;

		if (!MeshComponent)
		{
			continue;
		}

		for (const FMaterialSlotAssignment& MaterialPair : OldMaterials)
		{
			UMaterialInterface* MaterialInterface = MaterialPair.Value.LoadSynchronous();
			if (!MaterialInterface)
			{
				continue;
			}

			const int32 ElementIndex = MaterialPair.Key;
			MeshComponent->SetMaterial(ElementIndex, MaterialInterface);
		}
	}

	ModifiedComponents.Empty();
}

UMoviePipelineVisibilityModifier::UMoviePipelineVisibilityModifier()
	: bIsHidden(false)
	, bCastsShadows(true)
	, bCastShadowWhileHidden(false)
	, bAffectIndirectLightingWhileHidden(false)
	, bHoldout(false)
{
	// Note: The default modifier values here reflect the defaults on the scene component. If a modifier property is marked as overridden, the
	// override will initially be a no-op due to the defaults being the same.
}

void UMoviePipelineVisibilityModifier::ApplyModifier(const UWorld* World)
{
	ModifiedActors.Empty();

	// SetActorVisibilityState() takes a FActorVisibilityState, but all calls here will contain the same settings (with
	// a differing actor), so create one copy to prevent constantly re-creating structs.
	FActorVisibilityState NewVisibilityState;
	NewVisibilityState.bIsHidden = bIsHidden;

	// Create a stub component representation since we're just using this to propagate to all children right now.
	FActorVisibilityState::FComponentState& NewComponentState = NewVisibilityState.Components.AddDefaulted_GetRef();
	NewComponentState.bCastsShadows = bCastsShadows;
	NewComponentState.bCastShadowWhileHidden = bCastShadowWhileHidden;
	NewComponentState.bAffectIndirectLightingWhileHidden = bAffectIndirectLightingWhileHidden;
	NewComponentState.bHoldout = bHoldout;
	
	for (const UMovieGraphCollection* Collection : Collections)
	{
		if (!Collection)
		{
			continue;
		}

		const TSet<AActor*> MatchingActors = Collection->Evaluate(World);
		ModifiedActors.Reserve(MatchingActors.Num());

		for (const AActor* Actor : MatchingActors)
		{
			// Save out visibility state before the modifier is applied
			FActorVisibilityState OriginalVisibilityState;
			OriginalVisibilityState.Actor = Actor;
			OriginalVisibilityState.bIsHidden = Actor->IsHidden();

			const bool bIncludeFromChildActors = true;
			TInlineComponentArray<USceneComponent*> Components;
			Actor->GetComponents<USceneComponent>(Components, bIncludeFromChildActors);

			OriginalVisibilityState.Components.Reserve(Components.Num());
			for(const USceneComponent* SceneComponent : Components)
			{
				FActorVisibilityState::FComponentState& ComponentState = OriginalVisibilityState.Components.AddDefaulted_GetRef();
				ComponentState.Component = SceneComponent;

				// Cache the state
				if (const UPrimitiveComponent* AsPrimitiveComponent = Cast<UPrimitiveComponent>(SceneComponent))
				{
					ComponentState.bCastsShadows = AsPrimitiveComponent->CastShadow;
					ComponentState.bCastShadowWhileHidden = AsPrimitiveComponent->bCastHiddenShadow;
					ComponentState.bAffectIndirectLightingWhileHidden = AsPrimitiveComponent->bAffectIndirectLightingWhileHidden;
					ComponentState.bHoldout = AsPrimitiveComponent->bHoldout;
				}
				// Volumetrics are special cases as they don't inherit from UPrimitiveComponent, and don't support all of the flags.
				else if (const UVolumetricCloudComponent* AsVolumetricCloudComponent = Cast<UVolumetricCloudComponent>(SceneComponent))
				{
					ComponentState.bHoldout = AsVolumetricCloudComponent->bHoldout;
				}
				else if (const USkyAtmosphereComponent* AsSkyAtmosphereComponent = Cast<USkyAtmosphereComponent>(SceneComponent))
				{
					ComponentState.bHoldout = AsSkyAtmosphereComponent->bHoldout;
				}
				else if (const UExponentialHeightFogComponent* AsExponentialHeightFogComponent = Cast<UExponentialHeightFogComponent>(SceneComponent))
				{
					ComponentState.bHoldout = AsExponentialHeightFogComponent->bHoldout;
				}

				// Then override it. We override it one component at a time so that we can avoid making a bunch of
				// copies of NewVisibilityState for the varying number of components you might have.
				NewVisibilityState.Actor = Actor;
				NewVisibilityState.Components[0].Component = SceneComponent;
				SetActorVisibilityState(NewVisibilityState);
			}
			
			ModifiedActors.Add(OriginalVisibilityState);

		}
	}
}

void UMoviePipelineVisibilityModifier::UndoModifier()
{
	for (const FActorVisibilityState& PrevVisibilityState : ModifiedActors)
	{
		SetActorVisibilityState(PrevVisibilityState);
	}

	ModifiedActors.Empty();
}

void UMoviePipelineVisibilityModifier::SetActorVisibilityState(const FActorVisibilityState& NewVisibilityState)
{
	const TSoftObjectPtr<AActor> Actor = NewVisibilityState.Actor.LoadSynchronous();
	if (!Actor)
	{
		return;
	}

	if (bOverride_bIsHidden)
	{
		Actor->SetActorHiddenInGame(NewVisibilityState.bIsHidden);

#if WITH_EDITOR
		Actor->SetIsTemporarilyHiddenInEditor(NewVisibilityState.bIsHidden);
#endif
	}

	for (const FActorVisibilityState::FComponentState& ComponentState : NewVisibilityState.Components)
	{
		// TODO: These could potentially cause a large rendering penalty due to dirtying the render state; investigate potential
		// ways to optimize this
		if (!ComponentState.Component)
		{
			continue;
		}

		if (UPrimitiveComponent* AsPrimitiveComponent = Cast<UPrimitiveComponent>(ComponentState.Component.Get()))
		{
			if (bOverride_bCastsShadows)
			{
				AsPrimitiveComponent->SetCastShadow(ComponentState.bCastsShadows);
			}

			if (bOverride_bCastShadowWhileHidden)
			{
				AsPrimitiveComponent->SetCastHiddenShadow(ComponentState.bCastShadowWhileHidden);
			}

			if (bOverride_bAffectIndirectLightingWhileHidden)
			{
				AsPrimitiveComponent->SetAffectIndirectLightingWhileHidden(ComponentState.bAffectIndirectLightingWhileHidden);
			}

			if (bOverride_bHoldout)
			{
				AsPrimitiveComponent->SetHoldout(ComponentState.bHoldout);
			}
		}
		// Volumetrics are special cases as they don't inherit from UPrimitiveComponent, and don't support all of the flags.
		else if (UVolumetricCloudComponent* AsVolumetricCloudComponent = Cast<UVolumetricCloudComponent>(ComponentState.Component.Get()))
		{
			if (bOverride_bHoldout)
			{
				AsVolumetricCloudComponent->SetHoldout(ComponentState.bHoldout);
			}
		}
		else if (USkyAtmosphereComponent* AsSkyAtmosphereComponent = Cast<USkyAtmosphereComponent>(ComponentState.Component.Get()))
		{
			if (bOverride_bHoldout)
			{
				AsSkyAtmosphereComponent->SetHoldout(ComponentState.bHoldout);
			}
		}
		else if (UExponentialHeightFogComponent* AsExponentialHeightFogComponent = Cast<UExponentialHeightFogComponent>(ComponentState.Component.Get()))
		{
			if (bOverride_bHoldout)
			{
				AsExponentialHeightFogComponent->SetHoldout(ComponentState.bHoldout);
			}
		}
	}
}

// TODO: This really should be "DoesComponentMatchQuery()"
bool UMoviePipelineCollectionCommonQuery::DoesActorMatchQuery(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// TODO: This method should short-circuit so all query types aren't executed for every actor when the query mode is OR

	bool bMatchesActorNames = false;
	if (!ActorNames.IsEmpty())
	{
		bMatchesActorNames = ActorNames.Contains(Actor->GetName());
	}
	
	bool bMatchesComponentTypes = false;
	if (!ComponentTypes.IsEmpty())
	{
		const bool bIncludeFromChildActors = true;
		TArray<UActorComponent*> ActorComponents;
		Actor->GetComponents(ActorComponents, bIncludeFromChildActors);
		for (const UActorComponent* Component : ActorComponents)
		{
			for (const UClass* ComponentType : ComponentTypes)
			{
				if (Component->IsA(ComponentType))
				{
					bMatchesComponentTypes = true;
					break;
				}
			}
		}
	}

	bool bMatchesTags = false;
	if (!Tags.IsEmpty())
	{
		for (const FName& Tag : Tags)
		{
			if (Actor->Tags.Contains(Tag))
			{
				bMatchesTags = true;
				break;
			}
		}
	}

	const bool bUsingActorNames = !ActorNames.IsEmpty();
	const bool bUsingComponentTypes = !ComponentTypes.IsEmpty();
	const bool bUsingTags = !Tags.IsEmpty();

	if (QueryMode == EMoviePipelineCollectionCommonQueryMode::And)
	{
		return (!bUsingActorNames || bMatchesActorNames) &&
			   (!bUsingTags || bMatchesTags) &&
			   (!bUsingComponentTypes || bMatchesComponentTypes);
	}
	
	return bMatchesActorNames || bMatchesTags || bMatchesComponentTypes;
}

bool UMoviePipelineCollectionLightingQuery::DoesActorMatchQuery(const AActor* Actor) const
{
	const TArray<UClass*> LightingComponentTypes = {
		ULightComponentBase::StaticClass(),
		UReflectionCaptureComponent::StaticClass(),
		USkyAtmosphereComponent::StaticClass(),
		USkyLightComponent::StaticClass()
	};
	
	TArray<UActorComponent*> ActorComponents;
	constexpr bool bIncludeFromChildActors = true;
	Actor->GetComponents(ActorComponents, bIncludeFromChildActors);
	
	for (const UActorComponent* Component : ActorComponents)
	{
		for (const UClass* ComponentType : LightingComponentTypes)
		{
			if (Component->IsA(ComponentType))
			{
				return true;
			}
		}
	}

	return false;
}

TArray<AActor*> UMoviePipelineCollection::GetMatchingActors(const UWorld* World, const bool bInvertResult) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MRQ::Collection::GetMatchingActors);
	
	TArray<AActor*> MatchingActors;

	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		if (!Actor)
		{
			continue;
		}

		// If there aren't any queries, and the result should be inverted, just include the actor
		if (bInvertResult && Queries.IsEmpty())
		{
			MatchingActors.Add(Actor);
			continue;
		}

		for (const UMoviePipelineCollectionQuery* Query : Queries)
		{
			const bool bActorMatchesQuery = Query->DoesActorMatchQuery(Actor);
			
			if (bActorMatchesQuery && !bInvertResult)
			{
				MatchingActors.Add(Actor);
				break;
			}

			if (!bActorMatchesQuery && bInvertResult)
			{
				MatchingActors.Add(Actor);
				break;
			}
		}
	}
	
	return MatchingActors;
}

void UMoviePipelineCollectionModifier::AddCollection(UMovieGraphCollection* Collection)
{
	// Don't allow adding a duplicate collection
	for (const UMovieGraphCollection* ExistingCollection : Collections)
	{
		if (Collection && ExistingCollection && Collection->GetCollectionName().Equals(ExistingCollection->GetCollectionName()))
		{
			return;
		}
	}
	
	Collections.Add(Collection);
}

void UMoviePipelineCollection::AddQuery(UMoviePipelineCollectionQuery* Query)
{
	if (!Queries.Contains(Query))
	{
		Queries.Add(Query);
	}
}

UMovieGraphConditionGroupQueryBase::UMovieGraphConditionGroupQueryBase()
	: OpType(EMovieGraphConditionGroupQueryOpType::Add)
	, bIsEnabled(true)
{
}

void UMovieGraphConditionGroupQueryBase::SetOperationType(const EMovieGraphConditionGroupQueryOpType OperationType)
{
	// Always allow setting the operation type to Union. If not setting to Union, only allow setting the operation type if this is not the first
	// query in the condition group. The first query is always a Union.
	if (OperationType == EMovieGraphConditionGroupQueryOpType::Add)
	{
		OpType = EMovieGraphConditionGroupQueryOpType::Add;
		return;
	}

	const UMovieGraphConditionGroup* ParentConditionGroup = GetTypedOuter<UMovieGraphConditionGroup>();
	if (ensureMsgf(ParentConditionGroup, TEXT("Cannot set the operation type on a condition group query that doesn't have a condition group outer")))
	{
		if (ParentConditionGroup->GetQueries().Find(this) != 0)
		{
			OpType = OperationType;
		}
	}
}

EMovieGraphConditionGroupQueryOpType UMovieGraphConditionGroupQueryBase::GetOperationType() const
{
	return OpType;
}

void UMovieGraphConditionGroupQueryBase::Evaluate(const TArray<AActor*>& InActorsToQuery, const UWorld* InWorld, TSet<AActor*>& OutMatchingActors) const
{
	// No implementation
}

bool UMovieGraphConditionGroupQueryBase::ShouldHidePropertyNames() const
{
	// Show property names by default; subclassed queries can opt-out if they want a cleaner UI 
	return false;
}

const FSlateIcon& UMovieGraphConditionGroupQueryBase::GetIcon() const
{
	static const FSlateIcon EmptyIcon = FSlateIcon();
	return EmptyIcon;
}

const FText& UMovieGraphConditionGroupQueryBase::GetDisplayName() const
{
	static const FText DisplayName = LOCTEXT("ConditionGroupQueryDisplayName", "Query Base");
	return DisplayName;
}

#if WITH_EDITOR
TArray<TSharedRef<SWidget>> UMovieGraphConditionGroupQueryBase::GetWidgets()
{
	return TArray<TSharedRef<SWidget>>();
}

TSharedRef<SWidget> UMovieGraphConditionGroupQueryBase::GetAddMenuContents(const FMovieGraphConditionGroupQueryContentsChanged& OnAddFinished)
{
	return SNullWidget::NullWidget;
}
#endif

bool UMovieGraphConditionGroupQueryBase::IsEditorOnlyQuery() const
{
	return false;
}

void UMovieGraphConditionGroupQueryBase::SetEnabled(const bool bEnabled)
{
	bIsEnabled = bEnabled;
}

bool UMovieGraphConditionGroupQueryBase::IsEnabled() const
{
	return bIsEnabled;
}

bool UMovieGraphConditionGroupQueryBase::IsFirstConditionGroupQuery() const
{
	const UMovieGraphConditionGroup* ParentConditionGroup = GetTypedOuter<UMovieGraphConditionGroup>();
	if (ensureMsgf(ParentConditionGroup, TEXT("Cannot determine if this is the first condition group query when no parent condition group is present")))
	{
		// GetQueries() returns an array of non-const pointers, so Find() doesn't like having a const pointer passed to it.
		// Find() won't mutate the condition group query though, so the const_cast here is OK.
		return ParentConditionGroup->GetQueries().Find(const_cast<UMovieGraphConditionGroupQueryBase*>(this)) == 0;
	}
	
	return false;
}

void UMovieGraphConditionGroupQuery_Actor::Evaluate(const TArray<AActor*>& InActorsToQuery, const UWorld* InWorld, TSet<AActor*>& OutMatchingActors) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroupQuery_Actor::Evaluate);

#if WITH_EDITOR
	const bool bIsPIE = GEditor->IsPlaySessionInProgress();
#else
	const bool bIsPIE = false;
#endif

	// Convert the actors in the query to PIE (or editor) equivalents once, rather than constantly in the loop.
	TArray<AActor*> ActorsToMatch_Converted;
	ActorsToMatch_Converted.Reserve(ActorsToMatch.Num());
	for (const TSoftObjectPtr<AActor>& SoftActorToMatch : ActorsToMatch)
	{
		if (!SoftActorToMatch.IsValid())
		{
			continue;
		}
		
		AActor* ActorToMatch = SoftActorToMatch.Get();
		const UWorld* ActorWorld = ActorToMatch->GetWorld();
		const bool bIsEditorActor = ActorWorld && ActorWorld->IsEditorWorld();

		// If a PIE session is NOT in progress, make sure that the actor is the editor equivalent
		if (!bIsPIE)
		{
			// Only do PIE -> editor actor conversion when the actor is NOT from the editor
			if (!bIsEditorActor)
			{
#if WITH_EDITOR
				if (AActor* EditorActor = EditorUtilities::GetEditorWorldCounterpartActor(ActorToMatch))
				{
					ActorsToMatch_Converted.Add(EditorActor);
				}
#endif
			}
			else
			{
				// Just use ActorToMatch as-is if it's not from PIE
				ActorsToMatch_Converted.Add(ActorToMatch);
			}
		}

		// If a PIE session IS active, try to get the PIE equivalent of the editor actor
		else
		{
			// Only do editor -> PIE actor conversion when the actor is from an editor world
			if (bIsEditorActor)
			{
#if WITH_EDITOR
				if (AActor* PieActor = EditorUtilities::GetSimWorldCounterpartActor(ActorToMatch))
				{
					ActorsToMatch_Converted.Add(PieActor);
				}
#endif
			}
			else
			{
				// Just use ActorToMatch as-is if it's not from an editor actor
				ActorsToMatch_Converted.Add(ActorToMatch);
			}	
		}
	}
	
	for (AActor* Actor : InActorsToQuery)
	{
		if (ActorsToMatch_Converted.Contains(Actor))
		{
			OutMatchingActors.Add(Actor);
		}
	}
}

const FSlateIcon& UMovieGraphConditionGroupQuery_Actor::GetIcon() const
{
	static const FSlateIcon ActorIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Actor");
	return ActorIcon;
}

const FText& UMovieGraphConditionGroupQuery_Actor::GetDisplayName() const
{
	static const FText DisplayName = LOCTEXT("ConditionGroupQueryDisplayName_Actor", "Actor");
	return DisplayName;
}

#if WITH_EDITOR
TArray<TSharedRef<SWidget>> UMovieGraphConditionGroupQuery_Actor::GetWidgets()
{
	TArray<TSharedRef<SWidget>> Widgets;

	// Create the data source for the list view
	ListDataSource.Empty();
	for (TSoftObjectPtr<AActor>& Actor : ActorsToMatch)
	{
		ListDataSource.Add(MakeShared<TSoftObjectPtr<AActor>>(Actor));
	}

	Widgets.Add(
		SAssignNew(ActorsList, SMovieGraphSimpleList<TSharedPtr<TSoftObjectPtr<AActor>>>)
			.DataSource(&ListDataSource)
			.DataType(FText::FromString("Actor"))
			.DataTypePlural(FText::FromString("Actors"))
			.OnGetRowText_Static(&GetRowText)
			.OnGetRowIcon_Static(&GetRowIcon)
			.OnDelete_Lambda([this](const TSharedPtr<TSoftObjectPtr<AActor>> InActor)
			{
				ListDataSource.Remove(InActor);
				ActorsToMatch.Remove(*InActor.Get());
				ActorsList->Refresh();
			})
	);

	return Widgets;
}

TSharedRef<SWidget> UMovieGraphConditionGroupQuery_Actor::GetAddMenuContents(const FMovieGraphConditionGroupQueryContentsChanged& OnAddFinished)
{	
	FSceneOutlinerInitializationOptions SceneOutlinerInitOptions;
	SceneOutlinerInitOptions.bShowHeaderRow = true;
	SceneOutlinerInitOptions.bShowSearchBox = true;
	SceneOutlinerInitOptions.bShowCreateNewFolder = false;
	SceneOutlinerInitOptions.bFocusSearchBoxWhenOpened = true;

	// Show the name/label column and the type column
	SceneOutlinerInitOptions.ColumnMap.Add(
		FSceneOutlinerBuiltInColumnTypes::Label(),
		FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0, FCreateSceneOutlinerColumn(), false, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Label_Localized()));
	SceneOutlinerInitOptions.ColumnMap.Add(
		FSceneOutlinerBuiltInColumnTypes::ActorInfo(),
		FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10,  FCreateSceneOutlinerColumn(), false, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::ActorInfo_Localized()));

	// Don't show actors which have already been picked
	SceneOutlinerInitOptions.Filters->AddFilterPredicate<FActorTreeItem>(
		FActorTreeItem::FFilterPredicate::CreateLambda([this](const AActor* InActor)
		{
			return !ActorsToMatch.Contains(InActor);
		}));
	
	const FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");

	FMenuBuilder MenuBuilder(false, MakeShared<FUICommandList>());

	auto AddActorToList = [this](const AActor* InActor, const FMovieGraphConditionGroupQueryContentsChanged& OnAddFinished)
	{
		ActorsToMatch.Add(InActor);
		ListDataSource.Add(MakeShared<TSoftObjectPtr<AActor>>(ActorsToMatch.Last()));
		OnAddFinished.ExecuteIfBound();
	};
	
	auto RefreshActorPickerFilterAndList = [this]()
	{
		// Ensure that the actor picker filter runs again so duplicate actors cannot be selected
		if (ActorPickerWidget.IsValid())
		{
			ActorPickerWidget->FullRefresh();
			ActorsList->Refresh();
		}
	};
	
	MenuBuilder.BeginSection("AddActor", LOCTEXT("AddActor", "Add Actor"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddSelectedInOutliner", "Add Selected In Outliner"),
			LOCTEXT("AddSelectedInOutlinerTooltip", "Add actors currently selected in the level editor's scene outliner."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(),"FoliageEditMode.SetSelect"),
			FUIAction(
				FExecuteAction::CreateLambda([this, OnAddFinished, RefreshActorPickerFilterAndList, AddActorToList]()
				{
					USelection* CurrentSelection = GEditor->GetSelectedActors();
					TArray<AActor*> SelectedActors;
					CurrentSelection->GetSelectedObjects<AActor>(SelectedActors);
					for (const AActor* Actor : SelectedActors)
					{
						if (Actor && !ActorsToMatch.Contains(Actor))
						{
							AddActorToList(Actor, OnAddFinished);
						}
					}

					RefreshActorPickerFilterAndList();

					FSlateApplication::Get().DismissAllMenus();
				}
				),
				FCanExecuteAction::CreateLambda([]()
				{
					return GEditor->GetSelectedActors()->Num() > 0;
					
				})
			)
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Browse", LOCTEXT("Browse", "Browse"));
	{
		ActorPickerWidget = SceneOutlinerModule.CreateActorPicker(
			SceneOutlinerInitOptions,
			FOnActorPicked::CreateLambda([this, OnAddFinished, RefreshActorPickerFilterAndList, AddActorToList](AActor* InActor)
			{
				AddActorToList(InActor, OnAddFinished);

				RefreshActorPickerFilterAndList();
			}));

		const TSharedRef<SBox> ActorPickerWidgetBox =
			SNew(SBox)
			.WidthOverride(400.f)
			.HeightOverride(300.f)
			[
				ActorPickerWidget.ToSharedRef()
			];

		MenuBuilder.AddWidget(ActorPickerWidgetBox, FText());
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

const FSlateBrush* UMovieGraphConditionGroupQuery_Actor::GetRowIcon(TSharedPtr<TSoftObjectPtr<AActor>> InActor)
{
	if (InActor.IsValid())
	{
		if (InActor.Get()->IsValid())
		{
			// The first Get() returns the TSoftObjectPtr, the second Get() dereferences the TSoftObjectPtr
			return FSlateIconFinder::FindIconForClass(InActor.Get()->Get()->GetClass()).GetIcon();
		}
	}

	return FSlateIconFinder::FindIconForClass(AActor::StaticClass()).GetIcon();
}

FText UMovieGraphConditionGroupQuery_Actor::GetRowText(TSharedPtr<TSoftObjectPtr<AActor>> InActor)
{
	if (InActor.IsValid())
	{
		if (InActor.Get()->IsValid())
		{
			// The first Get() returns the TSoftObjectPtr, the second Get() dereferences the TSoftObjectPtr
			return FText::FromString(InActor.Get()->Get()->GetActorLabel());
		}
	}

	return LOCTEXT("MovieGraphActorConditionGroupQuery_InvalidActor", "(invalid)");
}
#endif	// WITH_EDITOR

void UMovieGraphConditionGroupQuery_ActorTagName::Evaluate(const TArray<AActor*>& InActorsToQuery, const UWorld* InWorld, TSet<AActor*>& OutMatchingActors) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroupQuery_ActorTag::Evaluate);
	
	// Quick early-out if "*" is used as the wildcard. Faster than doing the wildcard matching.
	if (TagsToMatch == TEXT("*"))
	{
		OutMatchingActors.Append(InActorsToQuery);
		return;
	}

	// Actor tags can be specified on multiple lines
	TArray<FString> AllTagNameStrings;
	TagsToMatch.ParseIntoArrayLines(AllTagNameStrings);

	for (AActor* Actor : InActorsToQuery)
	{
		for (const FString& TagToMatch : AllTagNameStrings)
		{
			bool bMatchedTag = false;
			
			for (const FName& ActorTag : Actor->Tags)
			{
				if (ActorTag.ToString().MatchesWildcard(TagToMatch))
				{
					OutMatchingActors.Add(Actor);
					bMatchedTag = true;
					break;
				}
			}

			// Skip comparing the rest of the tags if one tag already matched 
			if (bMatchedTag)
			{
				break;
			}
		}
	}
}

const FSlateIcon& UMovieGraphConditionGroupQuery_ActorTagName::GetIcon() const
{
	// TODO: This icon is wrong
	static const FSlateIcon ActorTagIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Debug");
	return ActorTagIcon;
}

const FText& UMovieGraphConditionGroupQuery_ActorTagName::GetDisplayName() const
{
	static const FText DisplayName = LOCTEXT("ConditionGroupQueryDisplayName_ActorTagName", "Actor Tag Name");
	return DisplayName;
}

#if WITH_EDITOR
TArray<TSharedRef<SWidget>> UMovieGraphConditionGroupQuery_ActorTagName::GetWidgets()
{
	TArray<TSharedRef<SWidget>> Widgets;

	Widgets.Add(
		SNew(SBox)
		.HAlign(HAlign_Fill)
		.Padding(7.f, 2.f)
		[
			SNew(SMultiLineEditableTextBox)
			.Text_Lambda([this]() { return FText::FromString(TagsToMatch); })
			.OnTextChanged_Lambda([this](const FText& InText) { TagsToMatch = InText.ToString(); })
			.HintText(LOCTEXT("MovieGraphActorTagNameQueryHintText", "The actor must match one or more tags. Wildcards allowed.\nEnter each tag on a separate line."))
		]
	);

	return Widgets;
}
#endif

void UMovieGraphConditionGroupQuery_ActorName::Evaluate(const TArray<AActor*>& InActorsToQuery, const UWorld* InWorld, TSet<AActor*>& OutMatchingActors) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroupQuery_ActorName::Evaluate);
	
	// Quick early-out if "*" is used as the wildcard. Faster than doing the wildcard matching.
	if (WildcardSearch == TEXT("*"))
	{
		OutMatchingActors.Append(InActorsToQuery);
		return;
	}

	// Actor names can be specified on multiple lines
	TArray<FString> AllActorNames;
	WildcardSearch.ParseIntoArrayLines(AllActorNames);

	for (AActor* Actor : InActorsToQuery)
	{
#if WITH_EDITOR
		for (const FString& ActorName : AllActorNames)
		{
			if (Actor->GetActorLabel().MatchesWildcard(ActorName))
			{
				OutMatchingActors.Add(Actor);
			}
		}
#endif
	}
}

const FSlateIcon& UMovieGraphConditionGroupQuery_ActorName::GetIcon() const
{
	static const FSlateIcon ActorTagIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.TextRenderActor");
	return ActorTagIcon;
}

const FText& UMovieGraphConditionGroupQuery_ActorName::GetDisplayName() const
{
	static const FText DisplayName = LOCTEXT("ConditionGroupQueryDisplayName_ActorName", "Actor Name");
	return DisplayName;
}

#if WITH_EDITOR
TArray<TSharedRef<SWidget>> UMovieGraphConditionGroupQuery_ActorName::GetWidgets()
{
	TArray<TSharedRef<SWidget>> Widgets;

	Widgets.Add(
		SNew(SBox)
		.HAlign(HAlign_Fill)
		.Padding(7.f, 2.f)
		[
			SNew(SMultiLineEditableTextBox)
			.Text_Lambda([this]() { return FText::FromString(WildcardSearch); })
			.OnTextChanged_Lambda([this](const FText& InText) { WildcardSearch = InText.ToString(); })
			.HintText(LOCTEXT("MovieGraphActorNameQueryHintText", "Actor names to query. Wildcards allowed.\nEnter each actor name on a separate line."))
		]
	);

	return Widgets;
}
#endif

bool UMovieGraphConditionGroupQuery_ActorName::IsEditorOnly() const
{
	// GetActorLabel() is editor-only
	return true;
}

void UMovieGraphConditionGroupQuery_ActorType::Evaluate(const TArray<AActor*>& InActorsToQuery, const UWorld* InWorld, TSet<AActor*>& OutMatchingActors) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroupQuery_ActorType::Evaluate);
	
	for (AActor* Actor : InActorsToQuery)
	{
		if (ActorTypes.Contains(Actor->GetClass()))
		{
			OutMatchingActors.Add(Actor);
		}
	}
}

const FSlateIcon& UMovieGraphConditionGroupQuery_ActorType::GetIcon() const
{
	static const FSlateIcon ActorTagIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.ActorComponent");
	return ActorTagIcon;
}

const FText& UMovieGraphConditionGroupQuery_ActorType::GetDisplayName() const
{
	static const FText DisplayName = LOCTEXT("ConditionGroupQueryDisplayName_ActorType", "Actor Type");
	return DisplayName;
}

#if WITH_EDITOR
TArray<TSharedRef<SWidget>> UMovieGraphConditionGroupQuery_ActorType::GetWidgets()
{
	TArray<TSharedRef<SWidget>> Widgets;

	Widgets.Add(
		SAssignNew(ActorTypesList, SMovieGraphSimpleList<UClass*>)
			.DataSource(&ActorTypes)
			.DataType(FText::FromString("Actor Type"))
			.DataTypePlural(FText::FromString("Actor Types"))
			.OnGetRowText_Static(&GetRowText)
			.OnGetRowIcon_Static(&GetRowIcon)
			.OnDelete_Lambda([this](UClass* InActorClass)
			{
				ActorTypes.Remove(InActorClass);
				ActorTypesList->Refresh();
			})
	);

	return Widgets;
}

TSharedRef<SWidget> UMovieGraphConditionGroupQuery_ActorType::GetAddMenuContents(const FMovieGraphConditionGroupQueryContentsChanged& OnAddFinished)
{
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.bShowNoneOption = false;
	Options.bIsActorsOnly = true;
	Options.bShowUnloadedBlueprints = false;

	// Add a class filter to disallow adding duplicates of actor types that were already picked
	Options.ClassFilters.Add(MakeShared<UE::MovieGraph::Private::FClassViewerTypeFilter>(&ActorTypes));

	const TSharedRef<SWidget> ClassViewer = ClassViewerModule.CreateClassViewer(
		Options,
		FOnClassPicked::CreateLambda([this, OnAddFinished](UClass* InNewClass)
		{
			FSlateApplication::Get().DismissAllMenus();
			
			ActorTypes.Add(InNewClass);
			OnAddFinished.ExecuteIfBound();

			// Ensure that the class filters run again so duplicate actor types cannot be selected
			if (ClassViewerWidget.IsValid())
			{
				ClassViewerWidget->Refresh();
				ActorTypesList->Refresh();
			}
		}));

	ClassViewerWidget = StaticCastSharedPtr<SClassViewer>(ClassViewer.ToSharedPtr()); 
	
	return SNew(SBox)
		.WidthOverride(300.f)
		.HeightOverride(300.f)
		[
			ClassViewerWidget.ToSharedRef()
		];
}

const FSlateBrush* UMovieGraphConditionGroupQuery_ActorType::GetRowIcon(UClass* InActorType)
{
	return FSlateIconFinder::FindIconForClass(InActorType).GetIcon();
}

FText UMovieGraphConditionGroupQuery_ActorType::GetRowText(UClass* InActorType)
{
	if (InActorType)
	{
		return InActorType->GetDisplayNameText();
	}

	return LOCTEXT("MovieGraphActorTypeConditionGroupQuery_Invalid", "(invalid)");
}
#endif

void UMovieGraphConditionGroupQuery_ComponentTagName::Evaluate(const TArray<AActor*>& InActorsToQuery, const UWorld* InWorld, TSet<AActor*>& OutMatchingActors) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroupQuery_ComponentTag::Evaluate);
	
	// Quick early-out if "*" is used as the wildcard. Faster than doing the wildcard matching.
	if (TagsToMatch == TEXT("*"))
	{
		OutMatchingActors.Append(InActorsToQuery);
		return;
	}

	// Component tags can be specified on multiple lines
	TArray<FString> AllTagNameStrings;
	TagsToMatch.ParseIntoArrayLines(AllTagNameStrings);
	
	TInlineComponentArray<UActorComponent*> ActorComponents;

	for (AActor* Actor : InActorsToQuery)
	{
		// Include child components so components inside of Blueprints can be found
		constexpr bool bIncludeFromChildActors = false;
		Actor->GetComponents<UActorComponent*>(ActorComponents, bIncludeFromChildActors);
		
		for (const UActorComponent* Component : ActorComponents)
		{
			bool bMatchedTag = false;
			
			for (const FString& TagToMatch : AllTagNameStrings)
			{			
				for (const FName& ComponentTag : Component->ComponentTags)
				{
					if (ComponentTag.ToString().MatchesWildcard(TagToMatch))
					{
						OutMatchingActors.Add(Actor);
						bMatchedTag = true;
						break;
					}
				}

				// Skip comparing the rest of the tags if one tag already matched 
				if (bMatchedTag)
				{
					break;
				}
			}

			// Skip comparing the rest of the components if one component already matched
			if (bMatchedTag)
			{
				break;
			}
		}

		ActorComponents.Reset();
	}
}

const FSlateIcon& UMovieGraphConditionGroupQuery_ComponentTagName::GetIcon() const
{
	// TODO: This icon is wrong
	static const FSlateIcon ActorTagIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset");
	return ActorTagIcon;
}

const FText& UMovieGraphConditionGroupQuery_ComponentTagName::GetDisplayName() const
{
	static const FText DisplayName = LOCTEXT("ConditionGroupQueryDisplayName_ComponentTagName", "Component Tag Name");
	return DisplayName;
}

#if WITH_EDITOR
TArray<TSharedRef<SWidget>> UMovieGraphConditionGroupQuery_ComponentTagName::GetWidgets()
{
	TArray<TSharedRef<SWidget>> Widgets;

	Widgets.Add(
		SNew(SBox)
		.HAlign(HAlign_Fill)
		.Padding(7.f, 2.f)
		[
			SNew(SMultiLineEditableTextBox)
			.Text_Lambda([this]() { return FText::FromString(TagsToMatch); })
			.OnTextChanged_Lambda([this](const FText& InText) { TagsToMatch = InText.ToString(); })
			.HintText(LOCTEXT("MovieGraphComponentTagNameQueryHintText", "A component on the actor must match one or more component tags.\nWildcards allowed. Enter each tag on a separate line."))
		]
	);

	return Widgets;
}
#endif

void UMovieGraphConditionGroupQuery_ComponentType::Evaluate(const TArray<AActor*>& InActorsToQuery, const UWorld* InWorld, TSet<AActor*>& OutMatchingActors) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroupQuery_ComponentType::Evaluate);
	
	TInlineComponentArray<UActorComponent*> ActorComponents;

	for (AActor* Actor : InActorsToQuery)
	{
		// Include child components so components inside of Blueprints can be found
		constexpr bool bIncludeFromChildActors = false;
		Actor->GetComponents<UActorComponent*>(ActorComponents, bIncludeFromChildActors);
		
		for (const UActorComponent* Component : ActorComponents)
		{
			if (ComponentTypes.Contains(Component->GetClass()))
			{
				OutMatchingActors.Add(Actor);
			}
		}

		ActorComponents.Reset();
	}
}

const FSlateIcon& UMovieGraphConditionGroupQuery_ComponentType::GetIcon() const
{
	static const FSlateIcon ActorTagIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.ActorComponent");
	return ActorTagIcon;
}

const FText& UMovieGraphConditionGroupQuery_ComponentType::GetDisplayName() const
{
	static const FText DisplayName = LOCTEXT("ConditionGroupQueryDisplayName_ComponentType", "Component Type");
	return DisplayName;
}

#if WITH_EDITOR
TArray<TSharedRef<SWidget>> UMovieGraphConditionGroupQuery_ComponentType::GetWidgets()
{
	TArray<TSharedRef<SWidget>> Widgets;

	Widgets.Add(
		SAssignNew(ComponentTypesList, SMovieGraphSimpleList<UClass*>)
			.DataSource(&ComponentTypes)
			.DataType(FText::FromString("Component Type"))
			.DataTypePlural(FText::FromString("Component Types"))
			.OnGetRowText_Static(&GetRowText)
			.OnGetRowIcon_Static(&GetRowIcon)
			.OnDelete_Lambda([this](UClass* InComponentType)
			{
				ComponentTypes.Remove(InComponentType);
				ComponentTypesList->Refresh();
			})			
	);

	return Widgets;
}

TSharedRef<SWidget> UMovieGraphConditionGroupQuery_ComponentType::GetAddMenuContents(const FMovieGraphConditionGroupQueryContentsChanged& OnAddFinished)
{
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.bShowNoneOption = false;
	Options.bIsActorsOnly = false;
	Options.bShowUnloadedBlueprints = false;

	// Add a class filter to disallow adding duplicates of component types that were already picked, as well as restrict the types of classes displayed
	// to only show component classes
	Options.ClassFilters.Add(MakeShared<UE::MovieGraph::Private::FClassViewerTypeFilter>(&ComponentTypes, UActorComponent::StaticClass()));

	const TSharedRef<SWidget> ClassViewer = ClassViewerModule.CreateClassViewer(
		Options,
		FOnClassPicked::CreateLambda([this, OnAddFinished](UClass* InNewClass)
		{
			FSlateApplication::Get().DismissAllMenus();
			
			ComponentTypes.Add(InNewClass);
			OnAddFinished.ExecuteIfBound();

			// Ensure that the class filters run again so duplicate actor types cannot be selected
			if (ClassViewerWidget.IsValid())
			{
				ClassViewerWidget->Refresh();
				ComponentTypesList->Refresh();
			}
		}));

	ClassViewerWidget = StaticCastSharedPtr<SClassViewer>(ClassViewer.ToSharedPtr()); 
	
	return SNew(SBox)
		.WidthOverride(300.f)
		.HeightOverride(300.f)
		[
			ClassViewerWidget.ToSharedRef()
		];
}

const FSlateBrush* UMovieGraphConditionGroupQuery_ComponentType::GetRowIcon(UClass* InComponentType)
{
	return FSlateIconFinder::FindIconForClass(InComponentType).GetIcon();
}

FText UMovieGraphConditionGroupQuery_ComponentType::GetRowText(UClass* InComponentType)
{
	if (InComponentType)
	{
		return InComponentType->GetDisplayNameText();
	}
	
	return LOCTEXT("MovieGraphComponentTypeConditionGroupQuery_Invalid", "(invalid)");
}
#endif	// WITH_EDITOR

void UMovieGraphConditionGroupQuery_EditorFolder::Evaluate(const TArray<AActor*>& InActorsToQuery, const UWorld* InWorld, TSet<AActor*>& OutMatchingActors) const
{
#if WITH_EDITOR
	// This const cast is unfortunate, but should be harmless
	TArray<AActor*> ActorsInFolders;
	FActorFolders::GetActorsFromFolders(*const_cast<UWorld*>(InWorld), FolderPaths, ActorsInFolders);
	
	OutMatchingActors.Append(ActorsInFolders);
#endif	// WITH_EDITOR
}

const FSlateIcon& UMovieGraphConditionGroupQuery_EditorFolder::GetIcon() const
{
	static const FSlateIcon EditorFolderIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen");
	return EditorFolderIcon;
}

const FText& UMovieGraphConditionGroupQuery_EditorFolder::GetDisplayName() const
{
	static const FText DisplayName = LOCTEXT("ConditionGroupQueryDisplayName_EditorFolder", "Editor Folder");
	return DisplayName;
}

bool UMovieGraphConditionGroupQuery_EditorFolder::IsEditorOnlyQuery() const
{
	// This query is editor-only because folders do not exist outside of the editor
	return true;
}

#if WITH_EDITOR
TArray<TSharedRef<SWidget>> UMovieGraphConditionGroupQuery_EditorFolder::GetWidgets()
{
	TArray<TSharedRef<SWidget>> Widgets;
    
    Widgets.Add(
    	SAssignNew(FolderPathsList, SMovieGraphSimpleList<FName>)
    		.DataSource(&FolderPaths)
    		.DataType(FText::FromString("Folder"))
    		.DataTypePlural(FText::FromString("Folders"))
    		.OnGetRowText_Static(&GetRowText)
    		.OnGetRowIcon_Static(&GetRowIcon)
    		.OnDelete_Lambda([this](FName InFolderPath)
    		{
    			FolderPaths.Remove(InFolderPath);
    			FolderPathsList->Refresh();
    			FolderPickerWidget->FullRefresh();
    		})			
    );

    return Widgets;
}

TSharedRef<SWidget> UMovieGraphConditionGroupQuery_EditorFolder::GetAddMenuContents(const FMovieGraphConditionGroupQueryContentsChanged& OnAddFinished)
{
	auto OnItemPicked = FOnSceneOutlinerItemPicked::CreateLambda([this, OnAddFinished](TSharedRef<ISceneOutlinerTreeItem> Item)
	{
		if (const FActorFolderTreeItem* FolderItem = Item->CastTo<FActorFolderTreeItem>())
		{
			if (FolderItem->IsValid())
			{
				const FName& FolderPath = FolderItem->GetPath();
					
				// Don't allow duplicate folder paths
				if (FolderPaths.Contains(FolderPath))
				{
					return;
				}
				
				FolderPaths.AddUnique(FolderPath);
				OnAddFinished.ExecuteIfBound();
				
				if (FolderPickerWidget.IsValid())
				{
					FolderPickerWidget->FullRefresh();
					FolderPathsList->Refresh();
				}
			}
		}
	});

	const FCreateSceneOutlinerMode ModeFactory = FCreateSceneOutlinerMode::CreateLambda([&OnItemPicked](SSceneOutliner* Outliner)
	{
		return new FActorFolderPickingMode(Outliner, OnItemPicked);
	});
	
	FSceneOutlinerInitializationOptions SceneOutlinerInitOptions;
	SceneOutlinerInitOptions.bShowCreateNewFolder = false;
	SceneOutlinerInitOptions.bFocusSearchBoxWhenOpened = true;
	SceneOutlinerInitOptions.ModeFactory = ModeFactory;

	// Don't show folders which have already been picked
	SceneOutlinerInitOptions.Filters->AddFilterPredicate<FActorFolderTreeItem>(
		FActorFolderTreeItem::FFilterPredicate::CreateLambda([this](const FFolder& InFolder)
		{
			return !FolderPaths.Contains(InFolder.GetPath());
		}));

	// Only show the name/label column, that's the only column relevant to folders
	SceneOutlinerInitOptions.ColumnMap.Add(
		FSceneOutlinerBuiltInColumnTypes::Label(),
		FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0, FCreateSceneOutlinerColumn(), false, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Label_Localized()));

	FolderPickerWidget = SNew(SSceneOutliner, SceneOutlinerInitOptions)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
	
	return
		SNew(SBox)
		.WidthOverride(400.f)
		.HeightOverride(300.f)
		[
			FolderPickerWidget.ToSharedRef()
		];
}

const FSlateBrush* UMovieGraphConditionGroupQuery_EditorFolder::GetRowIcon(FName InFolderPath)
{
	return FAppStyle::Get().GetBrush("Icons.FolderOpen");
}

FText UMovieGraphConditionGroupQuery_EditorFolder::GetRowText(FName InFolderPath)
{
	return FText::FromString(InFolderPath.ToString());
}
#endif	// WITH_EDITOR

void UMovieGraphConditionGroupQuery_Sublevel::Evaluate(const TArray<AActor*>& InActorsToQuery, const UWorld* InWorld, TSet<AActor*>& OutMatchingActors) const
{
	for (const TSoftObjectPtr<UWorld>& World : Sublevels)
	{
		// Don't load the level, only use levels which are already loaded
		const UWorld* LoadedWorld = World.Get();
		if (!LoadedWorld)
		{
			const UMovieGraphCollection* ParentCollection = GetTypedOuter<UMovieGraphCollection>();
			const FString CollectionName = ParentCollection ? ParentCollection->GetCollectionName() : TEXT("<unknown>");
			
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Sublevel query in collection '%s' is excluding level (%s) because it is not loaded."), *CollectionName, *World.ToString())
			continue;
		}

		ULevel* CurrentLevel = LoadedWorld->GetCurrentLevel();
		if (!CurrentLevel)
		{
			continue;
		}

		for (TObjectPtr<AActor>& LevelActor : CurrentLevel->Actors)
		{
			OutMatchingActors.Add(LevelActor.Get());
		}
	}
}

const FSlateIcon& UMovieGraphConditionGroupQuery_Sublevel::GetIcon() const
{
	static const FSlateIcon SublevelIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Level");
	return SublevelIcon;
}

const FText& UMovieGraphConditionGroupQuery_Sublevel::GetDisplayName() const
{
	static const FText DisplayName = LOCTEXT("ConditionGroupQueryDisplayName_Sublevel", "Sublevel");
	return DisplayName;
}

#if WITH_EDITOR
TArray<TSharedRef<SWidget>> UMovieGraphConditionGroupQuery_Sublevel::GetWidgets()
{
	TArray<TSharedRef<SWidget>> Widgets;

	// Create the data source for the list view
	ListDataSource.Empty();
	for (TSoftObjectPtr<UWorld>& Sublevel : Sublevels)
	{
		ListDataSource.Add(MakeShared<TSoftObjectPtr<UWorld>>(Sublevel));
	}

	Widgets.Add(
		SAssignNew(SublevelsList, SMovieGraphSimpleList<TSharedPtr<TSoftObjectPtr<UWorld>>>)
			.DataSource(&ListDataSource)
			.DataType(FText::FromString("Sublevel"))
			.DataTypePlural(FText::FromString("Sublevels"))
			.OnGetRowText_Static(&GetRowText)
			.OnGetRowIcon_Static(&GetRowIcon)
			.OnDelete_Lambda([this](const TSharedPtr<TSoftObjectPtr<UWorld>> InSublevel)
			{
				ListDataSource.Remove(InSublevel);
				Sublevels.Remove(*InSublevel.Get());
				
				SublevelsList->Refresh();
				
				constexpr bool bUpdateSources = true;
				RefreshLevelPicker.ExecuteIfBound(bUpdateSources);
			})
	);

	return Widgets;
}

TSharedRef<SWidget> UMovieGraphConditionGroupQuery_Sublevel::GetAddMenuContents(const FMovieGraphConditionGroupQueryContentsChanged& OnAddFinished)
{
	FAssetPickerConfig SublevelPickerConfig;
	{
		SublevelPickerConfig.SelectionMode = ESelectionMode::Single;
		SublevelPickerConfig.SaveSettingsName = TEXT("MovieRenderGraphSublevelPicker");
		SublevelPickerConfig.RefreshAssetViewDelegates.Add(&RefreshLevelPicker);
		SublevelPickerConfig.InitialAssetViewType = EAssetViewType::Column;
		SublevelPickerConfig.bFocusSearchBoxWhenOpened = true;
		SublevelPickerConfig.bAllowNullSelection = false;
		SublevelPickerConfig.bShowBottomToolbar = true;
		SublevelPickerConfig.bAutohideSearchBar = false;
		SublevelPickerConfig.bAllowDragging = false;
		SublevelPickerConfig.bCanShowClasses = false;
		SublevelPickerConfig.bShowPathInColumnView = true;
		SublevelPickerConfig.bShowTypeInColumnView = false;
		SublevelPickerConfig.bSortByPathInColumnView = false;
		SublevelPickerConfig.HiddenColumnNames = {
			ContentBrowserItemAttributes::ItemDiskSize.ToString(),
			ContentBrowserItemAttributes::VirtualizedData.ToString(),
			TEXT("PrimaryAssetType"),
			TEXT("PrimaryAssetName")
		};
		SublevelPickerConfig.AssetShowWarningText = LOCTEXT("ConditionGroupQuery_NoSublevelsFound", "No Sublevels Found");
		SublevelPickerConfig.Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
		SublevelPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([this, OnAddFinished](const FAssetData& InLevelAsset)
		{
			FSlateApplication::Get().DismissAllMenus();
			
			Sublevels.AddUnique(InLevelAsset.GetAsset());
			ListDataSource.AddUnique(MakeShared<TSoftObjectPtr<UWorld>>(InLevelAsset.GetAsset()));
			OnAddFinished.ExecuteIfBound();

			if (SublevelsList.IsValid())
			{
				SublevelsList->Refresh();
				
				constexpr bool bUpdateSources = false;
				RefreshLevelPicker.ExecuteIfBound(bUpdateSources);
			}
		});
		SublevelPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda([this](const FAssetData& InLevelAsset)
		{
			// Don't show sublevels which have already been picked
			UWorld* Sublevel = Cast<UWorld>(InLevelAsset.GetAsset());
			return !Sublevel || Sublevels.Contains(Sublevel);
		});
	}

	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

	return
		SNew(SBox)
		.Padding(0, 10.f, 0, 0)
		.WidthOverride(400.f)
		.HeightOverride(300.f)
		[
			ContentBrowser.CreateAssetPicker(SublevelPickerConfig)
		];
}

const FSlateBrush* UMovieGraphConditionGroupQuery_Sublevel::GetRowIcon(TSharedPtr<TSoftObjectPtr<UWorld>> InSublevel)
{
	return FAppStyle::Get().GetBrush("Icons.Level");
}

FText UMovieGraphConditionGroupQuery_Sublevel::GetRowText(TSharedPtr<TSoftObjectPtr<UWorld>> InSublevel)
{
	if (InSublevel.IsValid())
	{
		if (InSublevel.Get()->IsValid())
		{
			// The first Get() returns the TSoftObjectPtr, the second Get() dereferences the TSoftObjectPtr
			return FText::FromString(InSublevel.Get()->Get()->GetName());
		}
	}

	return LOCTEXT("MovieGraphSublevelConditionGroupQuery_InvalidLevel", "(invalid)");
}
#endif	// WITH_EDITOR

UMovieGraphConditionGroup::UMovieGraphConditionGroup()
	: OpType(EMovieGraphConditionGroupOpType::Add)
{
	// The CDO will always have the default GUID
	if (!HasAllFlags(RF_ClassDefaultObject))
	{
		Id = FGuid::NewGuid();
	}
	else
	{
		Id = FGuid();
	}
}

void UMovieGraphConditionGroup::SetOperationType(const EMovieGraphConditionGroupOpType OperationType)
{
	// Always allow setting the operation type to Union. If not setting to Union, only allow setting the operation type if this is not the first
	// condition group in the collection. The first condition group is always a Union.
	if (OperationType == EMovieGraphConditionGroupOpType::Add)
	{
		OpType = EMovieGraphConditionGroupOpType::Add;
		return;
	}

	const UMovieGraphCollection* ParentCollection = GetTypedOuter<UMovieGraphCollection>();
	if (ensureMsgf(ParentCollection, TEXT("Cannot set the operation type on a condition group that doesn't have a collection outer")))
	{
		if (ParentCollection->GetConditionGroups().Find(this) != 0)
		{
			OpType = OperationType;
		}
	}
}

EMovieGraphConditionGroupOpType UMovieGraphConditionGroup::GetOperationType() const
{
	return OpType;
}

TSet<AActor*> UMovieGraphConditionGroup::Evaluate(const UWorld* InWorld) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroup::Evaluate);

	// Reset the TSet for evaluation results; it is persisted across frames to prevent constantly re-allocating it
	EvaluationResult.Reset();

	// Generate a list of actors that can be fed to the queries once, rather than having all queries perform this
	TArray<AActor*> AllActors;
	for (TActorIterator<AActor> ActorItr(InWorld); ActorItr; ++ActorItr)
	{
		if (AActor* Actor = *ActorItr)
		{
			AllActors.Add(Actor);
		}
	}

	for (int32 QueryIndex = 0; QueryIndex < Queries.Num(); ++QueryIndex)
	{
		const TObjectPtr<UMovieGraphConditionGroupQueryBase>& Query = Queries[QueryIndex];
		if (!Query || !Query->IsEnabled())
		{
			continue;
		}
		
		if (QueryIndex == 0)
		{
			// The first query should always be a Union
			ensure(Query->GetOperationType() == EMovieGraphConditionGroupQueryOpType::Add);
		}

		// Similar to EvaluationResult, QueryResult is persisted+reset to prevent constantly re-allocating it
		QueryResult.Reset();

		Query->Evaluate(AllActors, InWorld, QueryResult);
		
		switch (Query->GetOperationType())
		{
		case EMovieGraphConditionGroupQueryOpType::Add:
			// Append() is faster than Union() because we don't need to allocate a new set
			EvaluationResult.Append(QueryResult);
			break;

		case EMovieGraphConditionGroupQueryOpType::And:
			EvaluationResult = EvaluationResult.Intersect(QueryResult);
			break;

		case EMovieGraphConditionGroupQueryOpType::Subtract:
			EvaluationResult = EvaluationResult.Difference(QueryResult);
			break;
		}
	}
	
	return EvaluationResult;
}

UMovieGraphConditionGroupQueryBase* UMovieGraphConditionGroup::AddQuery(const TSubclassOf<UMovieGraphConditionGroupQueryBase>& InQueryType, const int32 InsertIndex)
{
	UMovieGraphConditionGroupQueryBase* NewQueryObj = NewObject<UMovieGraphConditionGroupQueryBase>(this, InQueryType.Get());

	if (InsertIndex < 0)
	{
		Queries.Add(NewQueryObj);
	}
	else
	{
		// Clamp the insert index to a valid range in case an invalid one is provided
		Queries.Insert(NewQueryObj, FMath::Clamp(InsertIndex, 0, Queries.Num()));
	}
	
	return NewQueryObj;
}

const TArray<UMovieGraphConditionGroupQueryBase*>& UMovieGraphConditionGroup::GetQueries() const
{
	return Queries;
}

bool UMovieGraphConditionGroup::RemoveQuery(UMovieGraphConditionGroupQueryBase* InQuery)
{
	return Queries.RemoveSingle(InQuery) == 1;
}

bool UMovieGraphConditionGroup::IsFirstConditionGroup() const
{
	const UMovieGraphCollection* ParentCollection = GetTypedOuter<UMovieGraphCollection>();
	if (ensureMsgf(ParentCollection, TEXT("Cannot determine if this is the first condition group when no parent collection is present")))
	{
		// GetConditionGroups() returns an array of non-const pointers, so Find() doesn't like having a const pointer passed to it.
		// Find() won't mutate the condition group though, so the const_cast here is OK.
		return ParentCollection->GetConditionGroups().Find(const_cast<UMovieGraphConditionGroup*>(this)) == 0;
	}
	
	return false;
}

bool UMovieGraphConditionGroup::MoveQueryToIndex(UMovieGraphConditionGroupQueryBase* InQuery, const int32 NewIndex)
{
#if WITH_EDITOR
	Modify();
#endif

	if (!InQuery)
	{
		return false;
	}

	const int32 ExistingIndex = Queries.Find(InQuery);
	if (ExistingIndex == INDEX_NONE)
	{
		return false;
	}

	// If the new index is greater than the current index, then decrement the destination index so it remains valid after the removal below
	int32 DestinationIndex = NewIndex;
	if (DestinationIndex > ExistingIndex)
	{
		--DestinationIndex;
	}

	Queries.Remove(InQuery);
	Queries.Insert(InQuery, DestinationIndex);

	// Enforce that the first query is set to Union
	InQuery->SetOperationType(EMovieGraphConditionGroupQueryOpType::Add);

	return true;
}

const FGuid& UMovieGraphConditionGroup::GetId() const
{
	return Id;
}

TSet<AActor*> UMovieGraphCollection::Evaluate(const UWorld* InWorld) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphCollection::Evaluate);
	
	TSet<AActor*> ResultSet;

	for (int32 ConditionGroupIndex = 0; ConditionGroupIndex < ConditionGroups.Num(); ++ConditionGroupIndex)
	{
		const TObjectPtr<UMovieGraphConditionGroup>& ConditionGroup = ConditionGroups[ConditionGroupIndex];
		if (!ConditionGroup)
		{
			continue;
		}
		
		if (ConditionGroupIndex == 0)
		{
			// The first condition group should always be a Union
			ensure(ConditionGroup->GetOperationType() == EMovieGraphConditionGroupOpType::Add);
		}

		const TSet<AActor*> QueryResult = ConditionGroup->Evaluate(InWorld);
		
		switch (ConditionGroup->GetOperationType())
		{
		case EMovieGraphConditionGroupOpType::Add:
			ResultSet = ResultSet.Union(QueryResult);
			break;

		case EMovieGraphConditionGroupOpType::And:
			ResultSet = ResultSet.Intersect(QueryResult);
			break;

		case EMovieGraphConditionGroupOpType::Subtract:
			ResultSet = ResultSet.Difference(QueryResult);
			break;
		}
	}
	
	return ResultSet;
}

UMovieGraphConditionGroup* UMovieGraphCollection::AddConditionGroup()
{
	UMovieGraphConditionGroup* NewConditionGroup = NewObject<UMovieGraphConditionGroup>(this);
	ConditionGroups.Add(NewConditionGroup);
	return NewConditionGroup;
}

const TArray<UMovieGraphConditionGroup*>& UMovieGraphCollection::GetConditionGroups() const
{
	return ConditionGroups;
}

bool UMovieGraphCollection::RemoveConditionGroup(UMovieGraphConditionGroup* InConditionGroup)
{
	return ConditionGroups.RemoveSingle(InConditionGroup) == 1;
}

bool UMovieGraphCollection::MoveConditionGroupToIndex(UMovieGraphConditionGroup* InConditionGroup, const int32 NewIndex)
{
#if WITH_EDITOR
	Modify();
#endif

	if (!InConditionGroup)
	{
		return false;
	}

	const int32 ExistingIndex = ConditionGroups.Find(InConditionGroup);
	if (ExistingIndex == INDEX_NONE)
	{
		return false;
	}

	// If the new index is greater than the current index, then decrement the destination index so it remains valid after the removal below
	int32 DestinationIndex = NewIndex;
	if (DestinationIndex > ExistingIndex)
	{
		--DestinationIndex;
	}

	ConditionGroups.Remove(InConditionGroup);
	ConditionGroups.Insert(InConditionGroup, DestinationIndex);

	// Enforce that the first condition group is set to Union
	InConditionGroup->SetOperationType(EMovieGraphConditionGroupOpType::Add);

	return true;
}

#if WITH_EDITOR
void UMovieGraphCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Name change delegate is broadcast here so it catches both SetCollectionName() and a direct change of the property via the details panel
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphCollection, CollectionName))
	{
		OnCollectionNameChangedDelegate.Broadcast(this);
	}
}
#endif	// WITH_EDITOR

void UMovieGraphCollection::SetCollectionName(const FString& InName)
{
	CollectionName = InName;
}

const FString& UMovieGraphCollection::GetCollectionName() const
{
	return CollectionName;
}

UMovieGraphCollection* UMoviePipelineRenderLayer::GetCollectionByName(const FString& Name) const
{
	for (const UMoviePipelineCollectionModifier* Modifier : Modifiers)
	{
		if (!Modifier)
		{
			continue;
		}
		
		for (UMovieGraphCollection* Collection : Modifier->GetCollections())
		{
			if (Collection && Collection->GetCollectionName().Equals(Name))
			{
				return Collection;
			}
		}
	}

	return nullptr;
}

void UMoviePipelineRenderLayer::AddModifier(UMoviePipelineCollectionModifier* Modifier)
{
	if (!Modifiers.Contains(Modifier))
	{
		Modifiers.Add(Modifier);
	}
}

void UMoviePipelineRenderLayer::RemoveModifier(UMoviePipelineCollectionModifier* Modifier)
{
	Modifiers.Remove(Modifier);
}

void UMoviePipelineRenderLayer::Preview(const UWorld* World)
{
	if (!World)
	{
		return;
	}
	
	// Apply all modifiers
	for (UMoviePipelineCollectionModifier* Modifier : Modifiers)
	{
		Modifier->ApplyModifier(World);
	}
}

void UMoviePipelineRenderLayer::UndoPreview(const UWorld* World)
{
	if (!World)
	{
		return;
	}

	// Undo actions performed by all modifiers. Do this in the reverse order that they were applied, since the undo
	// state of one modifier may depend on modifiers that were previously applied.
	for (int32 Index = Modifiers.Num() - 1; Index >= 0; Index--)
	{
		if (UMoviePipelineCollectionModifier* Modifier = Modifiers[Index])
		{
			Modifier->UndoModifier();
		}
	}
}


UMoviePipelineRenderLayerSubsystem* UMoviePipelineRenderLayerSubsystem::GetFromWorld(const UWorld* World)
{
	if (World)
	{
		return UWorld::GetSubsystem<UMoviePipelineRenderLayerSubsystem>(World);
	}

	return nullptr;
}

void UMoviePipelineRenderLayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	VisualizationEmptyCollection = NewObject<UMovieGraphCollection>(GetTransientPackage(), NAME_None, RF_Transient);

	// By default, the visualizer should hide everything in the world
	VisualizationModifier_HideWorld = NewObject<UMoviePipelineVisibilityModifier>(GetTransientPackage(), NAME_None, RF_Transient);
	VisualizationModifier_HideWorld->AddCollection(VisualizationEmptyCollection);
	VisualizationModifier_HideWorld->SetHidden(true);

	// Selectively show collections in the visualization
	VisualizationModifier_VisibleCollections = NewObject<UMoviePipelineVisibilityModifier>(GetTransientPackage(), NAME_None, RF_Transient);

	// The visualizer render layer will hide the world, selectively show specified collections, and then apply any other provided modifiers
	VisualizationRenderLayer = NewObject<UMoviePipelineRenderLayer>(GetTransientPackage(), NAME_None, RF_Transient);
	VisualizationRenderLayer->AddModifier(VisualizationModifier_HideWorld);
	VisualizationRenderLayer->AddModifier(VisualizationModifier_VisibleCollections);
}

void UMoviePipelineRenderLayerSubsystem::Deinitialize()
{
}

void UMoviePipelineRenderLayerSubsystem::Reset()
{
	ClearAllPreviews();
	RenderLayers.Empty();
}

bool UMoviePipelineRenderLayerSubsystem::AddRenderLayer(UMoviePipelineRenderLayer* RenderLayer)
{
	if (!RenderLayer)
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Invalid render layer provided to AddRenderLayer()."));
		return false;
	}
	
	const bool bRenderLayerExists = RenderLayers.ContainsByPredicate([RenderLayer](const UMoviePipelineRenderLayer* RL)
	{
		return RL && (RenderLayer->GetRenderLayerName() == RL->GetName());
	});

	if (bRenderLayerExists)
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Render layer '%s' already exists in the render layer subsystem; it will not be added again."), *RenderLayer->GetRenderLayerName().ToString());
		return false;
	}

	RenderLayers.Add(RenderLayer);
	return true;
}

void UMoviePipelineRenderLayerSubsystem::RemoveRenderLayer(const FString& RenderLayerName)
{
	if (ActiveRenderLayer && (ActiveRenderLayer->GetName() == RenderLayerName))
	{
		ClearAllPreviews();
	}
	
	const uint32 Index = RenderLayers.IndexOfByPredicate([&RenderLayerName](const UMoviePipelineRenderLayer* RenderLayer)
	{
		return RenderLayer->GetRenderLayerName() == RenderLayerName;
	});

	if (Index != INDEX_NONE)
	{
		RenderLayers.RemoveAt(Index);
	}
}

void UMoviePipelineRenderLayerSubsystem::SetActiveRenderLayerByObj(UMoviePipelineRenderLayer* RenderLayer)
{
	if (!RenderLayer)
	{
		return;
	}
	
	ClearAllPreviews();
	SetAndPreviewRenderLayer(RenderLayer);
}

void UMoviePipelineRenderLayerSubsystem::SetActiveRenderLayerByName(const FName& RenderLayerName)
{
	const uint32 Index = RenderLayers.IndexOfByPredicate([&RenderLayerName](const UMoviePipelineRenderLayer* RenderLayer)
	{
		return RenderLayer->GetRenderLayerName() == RenderLayerName;
	});

	if (Index != INDEX_NONE)
	{
		SetActiveRenderLayerByObj(RenderLayers[Index]);
	}
}

void UMoviePipelineRenderLayerSubsystem::ClearActiveRenderLayer()
{
	ClearAllPreviews();
}

void UMoviePipelineRenderLayerSubsystem::PreviewCollection(UMovieGraphCollection* Collection)
{
	if (!Collection)
	{
		return;
	}

	ClearAllPreviews();
	
	ActiveCollection = Collection;
	VisualizationModifier_VisibleCollections->AddCollection(ActiveCollection);

	SetAndPreviewRenderLayer(VisualizationRenderLayer);
}

void UMoviePipelineRenderLayerSubsystem::ClearCollectionPreview()
{
	ClearAllPreviews();
}

void UMoviePipelineRenderLayerSubsystem::ClearAllPreviews()
{
	// Render layer previews and collection previews both use the active render layer, so undoing the preview this
	// way will clear previews for both
	if (ActiveRenderLayer)
	{
		ActiveRenderLayer->UndoPreview(GetWorld());

		// Remove the modifier preview if present (requires an active render layer)
		if (ActiveModifier)
		{
			ActiveRenderLayer->RemoveModifier(ActiveModifier);
		}
	}

	// Reset the viz modifier for the collection preview
	VisualizationModifier_VisibleCollections->SetCollections({});

	ActiveRenderLayer = nullptr;
	ActiveCollection = nullptr;
	ActiveModifier = nullptr;
}

void UMoviePipelineRenderLayerSubsystem::SetAndPreviewRenderLayer(UMoviePipelineRenderLayer* RenderLayer)
{
	ActiveRenderLayer = RenderLayer;
	ActiveRenderLayer->Preview(GetWorld());
}

void UMoviePipelineRenderLayerSubsystem::PreviewModifier(UMoviePipelineCollectionModifier* Modifier)
{
	if (!Modifier)
	{
		return;
	}

	ClearAllPreviews();

	ActiveModifier = Modifier;
	VisualizationRenderLayer->AddModifier(ActiveModifier);

	// Add the modifier's collections to the viz as well, so the actors that the modifier affects are actually visible
	// TODO: This may need special handing for visibility modifiers
	for (UMovieGraphCollection* Collection : ActiveModifier->GetCollections())
	{
		VisualizationModifier_VisibleCollections->AddCollection(Collection);
	}
	
	SetAndPreviewRenderLayer(VisualizationRenderLayer);
}

void UMoviePipelineRenderLayerSubsystem::ClearModifierPreview()
{
	ClearAllPreviews();
}

#undef LOCTEXT_NAMESPACE