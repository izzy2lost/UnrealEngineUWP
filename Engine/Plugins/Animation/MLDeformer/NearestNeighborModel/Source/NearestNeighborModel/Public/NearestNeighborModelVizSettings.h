// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerMorphModelVizSettings.h"
#include "NearestNeighborModelVizSettings.generated.h"

class UGeometryCache;
namespace UE::NearestNeighborModel
{
	class FNearestNeighborModelVizSettingsDetails;
};

/**
 * The vizualization settings specific to the the vertex delta model.
 */
UCLASS()
class NEARESTNEIGHBORMODEL_API UNearestNeighborModelVizSettings 
	: public UMLDeformerMorphModelVizSettings
{
	GENERATED_BODY()
#if	WITH_EDITORONLY_DATA
public:
	static FName GetNearestNeighborActorsOffsetPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModelVizSettings, NearestNeighborActorsOffset); }
	static FName GetNearestNeighborIdsPropertyName() { return GET_MEMBER_NAME_CHECKED(UNearestNeighborModelVizSettings, NearestNeighborIds); }
	
	/** Whether to show verts */
	UPROPERTY(EditAnywhere, Category = "Training Meshes")
	bool bDrawVerts = false;

	/** Show vertices in this section */
	UPROPERTY(EditAnywhere, Category = "Training Meshes", Meta = (DisplayName = "Show Verts in", EditorCondition = "bDrawVerts"))
	int32 VertVizSectionIndex = INDEX_NONE;

	/** The section used to display the nearest neighbor. */	
	UPROPERTY(EditAnywhere, Category = "Live Settings", Meta = (DisplayName = "Actor Section Index"))
	int32 NearestNeighborActorSectionIndex = 0;
	
	/** The offset of the nearest neighbor actor from the mesh. */
	UPROPERTY(EditAnywhere, Category = "Live Settings", Meta = (DisplayName = "Actor Offset"))
	float NearestNeighborActorsOffset = 2.0f;
	
	UPROPERTY(VisibleAnywhere, Category = "Live Settings")
	TArray<int32> NearestNeighborIds;
#endif
};
