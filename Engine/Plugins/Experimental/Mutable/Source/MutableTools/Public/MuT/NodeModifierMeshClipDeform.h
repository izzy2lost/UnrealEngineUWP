// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeModifier.h"

namespace mu
{

	// Forward definitions
	class NodeModifierMeshClipDeform;
	typedef Ptr<NodeModifierMeshClipDeform> NodeModifierMeshClipDeformPtr;
	typedef Ptr<const NodeModifierMeshClipDeform> NodeModifierMeshClipDeformPtrConst;

	class NodeMesh;
	typedef Ptr<NodeMesh> NodeMeshPtr;
	typedef Ptr<const NodeMesh> NodeMeshPtrConst;

	enum class EShapeBindingMethod : uint32;
	
	//! This node makes a new component from several meshes and images.
	//! \ingroup model
	class MUTABLETOOLS_API NodeModifierMeshClipDeform : public NodeModifier
	{
	public:

		NodeModifierMeshClipDeform();

		//-----------------------------------------------------------------------------------------
        // Node interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
        // Own interface
		//-----------------------------------------------------------------------------------------

		void SetClipMesh( NodeMesh* InClipMesh);
		void SetBindingMethod(EShapeBindingMethod BindingMethod);
	
		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeModifierMeshClipDeform();

	private:

		Private* m_pD;

	};


}
