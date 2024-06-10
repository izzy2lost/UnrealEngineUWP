// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	// Forward definitions
    class NodeSurface;
    typedef Ptr<NodeSurface> NodeSurfacePtr;
    typedef Ptr<const NodeSurface> NodeSurfacePtrConst;


    //! This class is the parent of all nodes that output a Surface.
    class MUTABLETOOLS_API NodeSurface : public Node
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();


	protected:

		//! Forbidden. Manage with the Ptr<> template.
        inline ~NodeSurface() {}

	};


}
