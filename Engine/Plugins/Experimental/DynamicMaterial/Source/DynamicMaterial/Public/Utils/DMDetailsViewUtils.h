// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "Containers/Array.h"
#include "Templates/SharedPointerFwd.h"
#include "UObject/UnrealNames.h"

class IDetailTreeNode;

struct FDMDetailsViewUtils
{
	static DYNAMICMATERIAL_API TSharedPtr<IDetailTreeNode> SearchNodesForProperty(const TArray<TSharedRef<IDetailTreeNode>>& InNodes, FName InPropertyName);
};
#endif
