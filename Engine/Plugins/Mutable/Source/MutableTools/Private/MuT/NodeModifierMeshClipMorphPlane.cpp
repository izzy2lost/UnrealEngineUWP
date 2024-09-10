// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeModifierMeshClipMorphPlane.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MutableMath.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    void NodeModifierMeshClipMorphPlane::SetPlane(FVector3f Center, FVector3f Normal)
	{
		Parameters.Origin = Center;
		Parameters.Normal = Normal;
	}


	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipMorphPlane::SetParams(float dist, float factor)
	{
		Parameters.DistanceToPlane = dist;
		Parameters.LinearityFactor = factor;
	}

	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipMorphPlane::SetMorphEllipse(float radius1, float radius2, float rotation)
	{
		Parameters.Radius1 = radius1;
		Parameters.Radius2 = radius2;
		Parameters.Rotation = rotation;
	}

	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipMorphPlane::SetVertexSelectionBox(float centerX, float centerY, float centerZ, float radiusX, float radiusY, float radiusZ)
	{
		Parameters.VertexSelectionType = FClipMorphPlaneParameters::VS_SHAPE;
		Parameters.SelectionBoxOrigin = FVector3f(centerX, centerY, centerZ);
		Parameters.SelectionBoxRadius = FVector3f(radiusX, radiusY, radiusZ);
	}

	//---------------------------------------------------------------------------------------------
	void NodeModifierMeshClipMorphPlane::SetVertexSelectionBone(const FBoneName& BoneId, float maxEffectRadius)
	{
		Parameters.VertexSelectionType = FClipMorphPlaneParameters::VS_BONE_HIERARCHY;
		Parameters.VertexSelectionBone = BoneId;
		Parameters.MaxEffectRadius = maxEffectRadius;
	}

}

