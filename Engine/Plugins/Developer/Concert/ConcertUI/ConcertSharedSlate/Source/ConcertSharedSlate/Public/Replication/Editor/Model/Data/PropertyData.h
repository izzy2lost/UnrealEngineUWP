// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ConcertPropertySelection.h"
#include "UObject/SoftObjectPtr.h"

namespace UE::ConcertSharedSlate
{
	/** Instanced for each property row in IPropertyTreeView.*/
	class FPropertyData
	{
	public:
		
		FPropertyData(FSoftClassPath OwningClass, FConcertPropertyChain Object)
			: OwningClassPtr(MoveTemp(OwningClass))
			, Property(MoveTemp(Object))
		{}
		
		const FConcertPropertyChain& GetProperty() const { return Property; }
		const TSoftClassPtr<>& GetOwningClassPtr() const { return OwningClassPtr; }

	private:

		/** The class with which the FProperty can be determined. */
		TSoftClassPtr<> OwningClassPtr;
		
		/**
		 * The property to be replicated.
		 *
		 * On the servers, this will usually not resolve to anything.
		 * This was promoted to be TSoftObjectPtr so that any UI that resolves this object path automatically caches it.
		 * In certain operations this can improve performance: e.g. when fully rebuilding the property tree, this saved 35% performance.
		 */
		FConcertPropertyChain Property;
	};
}