// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/Object/EditorObjectSelectionSourceModel.h"

#include "Replication/Editor/Model/Object/AddOuterSource.h"
#include "Replication/Editor/Model/Object/ComponentFromActorSource.h"
#include "Replication/Editor/Model/Object/SelectedActorsSource.h"
#include "Replication/Editor/Model/Object/WorldActorSource.h"

#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "FEditorObjectSelectionSourceModel"

namespace UE::ConcertClientSharedSlate
{
	FEditorObjectSelectionSourceModel::FEditorObjectSelectionSourceModel()
		: WorldActorSource(MakeShared<FWorldActorSource>())
		// Important: We dependent on the fact that WorldActorSource is declared (in the header) and hence initialized before BaseActorCategory
		, BaseActorCategory([this]()
		{
			return FObjectSourceCategory
			{
				ConcertSharedSlate::FBaseDisplayInfo
				{
					LOCTEXT("ObjectsCategory.Label", "Add Objects"),
					LOCTEXT("ObjectsCategory.Tooltip", "Options for adding objects from the open editor world")
				},
				{
					WorldActorSource,
					MakeShared<FSelectedActorsSource>()
				}
			};
		}())
	{}

	TArray<FObjectSourceCategory> FEditorObjectSelectionSourceModel::GetRootSources() const
	{
		FObjectSourceCategory ActorCategory = BaseActorCategory;
		FComponentFromActorSource_Root::AddComponentsAdditionToRootCategory(WorldActorSource.Get(), ActorCategory);
		return { ActorCategory };
	}

	TArray<TSharedRef<IObjectSourceModel>> FEditorObjectSelectionSourceModel::GetContextMenuOptions(const FSoftObjectPath& Item)
	{
		return ContextMenuSources.BuildContextSources(Item);
	}

	EObjectItemValidity FEditorObjectSelectionSourceModel::GetItemValidity(const FSoftObjectPath& Item) const
	{
		if (!Item.ResolveObject() && !Item.TryLoad())
		{
			return EObjectItemValidity::DoesNotExist;
		}
		
		return EObjectItemValidity::Invalid;
	}

	FEditorObjectSelectionSourceModel::FContextMenuSources::FContextMenuSources()
		: AddComponent(MakeShared<FComponentFromActorSource_ContextMenu>())
		, AddOuter(MakeShared<FAddOuterSource>())
	{}

	TArray<TSharedRef<IObjectSourceModel>> FEditorObjectSelectionSourceModel::FContextMenuSources::BuildContextSources(const FSoftObjectPath& Item)
	{
		UObject* ResolvedContextObject = Item.ResolveObject();
		AActor* ResolvedContextActor = Cast<AActor>(ResolvedContextObject);
		TArray<TSharedRef<IObjectSourceModel>> Result;

		// The "Add Outer" option is not valid for all cases. For example adding an actor's outer is not valid.
		if (AddOuter->TrySetObjectIfValid(ResolvedContextObject))
		{
			Result.Add(AddOuter);
		}

		// The "Add component" option is only valid if the context object is an actor displayed in the scene outliner
		if (AddComponent->TrySetActorIfValid(ResolvedContextActor))
		{
			Result.Add(AddComponent);
		}

		return Result;
	}
}

#undef LOCTEXT_NAMESPACE