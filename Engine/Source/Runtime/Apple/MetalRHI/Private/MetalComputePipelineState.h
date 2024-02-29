// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalComputePipelineState.h: Metal RHI compute pipeline state class.
=============================================================================*/

#pragma once

class FMetalComputePipelineState : public FRHIComputePipelineState
{
public:
	FMetalComputePipelineState(FMetalComputeShader* InComputeShader)
		: ComputeShader(InComputeShader)
	{
		check(InComputeShader);
	}

	FMetalComputeShader* GetComputeShader()
	{
		return ComputeShader;
	}

	TRefCountPtr<FMetalComputeShader> ComputeShader;
};
