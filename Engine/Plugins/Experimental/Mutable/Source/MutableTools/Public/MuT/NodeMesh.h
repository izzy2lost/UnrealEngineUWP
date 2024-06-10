// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"

namespace mu
{

	// Forward definitions
	class NodeMesh;
	typedef Ptr<NodeMesh> NodeMeshPtr;
	typedef Ptr<const NodeMesh> NodeMeshPtrConst;


    //! %Base class of any node that outputs a mesh.
	class MUTABLETOOLS_API NodeMesh : public Node
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------
        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		inline ~NodeMesh() {}
	};


}

