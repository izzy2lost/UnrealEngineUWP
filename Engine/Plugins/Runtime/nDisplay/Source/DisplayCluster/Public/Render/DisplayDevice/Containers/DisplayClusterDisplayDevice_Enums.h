// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
* Display Device materials
*/
enum class EDisplayClusterDisplayDeviceMaterialType : uint8
{
	// Default material used on preview mesh when preview is disabled
	DefaultPreviewMeshMaterial = 0,

	// Preview on mesh
	PreviewMeshMaterial,

	// Preview on mesh for techvis
	PreviewMeshTechvisMaterial,
};

/**
* Display Device mesh type
*/
enum class EDisplayClusterDisplayDeviceMeshType : uint8
{
	// Preview mesh
	PreviewMesh = 0,

	// Preview editable mesh
	PreviewEditableMesh,
};
