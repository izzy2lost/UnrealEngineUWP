// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/Node.h"
#include "MuT/NodePrivate.h"
#include "Misc/AssertionMacros.h"

#include "MuT/NodeComponent.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeExtensionData.h"
#include "MuT/NodeExtensionDataConstant.h"

namespace mu
{

	FNodeType NodeComponent::StaticType = FNodeType(Node::EType::Component, Node::GetStaticType());
	FNodeType NodeComponentNew::StaticType = FNodeType(Node::EType::ComponentNew, NodeComponent::GetStaticType());
	FNodeType NodeLOD::StaticType = FNodeType(Node::EType::LOD, Node::GetStaticType());

	FNodeType NodeExtensionData::StaticType = FNodeType(Node::EType::ExtensionData, Node::GetStaticType());
	FNodeType NodeExtensionDataConstant::StaticType = FNodeType(Node::EType::ExtensionDataConstant, NodeExtensionData::GetStaticType());

	// Static initialisation
	static FNodeType s_nodeType = FNodeType(Node::EType::Node, nullptr );

	FNodeType::FNodeType()
	{
		Type = Node::EType::None;
		m_pParent = nullptr;
	}


	FNodeType::FNodeType(Node::EType InType, const FNodeType* pParent )
	{
		Type = InType;
		m_pParent = pParent;
	}


	const FNodeType* Node::GetType() const
	{
		return GetStaticType();
	}


	const FNodeType* Node::GetStaticType()
	{
		return &s_nodeType;
	}


	void Node::SetMessageContext( const void* context )
	{
		MessageContext = context;
	}

	const void* Node::GetMessageContext() const 
	{ 
		return MessageContext; 
	}

}


