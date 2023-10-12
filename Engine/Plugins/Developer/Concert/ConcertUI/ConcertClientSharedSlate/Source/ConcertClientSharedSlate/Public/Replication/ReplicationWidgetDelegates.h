// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"

class FMenuBuilder;

namespace UE::ConcertClientSharedSlate
{
	class FReplicatedPropertyData;

	/** A predicate for determining Left < Right. */
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FSortPropertyPredicate, const FReplicatedPropertyData& Left, const FReplicatedPropertyData& Right);

	/** Extends entries already added to a FMenuBuilder */
	DECLARE_DELEGATE_OneParam(FExtendMenu, FMenuBuilder&);
}