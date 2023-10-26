// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/Object/EditorObjectSelectionSourceModel.h"

#include "Replication/Editor/Model/Object/SelectedActorsSource.h"
#include "Replication/Editor/Model/Object/WorldActorSource.h"

#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "FEditorObjectSelectionSourceModel"

namespace UE::ConcertClientSharedSlate
{
	FEditorObjectSelectionSourceModel::FEditorObjectSelectionSourceModel()
		: BaseActorCategory([this]()
		{
			return FObjectSourceCategory
			{
				ConcertSharedSlate::FBaseDisplayInfo
				{
					LOCTEXT("ObjectsCategory.Label", "Add Objects"),
					LOCTEXT("ObjectsCategory.Tooltip", "Options for adding objects from the open editor world")
				},
				{
					MakeShared<FSelectedActorsSource>(),
					MakeShared<FWorldActorSource>()
				}
			};
		}())
	{}

	TArray<FObjectSourceCategory> FEditorObjectSelectionSourceModel::GetRootSources() const
	{
		FObjectSourceCategory ActorCategory = BaseActorCategory;
		return { ActorCategory };
	}

	TArray<TSharedRef<IObjectSourceModel>> FEditorObjectSelectionSourceModel::GetContextMenuOptions(const FSoftObjectPath& Item)
	{
		return {};
	}

	EObjectItemValidity FEditorObjectSelectionSourceModel::GetItemValidity(const FSoftObjectPath& Item) const
	{
		if (!Item.ResolveObject() && !Item.TryLoad())
		{
			return EObjectItemValidity::DoesNotExist;
		}
		
		return EObjectItemValidity::Invalid;
	}
}

#undef LOCTEXT_NAMESPACE