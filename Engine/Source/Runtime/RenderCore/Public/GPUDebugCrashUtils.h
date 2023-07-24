// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUDebugCrashUtils.h: Utilities for crashing the GPU in various ways on purpose.
	=============================================================================*/
class FRDGBuilder;

extern RENDERCORE_API void ScheduleGPUDebugCrash(FRDGBuilder& GraphBuilder);
