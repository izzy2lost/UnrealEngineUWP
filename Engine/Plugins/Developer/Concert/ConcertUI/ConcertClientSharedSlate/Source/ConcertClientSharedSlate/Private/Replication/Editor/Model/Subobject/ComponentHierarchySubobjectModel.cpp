// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComponentHierarchySubobjectModel.h"

#include "Components/SceneComponent.h"
#include "Misc/EBreakBehavior.h"
#include "Replication/ObjectUtils.h"

#include "GameFramework/Actor.h"
#include "SubobjectDataSubsystem.h"

namespace UE::ConcertClientSharedSlate
{
	namespace Private
	{
		static bool IsValidComponent(const UActorComponent& Component)
		{
			return !Component.IsVisualizationComponent();
		}
	}
	
	const FName FComponentHierarchySubobjectModel::SceneComponentsCategory(TEXT("SceneComponents"));
	const FName FComponentHierarchySubobjectModel::ActorComponentsCategory(TEXT("ActorComponents"));
	
	void FComponentHierarchySubobjectModel::SetTopLevelObject(const FSoftObjectPath& InTopLevelObject)
	{
		if (InTopLevelObject != TopLevelObject)
		{
			TopLevelObject = InTopLevelObject;
			if (AActor* Actor = GetTopLevelObjectAsActor()
				; !TopLevelObject.IsNull() && ensureAlwaysMsgf(Actor, TEXT("Not a top-level object")))
			{
				USubobjectDataSubsystem* SubobjectDataSubsystem = USubobjectDataSubsystem::Get();
				TArray<FSubobjectDataHandle> Handles;
				SubobjectDataSubsystem->GatherSubobjectData(Actor, Handles);
				
				ObjectMetaData.Empty();
				for (const FSubobjectDataHandle& Handle : Handles)
				{
					const FSubobjectData* SubobjectData = Handle.GetData();
					const UObject* Object = SubobjectData->FindComponentInstanceInActor(Actor);
					if (Object)
					{
						ObjectMetaData.Add(Object, { FText::FromString(SubobjectData->GetDisplayString()) });
					}
				}
			}
		}
	}

	bool FComponentHierarchySubobjectModel::IsTopLevelObject(const FSoftObjectPath& Object) const
	{
		return ConcertSharedSlate::ObjectUtils::IsActor(Object);
	}

	void FComponentHierarchySubobjectModel::ForEachRootSubobject(FName Category, TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Callback) const
	{
		const AActor* Actor = GetTopLevelObjectAsActor();
		if (!Actor)
		{
			return;
		}

		if (Category == SceneComponentsCategory)
		{
			if (Actor->GetRootComponent())
			{
				Callback(Actor->GetRootComponent());
			}
		}
		else if (Category == ActorComponentsCategory)
		{
			for (const UActorComponent* Component : Actor->GetComponents())
			{
				if (Private::IsValidComponent(*Component)
					&& !Component->IsA<USceneComponent>()
					&& Callback(Component) == EBreakBehavior::Break)
				{
					break;
				}
			}
		}
	}

	void FComponentHierarchySubobjectModel::ForEachDirectChildSubobject(const FSoftObjectPath& Parent, TFunctionRef<EBreakBehavior(const FSoftObjectPath&)> Callback) const
	{
		const UObject* Object = Parent.ResolveObject();
		const UObject* TopLevel = GetTopLevelObjectAsActor();
		if (!Object || !ensureAlways(TopLevel && Object->IsIn(TopLevel)))
		{
			return;
		}
		
		const USceneComponent* SceneComponent = Cast<USceneComponent>(Object);
		if (!SceneComponent)
		{
			return;
		}

		for (int32 i = 0; i < SceneComponent->GetNumChildrenComponents(); ++i)
		{
			const USceneComponent* ChildComponent = SceneComponent->GetChildComponent(i);
			if (Private::IsValidComponent(*ChildComponent)
				&& Callback(ChildComponent) == EBreakBehavior::Break)
			{
				break;
			}
		}
	}

	AActor* FComponentHierarchySubobjectModel::GetTopLevelObjectAsActor() const
	{
		return Cast<AActor>(TopLevelObject.ResolveObject());
	}
}