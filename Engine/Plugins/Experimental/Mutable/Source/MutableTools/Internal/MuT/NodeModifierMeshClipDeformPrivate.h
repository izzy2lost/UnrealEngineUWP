// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Mesh.h"
#include "MuT/NodeModifierPrivate.h"
#include "MuT/NodeModifierMeshClipDeform.h"
#include "MuT/NodeMesh.h"

namespace mu
{


    class NodeModifierMeshClipDeform::Private : public NodeModifier::Private
    {
    public:

		Private()
		{
		}

		static FNodeType s_type;

		//! 
		NodeMeshPtr ClipMesh;
		EShapeBindingMethod BindingMethod = EShapeBindingMethod::ClipDeformClosestProject;

	};

}
