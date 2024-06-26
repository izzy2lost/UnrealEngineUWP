// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshClipMorphPlane.h"

#include "MuR/MutableMath.h"
#include "MuR/Skeleton.h"

namespace mu
{


    class NodeMeshClipMorphPlane::Private : public NodeMesh::Private
	{
	public:

		Private()
			: m_dist(0.0f), m_factor(0.0f)
			, m_radius1(0.0f), m_radius2(0.0f), m_rotation(0.0f)
			, m_vertexSelectionType(VS_ALL)
			, m_maxEffectRadius(-1.f)
		{
		}

		static FNodeType s_type;

		NodeMeshPtr m_pSource;

		// Morph field parameters

		//! Distance to the plane of last affected vertex
		float m_dist;

		//! "Linearity" factor of the influence.
		float m_factor;

		// Ellipse location
		FVector3f m_origin;
		FVector3f m_normal;
		float m_radius1, m_radius2, m_rotation;

		//! Typed of vertex selection
		typedef enum
		{
			//! All vertices, so no extra info is needed
			VS_ALL,

			//! Select vertices inside a shape
			VS_SHAPE,

			//! Select all vertices affected by any bone in a sub hierarchy
			VS_BONE_HIERARCHY,

		} VERTEX_SELECTION;

		// Vertex selection box 
        uint8 m_vertexSelectionType;
		FVector3f m_selectionBoxOrigin;
		FVector3f m_selectionBoxRadius;
		FBoneName m_vertexSelectionBone;

		TArray<FString> Tags;

		// Max distance a vertex can have to the bone in order to be affected. A negative value
		// means no limit.
		float m_maxEffectRadius;

	};


}
