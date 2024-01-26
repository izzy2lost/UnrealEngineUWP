// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Templates/SharedPointerFwd.h"

/** Common interface for treeview nodes. */
class IGMETreeNodeViewModel
{
protected:
	~IGMETreeNodeViewModel() = default;

public:
	/** See STreeView::OnGetChildren, return false if no children. */
	virtual bool GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren) = 0;
};
