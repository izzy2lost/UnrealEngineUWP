// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshFormat.h"

namespace mu
{


	class NodeMeshFormat::Private : public NodeMesh::Private
	{
	public:

		static FNodeType s_type;

		//! Source mesh to transform
		Ptr<NodeMesh> Source;

		/** New mesh format.The buffers in the sets have no elements, but they define the formats. */
		FMeshBufferSet VertexBuffers;
		FMeshBufferSet IndexBuffers;
		FMeshBufferSet FaceBuffers;
		
		/** */
		bool bOptimizeBuffers = false;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 5;
			arch << ver;

			arch << Source;
			arch << VertexBuffers;
			arch << IndexBuffers;
			arch << FaceBuffers;
			arch << bOptimizeBuffers;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver>=3 && ver<=5);

			arch >> Source;
			arch >> VertexBuffers;
			arch >> IndexBuffers;
			arch >> FaceBuffers;
			if (ver == 3)
			{
				bool bDummy;
				arch >> bDummy;
			}

			if (ver >= 5)
			{
				arch >> bOptimizeBuffers;
			}
		}

		// NodeMesh::Private interface
        Ptr<NodeLayout> GetLayout( int32 index ) const override;
	};


}
