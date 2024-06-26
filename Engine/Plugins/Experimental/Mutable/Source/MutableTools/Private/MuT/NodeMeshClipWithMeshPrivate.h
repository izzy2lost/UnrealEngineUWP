// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeMeshPrivate.h"
#include "MuT/NodeMeshClipWithMesh.h"

#include "MuR/MutableMath.h"


namespace mu
{


    class NodeMeshClipWithMesh::Private : public NodeMesh::Private
	{
	public:

		static FNodeType s_type;

		NodeMeshPtr m_pSource;
		NodeMeshPtr m_pClipMesh;

		TArray<FString> Tags;

	};


}
