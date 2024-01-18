// Copyright Epic Games, Inc. All Rights Reserved.
#include "MixerMeshComponent.h"
#include "Mix.h"

void UMixerMeshComponent::SetMesh(RenderMeshPtr mesh)
{
	MeshObj = mesh;
	verify(MeshObj);
}


