// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Templates/Function.h"

class FMenuBuilder;
enum class EBreakBehavior : uint8;
struct FConcertPropertyChain;
struct FSoftClassPath;
struct FSoftObjectPath;

namespace UE::ConcertSharedSlate
{
	class FPropertyData;
	class IPropertySelectionSourceModel;
	class IReplicationStreamModel;

	/** A predicate for determining Left < Right. */
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FSortPropertyPredicate, const FPropertyData& Left, const FPropertyData& Right);

	/** Extends a context menu that is being built for a selection of objects. */
	DECLARE_DELEGATE_TwoParams(FExtendObjectMenu, FMenuBuilder&, TConstArrayView<FSoftObjectPath> ContextObjects);

	/** Delegate for getting an object's class. */
	DECLARE_DELEGATE_RetVal_OneParam(FSoftClassPath, FGetObjectClass, const FSoftObjectPath&);

	/** Delegate for deciding whether an object should be displayed. */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FShouldDisplayObject, const FSoftObjectPath& ObjectPath);
}