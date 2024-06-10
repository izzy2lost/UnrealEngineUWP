// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeModifier.h"


namespace mu
{

	// Forward definitions
	class NodeMesh;


	//! This node makes a new component from several meshes and images.
	//! \ingroup model
	class MUTABLETOOLS_API NodeModifierMeshClipWithMesh : public NodeModifier
	{
	public:

		NodeModifierMeshClipWithMesh();

		//-----------------------------------------------------------------------------------------
        // Node interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
        // Own interface
		//-----------------------------------------------------------------------------------------

		void SetClipMesh(NodeMesh*);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeModifierMeshClipWithMesh();

	private:

		Private* m_pD;

	};


}
