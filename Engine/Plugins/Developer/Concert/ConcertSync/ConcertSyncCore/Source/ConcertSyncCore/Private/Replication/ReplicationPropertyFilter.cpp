// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ReplicationPropertyFilter.h"

#include "Replication/PropertyChainUtils.h"
#include "Replication/Data/ConcertPropertySelection.h"
#include "Serialization/ArchiveSerializedPropertyChain.h"

namespace UE::ConcertSyncCore
{
	FReplicationPropertyFilter::FReplicationPropertyFilter(const FConcertPropertySelection& PropertySelection)
		: PropertySelection(PropertySelection)
	{
		for (int32 i = 0; i < PropertySelection.ReplicatedProperties.Num(); ++i)
		{
			const FConcertPropertyChain& Chain = PropertySelection.ReplicatedProperties[i];
			LeafToChain.FindOrAdd(Chain.GetLeafProperty()).Add(i);
		}
	}

	bool FReplicationPropertyFilter::ShouldSerializeProperty(const FArchiveSerializedPropertyChain* Chain, const FProperty& Property) const
	{
		// This would mean Property is 1. in a container and 1.1 primitive or 1.2 a native serialized struct 
		if (PropertyChain::IsPropertyEligibleForMarkingAsInternal(Property))
		{
			if (ensureMsgf(Chain && Chain->GetNumProperties() >= 1, TEXT("Assumption broken that Property is in a container (array, set, map). Check IsPropertyEligibleForMarkingAsInternal implementation!")))
			{
				return false;
			}
			
			// We should only have gotten here because a previous call to IsPropertyInSelection(&CopiedChain, *OwnerOfProperty) returned true already (or the property would not have been pushed into the chain).
			return true;
		}
		
		const TArray<int32>* IndicesToSearch = LeafToChain.Find(Property.GetFName());
		if (!IndicesToSearch)
		{
			/* No chain ends with this property so it is not in the selection
			 * Note: The property selection must also contain every parent property.
			 * Example: If MyStruct.Float is in the selection, so must be MyStruct.
			 * Otherwise this will return false for properties in the middle of the chain (and it should return true).
			 */
			return false;
		}

		for (const int32 IndexToSearch : *IndicesToSearch)
		{
			const FConcertPropertyChain& ConcertChain = PropertySelection.ReplicatedProperties[IndexToSearch];
			if (ConcertChain.MatchesExactly(Chain, Property))
			{
				return true;
			}
		}
		
		return false;
	}
}
