// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ConcertPropertySelection.h"

namespace UE::MultiUserReplicationEditor
{
	/** Instanced for each property row in SPropertyReplicationSelectionViewer.*/
	class FReplicatedPropertyData
	{
	public:
		
		FReplicatedPropertyData(FSoftClassPath OwningClass, FConcertPropertyChain Object)
			: OwningClass(MoveTemp(OwningClass))
			, Property(MoveTemp(Object))
		{}
		
		const FConcertPropertyChain& GetProperty() const { return Property; }
		const FSoftClassPath& GetOwningClass() const { return OwningClass; }

	private:

		/** The class with which the FProperty can be determined. */
		FSoftClassPath OwningClass;
		
		/** The object identified by the */
		FConcertPropertyChain Property;
	};

	/** Instanced for each property row SPropertyReplicationSelectionEditor. */
	class FReplicatedPropertyData_Editor : public FReplicatedPropertyData
	{
		// Empty and unused for now. I just want to highlight that if you want custom data for SPropertyReplicationSelectionEditor,
		// you should add it here and override SPropertyReplicationSelectionViewer::AllocatePropertyData.
	};
}