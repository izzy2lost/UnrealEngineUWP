// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/PCGMeshSampler.h"

struct FPCGContext;
class UGeometryScriptDebug;

namespace PCGGeometryHelpers
{
	PCGGEOMETRYSCRIPTINTEROP_API void GeometryScriptDebugToPCGLog(FPCGContext* Context, const UGeometryScriptDebug* Debug);
}
