// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertySourceModel.h"

namespace UE::ConcertSharedSlate
{
	/** The allowed properties are those returned by UE::ConcertSyncCore::ForEachReplicatableProperty.*/
	class CONCERTSHAREDSLATE_API FConcertSyncCoreReplicatedPropertySource : public IPropertySourceModel
	{
	public:

		void SetClass(UClass* InClass);
		
		//~ Begin FSimplePropertyIterationSource Interface
		virtual ConcertSharedSlate::FSourceDisplayInfo GetDisplayInfo() const override;
		virtual uint32 GetNumSelectableItems() const override { return NumProperties; }
		virtual void EnumerateSelectableItems(TFunctionRef<EBreakBehavior(const FSelectablePropertyInfo& SelectableOption)> Delegate) const override;
		//~ End FSimplePropertyIterationSource Interface

	private:

		/** Weak ptr because it could be a Blueprint class that can be destroyed at any time. */
		TWeakObjectPtr<UClass> Class;

		int32 NumProperties = 0;
	};
}
