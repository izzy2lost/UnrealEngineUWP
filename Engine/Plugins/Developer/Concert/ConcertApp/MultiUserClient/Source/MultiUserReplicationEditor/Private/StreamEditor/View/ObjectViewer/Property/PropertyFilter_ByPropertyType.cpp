// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyFilter_ByPropertyType.h"

#include "Replication/PropertyChainUtils.h"
#include "StreamEditor/View/ObjectViewer/ReplicatedPropertyData.h"

#include "UObject/UnrealType.h"

namespace UE::MultiUserReplicationEditor
{
	bool FPropertyFilter_ByPropertyType::MatchesFilteredForProperty(const FReplicatedPropertyData& InItem) const
	{
		UClass* Class = InItem.GetOwningClass().TryLoadClass<UObject>();
		if (!Class)
		{
			return false;
		}

		const FProperty* Property = ConcertSyncCore::PropertyChain::ResolveProperty(*Class, InItem.GetProperty());
		return Property && AllowedClasses.Contains(Property->GetClass());
	}
}
