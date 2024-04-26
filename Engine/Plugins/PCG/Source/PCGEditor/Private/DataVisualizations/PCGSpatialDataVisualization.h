// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGDataVisualization.h"
#include "PCGEditorModule.h"

class AActor;
class UPCGData;
class UPCGPointData;
struct FPCGContext;

/** Default implementation for spatial data. Collapses to a PointData representation. */
class PCGEDITOR_API IPCGSpatialDataVisualization : public IPCGDataVisualization
{
public:
	virtual void ExecuteDebugDisplay(FPCGContext* Context, const UPCGData* Data, AActor* TargetActor) const override;
	virtual const UPCGPointData* CollapseToDebugPointData(FPCGContext* Context, const UPCGData* Data) const;
};
