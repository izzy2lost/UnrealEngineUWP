// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IObjectSelectionSourceModel.h"
#include "StreamEditor/Model/Object/IObjectSourceModel.h"

class AActor;
class UActorComponent;

namespace UE::MultiUserReplicationEditor
{
	/** Gets the components on a given actor. Reused by the context menu and root filters. */
	class FComponentFromActorSource_Base : public IObjectSourceModel
	{
	public:

		bool TrySetActorIfValid(AActor* InComponentSource);

		//~ Begin IObjectSourceModel Interface
		virtual uint32 GetNumSelectableItems() const override;
		virtual void EnumerateSelectableItems(TFunctionRef<EBreakBehavior(const FSelectableObjectInfo& SelectableOption)> Delegate) const override;
		//~ End IObjectSourceModel Interface

	protected:

		/** Checks that the component would be visible in the detail's SSubobjectEditor */
		static bool IsComponentValidReplicationObject(UActorComponent* Component);
		
		TWeakObjectPtr<AActor> GetComponentSource() const { return ComponentSource; }
		
	private:

		/** The actor from which to add a component from. */
		TWeakObjectPtr<AActor> ComponentSource;
	};

	/** Used from the context menu. The label is "Add component". */
	class FComponentFromActorSource_ContextMenu : public FComponentFromActorSource_Base
	{
	public:

		//~ Begin IObjectSourceModel Interface
		virtual ConcertSharedSlate::FSourceDisplayInfo GetDisplayInfo() const override;
		//~ End IObjectSourceModel Interface
	};

	/** A root filter. The label is the name of the actor. */
	class FComponentFromActorSource_Root : public FComponentFromActorSource_Base
	{
	public:
		
		/**
		 * Adds a "Add component" sub-category. When expanded, it will list all actors as sub-menus, which contain components to add.
		 *
		 * @param ActorSource This model provides the actors for which components can be added
		 * @param OutCategory A new sub-category is added to this category
		 */
		static void AddComponentsAdditionToRootCategory(IObjectSourceModel& ActorSource, FObjectSourceCategory& OutCategory);

		//~ Begin IObjectSourceModel Interface
		virtual ConcertSharedSlate::FSourceDisplayInfo GetDisplayInfo() const override;
		//~ End IObjectSourceModel Interface
	};
}
