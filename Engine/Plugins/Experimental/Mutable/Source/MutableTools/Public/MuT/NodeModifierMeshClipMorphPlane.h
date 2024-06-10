// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeModifier.h"


namespace mu
{

	// Forward definitions
	class NodeModifierMeshClipMorphPlane;
	typedef Ptr<NodeModifierMeshClipMorphPlane> NodeModifierMeshClipMorphPlanePtr;
	typedef Ptr<const NodeModifierMeshClipMorphPlane> NodeModifierMeshClipMorphPlanePtrConst;

	struct FBoneName;

	//! This node makes a new component from several meshes and images.
	//! \ingroup model
	class MUTABLETOOLS_API NodeModifierMeshClipMorphPlane : public NodeModifier
	{
	public:

		NodeModifierMeshClipMorphPlane();

		//-----------------------------------------------------------------------------------------
        // Node interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
        // Own interface
		//-----------------------------------------------------------------------------------------

		void SetPlane(float centerX, float centerY, float centerZ, float normalX, float normalY, float normalZ);
		void SetParams(float dist, float factor);
		void SetMorphEllipse(float radius1, float radius2, float rotation);

		//! Define an axis-aligned box that will select the vertices to be morphed.
		//! Only one of Box or Bone Hierarchy can be used (the last one set)
		void SetVertexSelectionBox(float centerX, float centerY, float centerZ, float radiusX, float radiusY, float radiusZ);

		//! Define the root bone of the subhierarchy of the mesh that will be affected.
		//! Only one of Box or Bone Hierarchy can be used (the last one set)
		void SetVertexSelectionBone(const FBoneName& BoneId, float maxEffectRadius);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeModifierMeshClipMorphPlane();

	private:

		Private* m_pD;

	};


}
