// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MoviePipelineRenderLayerSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Styling/AppStyle.h"
#include "UObject/Package.h"

void UMoviePipelineMaterialModifier::ApplyModifier(const UWorld* World)
{
	UMaterialInterface* NewMaterial = MaterialToApply.LoadSynchronous();
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
	
}

void UMoviePipelineVisibilityModifier::ApplyModifier(const UWorld* World)
{
	ModifiedActors.Empty();

	// SetActorVisibilityState() takes a FActorVisibilityState, but all calls here will contain the same settings (with
	// a differing actor), so create one copy to prevent constantly re-creating structs.
	FActorVisibilityState NewVisibilityState;
	NewVisibilityState.bIsHidden = bIsHidden;
	NewVisibilityState.bCastsShadows = bCastsShadows;
	NewVisibilityState.bCastShadowWhileHidden = bCastShadowWhileHidden;
	NewVisibilityState.bAffectIndirectLightingWhileHidden = bAffectIndirectLightingWhileHidden;
	NewVisibilityState.bHoldout = bHoldout;
	
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
			if (const UPrimitiveComponent* PrimitiveComponent = Actor->GetComponentByClass<UPrimitiveComponent>())
			{
				OriginalVisibilityState.bCastsShadows = PrimitiveComponent->CastShadow;
				OriginalVisibilityState.bCastShadowWhileHidden = PrimitiveComponent->bCastHiddenShadow;
				OriginalVisibilityState.bAffectIndirectLightingWhileHidden = PrimitiveComponent->bAffectIndirectLightingWhileHidden;
				OriginalVisibilityState.bHoldout = PrimitiveComponent->bHoldout;
			}
			
			ModifiedActors.Add(OriginalVisibilityState);

			// Set new visibility state
			NewVisibilityState.Actor = Actor;
			SetActorVisibilityState(NewVisibilityState);
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
	
	Actor->SetActorHiddenInGame(NewVisibilityState.bIsHidden);

#if WITH_EDITOR
	Actor->SetIsTemporarilyHiddenInEditor(NewVisibilityState.bIsHidden);
#endif

	if (UPrimitiveComponent* PrimitiveComponent = Actor->GetComponentByClass<UPrimitiveComponent>())
	{
		// TODO: These could potentially cause a large rendering penalty due to dirtying the render state; investigate potential
		// ways to optimize this
		PrimitiveComponent->SetCastShadow(NewVisibilityState.bCastsShadows);
		PrimitiveComponent->SetCastHiddenShadow(NewVisibilityState.bCastShadowWhileHidden);
		PrimitiveComponent->SetAffectIndirectLightingWhileHidden(NewVisibilityState.bAffectIndirectLightingWhileHidden);
		PrimitiveComponent->SetHoldout(NewVisibilityState.bHoldout);
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
	: OpType(EMovieGraphConditionGroupQueryOpType::Union)
	, bIsEnabled(true)
{
}

void UMovieGraphConditionGroupQueryBase::SetOperationType(const EMovieGraphConditionGroupQueryOpType OperationType)
{
	// Always allow setting the operation type to Union. If not setting to Union, only allow setting the operation type if this is not the first
	// query in the condition group. The first query is always a Union.
	if (OperationType == EMovieGraphConditionGroupQueryOpType::Union)
	{
		OpType = EMovieGraphConditionGroupQueryOpType::Union;
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

void UMovieGraphConditionGroupQueryBase::Evaluate(const TArray<AActor*>& InActorsToQuery, TSet<AActor*>& OutMatchingActors) const
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

void UMovieGraphConditionGroupQuery_ActorTag::Evaluate(const TArray<AActor*>& InActorsToQuery, TSet<AActor*>& OutMatchingActors) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroupQuery_ActorTag::Evaluate);
	
	for (AActor* Actor : InActorsToQuery)
	{
		if (Actor->Tags.Contains(TagToMatch))
		{
			OutMatchingActors.Add(Actor);
		}
	}
}

const FSlateIcon& UMovieGraphConditionGroupQuery_ActorTag::GetIcon() const
{
	// TODO: This icon is wrong
	static const FSlateIcon ActorTagIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "TODO");
	return ActorTagIcon;
}

void UMovieGraphConditionGroupQuery_ActorName::Evaluate(const TArray<AActor*>& InActorsToQuery, TSet<AActor*>& OutMatchingActors) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroupQuery_ActorName::Evaluate);
	
	const bool bIsMatchAll = (WildcardSearch == TEXT("*"));

	// Actor names can be specified on multiple lines
	TArray<FString> AllActorNames;
	WildcardSearch.ParseIntoArrayLines(AllActorNames);

	// Quick early-out if "*" is used as the wildcard. Faster than doing the wildcard matching.
	if (bIsMatchAll)
	{
		OutMatchingActors.Append(InActorsToQuery);
		return;
	}

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

bool UMovieGraphConditionGroupQuery_ActorName::IsEditorOnly() const
{
	// GetActorLabel() is editor-only
	return true;
}

