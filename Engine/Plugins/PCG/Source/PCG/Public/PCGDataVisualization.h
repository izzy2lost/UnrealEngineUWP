// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
class AActor;
class UPCGData;
struct FPCGContext;

/** Implement this interface to provide custom PCGData visualizations. Register your implementation to FPCGModule::FPCGDataVisualizationRegistry to be used automatically. */
class IPCGDataVisualization
{
public:
	virtual ~IPCGDataVisualization() = default;
	virtual void ExecuteDebugDisplay(FPCGContext* Context, const UPCGData* Data, AActor* TargetActor) const = 0;
};
#endif
