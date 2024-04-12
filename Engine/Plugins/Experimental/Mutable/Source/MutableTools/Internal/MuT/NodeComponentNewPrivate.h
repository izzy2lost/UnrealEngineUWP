// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once


#include "MuT/NodeComponentPrivate.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodePatchImagePrivate.h"

#include "MuT/NodeMesh.h"
#include "MuT/NodePatchMesh.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeSurface.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class NodeComponentNew::Private : public NodeComponent::Private
	{
	public:

		static FNodeType s_type;

		FString m_name;

		uint16 m_id = 0;

        TArray<NodeSurfacePtr> m_surfaces;

		// NodeComponent::Private interface
        const NodeComponentNew::Private* GetParentComponentNew() const override
		{
			return this;
		}

	};

}

