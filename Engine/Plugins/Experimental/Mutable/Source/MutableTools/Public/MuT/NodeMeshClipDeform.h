// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	//! 
	class MUTABLETOOLS_API NodeMeshClipDeform : public NodeMesh
	{
	public:

		NodeMeshClipDeform();

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		const NodeMeshPtr& GetBaseMesh() const;
		void SetBaseMesh( const NodeMeshPtr& );

		const NodeMeshPtr& GetClipShape() const;
		void SetClipShape(const NodeMeshPtr&);

		const NodeImagePtr& GetShapeWeights() const;
		void SetShapeWeights(const NodeImagePtr&);


        //-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeMeshClipDeform();

	private:

		Private* m_pD;

	};

}
