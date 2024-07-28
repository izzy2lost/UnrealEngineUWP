// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Templates/SharedPointer.h"

#include "TedsOutlinerColumns.generated.h"

class ISceneOutliner;

// Column used to store a reference to the table viewer owning a specific row
// Currently only added to widget rows in a table viewer
USTRUCT(meta = (DisplayName = "Owning Table Viewer"))
struct FTableViewerColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	// The table viewer is internally a scene outliner
	TWeakPtr<ISceneOutliner> Outliner;
};
