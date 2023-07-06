// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StreamEditor/Model/Object/IObjectSelectionSourceModel.h"

namespace UE::MultiUserReplicationEditor
{
	class FAddOuterSource;
	class FComponentFromActorSource_ContextMenu;
	class FWorldActorSource;
	
	class FEditorObjectSelectionSourceModel : public IObjectSelectionSourceModel
	{
	public:

		FEditorObjectSelectionSourceModel();

		//~ Begin IObjectSelectionSourceModel Interface
		virtual TArray<FObjectSourceCategory> GetRootSources() const override;
		virtual TArray<TSharedRef<IObjectSourceModel>> GetContextMenuOptions(const FSoftObjectPath& Item) override;
		virtual EObjectItemValidity GetItemValidity(const FSoftObjectPath& Item) const override;
		//~ End IObjectSelectionSourceModel Interface

	private:

		/** Used to build Add Component source from BaseActorCategory. If you move this property, make sure it declared before BaseActorCategory, or update the constructor. */
		TSharedRef<FWorldActorSource> WorldActorSource;

		/** Contains actions around adding actor related objects. Displayed next to the search bar. Refined with dynamic options in GetRootSources. */
		FObjectSourceCategory BaseActorCategory;

		/** Sources which have to be instantiated with a context. */
		struct FContextMenuSources
		{
			/** Add components from the actor for which the context menu is instantiated. */
			TSharedRef<FComponentFromActorSource_ContextMenu> AddComponent;

			/** Adds the object's outer as object. */
			TSharedRef<FAddOuterSource> AddOuter;
			
			FContextMenuSources();

			/** Gets all applicable context menu sources. */
			TArray<TSharedRef<IObjectSourceModel>> BuildContextSources(const FSoftObjectPath& Item);
		} ContextMenuSources;
	};
}

