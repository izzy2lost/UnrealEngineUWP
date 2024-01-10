// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertySelectionSourceModel.h"

namespace UE::ConcertSharedSlate
{
	class FConcertSyncCoreReplicatedPropertySource;
	
	/**
	 * Decides which properties can be added to IEditableReplicationStreamModel.
	 * The allowed properties are those returned by UE::ConcertSyncCore::ForEachReplicatableProperty.
	 */
	class CONCERTSHAREDSLATE_API FSelectPropertyFromUClassModel : public IPropertySelectionSourceModel
	{
	public:

		FSelectPropertyFromUClassModel();

		//~ Begin IPropertySelectionSourceModel Interface
		virtual TSharedRef<IPropertySourceModel> GetPropertySource(const FSoftClassPath& Class) const override;
		//~ End IPropertySelectionSourceModel Interface

	private:

		TSharedRef<FConcertSyncCoreReplicatedPropertySource> UClassIteratorSource;
	};
}

