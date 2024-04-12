// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeModifierPrivate.h"
#include "MuT/NodeModifierMeshClipWithMesh.h"
#include "MuT/NodeMesh.h"

#include "MuR/MutableMath.h"


namespace mu
{

    class NodeModifierMeshClipWithMesh::Private : public NodeModifier::Private
	{
	public:

		Private()
		{
		}

		static FNodeType s_type;

		//! 
		Ptr<NodeMesh> ClipMesh;

	};


}
