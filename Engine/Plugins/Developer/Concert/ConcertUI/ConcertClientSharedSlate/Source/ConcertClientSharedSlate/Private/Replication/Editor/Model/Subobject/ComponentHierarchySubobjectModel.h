// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/ISubobjectModel.h"
#include "UObject/SoftObjectPtr.h"

class AActor;

namespace UE::ConcertClientSharedSlate
{
	/**
	 * Builds a similar tree hierarchy as SSubobjectEditor.
	 * This model only inspects UActorComponents on the root object, which must be an actor.
	 */
	class FComponentHierarchySubobjectModel : public ConcertSharedSlate::ISubobjectModel
	{
	public:

		/** Instances of USceneComponent are grouped to this category. */
		static const FName SceneComponentsCategory;
		/** Instances of UActorComponent which are not USceneComponents are grouped to this category. */
		static const FName ActorComponentsCategory;

		//~ Begin ISubobjectModel Interface
		virtual void SetTopLevelObject(const FSoftObjectPath& InTopLevelObject) override;
		virtual FSoftObjectPath GetTopLevelObject() const override { return TopLevelObject; }
		virtual bool IsTopLevelObject(const FSoftObjectPath& Object) const override;
		virtual TArray<FName> GetCategories() const override { return { SceneComponentsCategory, ActorComponentsCategory }; }
		virtual FText GetSubobjectDisplayName(const FSoftObjectPath& ObjectPath) const override;
		virtual void ForEachRootSubobject(FName Category, TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Callback) const override;
		virtual void ForEachDirectChildSubobject(const FSoftObjectPath& Parent, TFunctionRef<EBreakBehavior(const FSoftObjectPath&)> Callback) const override;
		virtual FOnHierarchyChanged& OnHierarchyChanged() override { return OnHierarchyChangedDelegate; }
		//~ End ISubobjectModel Interface

	private:

		/** The object from which the hierarchy is generated. */
		FSoftObjectPath TopLevelObject;

		struct FObjectMetaData
		{
			FText DisplayLabel;
		};
		TMap<FSoftObjectPath,FObjectMetaData> ObjectMetaData;

		/** Called when the hierarchy has changed, e.g. due to calling SetTopLevelObject. */
		FOnHierarchyChanged OnHierarchyChangedDelegate;

		AActor* GetTopLevelObjectAsActor() const;
	};
}


