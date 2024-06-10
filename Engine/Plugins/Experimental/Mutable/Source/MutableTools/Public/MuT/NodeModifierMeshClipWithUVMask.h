// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeModifier.h"


namespace mu
{

	// Forward definitions
	class NodeImage;

	//! This node makes a new component from several meshes and images.
	//! \ingroup model
	class MUTABLETOOLS_API NodeModifierMeshClipWithUVMask : public NodeModifier
	{
	public:

		NodeModifierMeshClipWithUVMask();

		//-----------------------------------------------------------------------------------------
        // Node interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
        // Own interface
		//-----------------------------------------------------------------------------------------

		/** Set the expression generating the image witht he UV mask use to clip the mesh. */
		void SetClipMask(NodeImage*);

		/** Set the UV channel index for the UVs to check against the mask. */
		void SetLayoutIndex(uint8 LayoutIndex);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeModifierMeshClipWithUVMask();

	private:

		Private* m_pD;

	};


}
