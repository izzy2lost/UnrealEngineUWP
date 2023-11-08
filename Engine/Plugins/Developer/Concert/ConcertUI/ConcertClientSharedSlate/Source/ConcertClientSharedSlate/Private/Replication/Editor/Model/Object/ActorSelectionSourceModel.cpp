// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/Object/ActorSelectionSourceModel.h"

#include "Replication/Editor/Model/Object/SelectedActorsSource.h"
#include "Replication/Editor/Model/Object/WorldActorSource.h"

#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "FEditorObjectSelectionSourceModel"

namespace UE::ConcertClientSharedSlate
{
	FActorSelectionSourceModel::FActorSelectionSourceModel()
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

	TArray<FObjectSourceCategory> FActorSelectionSourceModel::GetRootSources() const
	{
		FObjectSourceCategory ActorCategory = BaseActorCategory;
		return { ActorCategory };
	}

	TArray<TSharedRef<IObjectSourceModel>> FActorSelectionSourceModel::GetContextMenuOptions(const FSoftObjectPath& Item)
	{
		return {};
	}
}

#undef LOCTEXT_NAMESPACE