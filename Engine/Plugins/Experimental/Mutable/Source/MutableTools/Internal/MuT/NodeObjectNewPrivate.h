// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeObjectPrivate.h"

#include "MuT/NodeExtensionData.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodeLOD.h"
#include "MuT/Compiler.h"


namespace mu
{

	class NodeObjectNew::Private : public NodeObject::Private
	{
	public:

		static FNodeType s_type;

		FString m_name;
		FString m_uid;

		TArray<NodeLODPtr> m_lods;

		TArray<NodeObjectPtr> m_children;

		struct NamedExtensionDataNode
		{
			Ptr<NodeExtensionData> Node;
			FString Name;
		};

		TArray<NamedExtensionDataNode> m_extensionDataNodes;


		//! List of states
        TArray<FObjectState> m_states;


		//! Return true if the given component is set in any lod of this object.
        bool HasComponent( const NodeComponent* pComponent ) const;

        // NodeObject::Private interface
        NodeLayoutPtr GetLayout( int lod, int component, int surface, int texture ) const override;

	};

}