void UMovieGraphConditionGroupQuery_ActorType::Evaluate(const TArray<AActor*>& InActorsToQuery, TSet<AActor*>& OutMatchingActors) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroupQuery_ActorType::Evaluate);
	
	for (AActor* Actor : InActorsToQuery)
	{
		if (Actor->GetClass() == ActorType)
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

void UMovieGraphConditionGroupQuery_ComponentTag::Evaluate(const TArray<AActor*>& InActorsToQuery, TSet<AActor*>& OutMatchingActors) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroupQuery_ComponentTag::Evaluate);
	
	TInlineComponentArray<UActorComponent*> ActorComponents;

	for (AActor* Actor : InActorsToQuery)
	{
		Actor->GetComponents<UActorComponent*>(ActorComponents);
		
		for (const UActorComponent* Component : ActorComponents)
		{
			if (Component && Component->ComponentTags.Contains(TagToMatch))
			{
				OutMatchingActors.Add(Actor);
			}
		}

		ActorComponents.Reset();
	}
}

const FSlateIcon& UMovieGraphConditionGroupQuery_ComponentTag::GetIcon() const
{
	// TODO: This icon is wrong
	static const FSlateIcon ActorTagIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset");
	return ActorTagIcon;
}

void UMovieGraphConditionGroupQuery_ComponentType::Evaluate(const TArray<AActor*>& InActorsToQuery, TSet<AActor*>& OutMatchingActors) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphConditionGroupQuery_ComponentType::Evaluate);
	
	TInlineComponentArray<UActorComponent*> ActorComponents;

	for (AActor* Actor : InActorsToQuery)
	{
		Actor->GetComponents<UActorComponent*>(ActorComponents);
		
		for (const UActorComponent* Component : ActorComponents)
		{
			if (Component->GetClass() == ComponentType)
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

UMovieGraphConditionGroup::UMovieGraphConditionGroup()
	: OpType(EMovieGraphConditionGroupOpType::Union)
	, bIsEnabled(true)
{
}

void UMovieGraphConditionGroup::SetOperationType(const EMovieGraphConditionGroupOpType OperationType)
{
	// Always allow setting the operation type to Union. If not setting to Union, only allow setting the operation type if this is not the first
	// condition group in the collection. The first condition group is always a Union.
	if (OperationType == EMovieGraphConditionGroupOpType::Union)
	{
		OpType = EMovieGraphConditionGroupOpType::Union;
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
			ensure(Query->GetOperationType() == EMovieGraphConditionGroupQueryOpType::Union);
		}

		// Similar to EvaluationResult, QueryResult is persisted+reset to prevent constantly re-allocating it
		QueryResult.Reset();

		Query->Evaluate(AllActors, QueryResult);
		
		switch (Query->GetOperationType())
		{
		case EMovieGraphConditionGroupQueryOpType::Union:
			// Append() is faster than Union() because we don't need to allocate a new set
			EvaluationResult.Append(QueryResult);
			break;

		case EMovieGraphConditionGroupQueryOpType::Intersect:
			EvaluationResult = EvaluationResult.Intersect(QueryResult);
			break;

		case EMovieGraphConditionGroupQueryOpType::Subtract:
			EvaluationResult = EvaluationResult.Difference(QueryResult);
			break;
		}
	}
	
	return EvaluationResult;
}

UMovieGraphConditionGroupQueryBase* UMovieGraphConditionGroup::AddQuery(const TSubclassOf<UMovieGraphConditionGroupQueryBase>& InQueryType)
{
	UMovieGraphConditionGroupQueryBase* NewQueryObj = NewObject<UMovieGraphConditionGroupQueryBase>(this, InQueryType.Get());
	Queries.Add(NewQueryObj);
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

void UMovieGraphConditionGroup::SetQueryOrder(TArray<UMovieGraphConditionGroupQueryBase*>& InQueryOrder)
{
	// TODO
}

void UMovieGraphConditionGroup::SetEnabled(const bool bEnabled)
{
	bIsEnabled = bEnabled;
}

bool UMovieGraphConditionGroup::IsEnabled() const
{
	return bIsEnabled;
}

TSet<AActor*> UMovieGraphCollection::Evaluate(const UWorld* InWorld) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMovieGraphCollection::Evaluate);
	
	TSet<AActor*> ResultSet;

	for (int32 ConditionGroupIndex = 0; ConditionGroupIndex < ConditionGroups.Num(); ++ConditionGroupIndex)
	{
		const TObjectPtr<UMovieGraphConditionGroup>& ConditionGroup = ConditionGroups[ConditionGroupIndex];
		if (!ConditionGroup || !ConditionGroup->IsEnabled())
		{
			continue;
		}
		
		if (ConditionGroupIndex == 0)
		{
			// The first condition group should always be a Union
			ensure(ConditionGroup->GetOperationType() == EMovieGraphConditionGroupOpType::Union);
		}

		const TSet<AActor*> QueryResult = ConditionGroup->Evaluate(InWorld);
		
		switch (ConditionGroup->GetOperationType())
		{
		case EMovieGraphConditionGroupOpType::Union:
			ResultSet = ResultSet.Union(QueryResult);
			break;

		case EMovieGraphConditionGroupOpType::Intersect:
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

void UMovieGraphCollection::SetCollectionName(const FString& InName)
{
	CollectionName = InName;
}

const FString& UMovieGraphCollection::GetCollectionName() const
{
	return CollectionName;
}

void UMovieGraphCollection::SetConditionGroupOrder(TArray<UMovieGraphConditionGroup*>& InConditionGroupOrder)
{
	// TODO
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
	VisualizationModifier_HideWorld->bUseInvertedActors = true;

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