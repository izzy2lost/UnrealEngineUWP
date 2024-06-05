// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/EBreakBehavior.h"
#include "Replication/Data/ConcertPropertySelection.h"

#include "Templates/FunctionFwd.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"

namespace UE::ConcertSharedSlate
{
	/** Wraps FConcertPropertyChain so it is easier to potentially change what IPropertySource lists in the future. */
	struct FPropertyInfo
	{
		FConcertPropertyChain Property;
		explicit FPropertyInfo(FConcertPropertyChain Property) : Property(MoveTemp(Property)) {}
	};
	
	/** Lists out a bunch of properties. */
	class IPropertySource
	{
	public:

		/** Lists a bunch of properties. */
		virtual void EnumerateProperties(TFunctionRef<EBreakBehavior(FPropertyInfo&& Property)> Delegate) const = 0;
		
		virtual ~IPropertySource() = default;
	};
}
