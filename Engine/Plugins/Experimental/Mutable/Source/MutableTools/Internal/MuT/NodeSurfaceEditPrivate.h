// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeSurfaceEdit.h"

#include "MuT/NodePrivate.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodePatchImage.h"

namespace mu
{


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class NodeSurfaceEdit::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

        NodeSurfacePtr m_pParent;
		NodePatchMeshPtr m_pMesh;
		NodeMeshPtr m_pMorph;

        //! This flag indicates that the mesh in the m_pMorph connection is a target mesh, and not
        //! morph information. This means that the morph information needs to be generated.
        bool m_morphIsTarget = true;

		struct FTexture
		{
			NodeImagePtr m_pExtend;
			NodePatchImagePtr m_pPatch;
		};

		TArray<FTexture> m_textures;

        //! Tags in this surface edit
		TArray<FString> m_tags;

		//! Factor of the morph
		NodeScalarPtr m_pFactor;
	};

}

