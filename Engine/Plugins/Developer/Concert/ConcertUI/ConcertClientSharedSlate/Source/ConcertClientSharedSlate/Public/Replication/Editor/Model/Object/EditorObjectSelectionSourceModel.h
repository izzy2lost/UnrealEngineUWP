// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/Object/IObjectSelectionSourceModel.h"

namespace UE::ConcertClientSharedSlate
{
	class FAddOuterSource;
	class FComponentFromActorSource_ContextMenu;
	class FWorldActorSource;

	/** Collection of default logic for selecting objects from the current editor world. */
	class CONCERTCLIENTSHAREDSLATE_API FEditorObjectSelectionSourceModel : public IObjectSelectionSourceModel
	{
	public:

		FEditorObjectSelectionSourceModel();

		//~ Begin IObjectSelectionSourceModel Interface
		virtual TArray<FObjectSourceCategory> GetRootSources() const override;
		virtual TArray<TSharedRef<IObjectSourceModel>> GetContextMenuOptions(const FSoftObjectPath& Item) override;
		virtual EObjectItemValidity GetItemValidity(const FSoftObjectPath& Item) const override;
		//~ End IObjectSelectionSourceModel Interface

	private:

		/** Contains actions around adding actor related objects. Displayed next to the search bar. Refined with dynamic options in GetRootSources. */
		FObjectSourceCategory BaseActorCategory;
	};
}

