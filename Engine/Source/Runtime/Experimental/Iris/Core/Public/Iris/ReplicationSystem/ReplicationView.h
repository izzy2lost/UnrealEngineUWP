// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Math/Vector.h"

// Define this macro in your game's Target.cs and set it to the maximum clients per connection your game can have (ex: splitscreen).
#ifndef UE_IRIS_INLINE_VIEWS_PER_CONNECTION
	#define UE_IRIS_INLINE_VIEWS_PER_CONNECTION 4
#endif

namespace UE::Net
{

struct FReplicationView
{
	struct FView
	{
		FVector Pos;
		FVector Dir;
		float FoVRadians;
	};

	TArray<FView, TInlineAllocator<UE_IRIS_INLINE_VIEWS_PER_CONNECTION>> Views;
};

}
