// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/Node.h"
#include "MuT/NodePrivate.h"
#include "Misc/AssertionMacros.h"

#include "MuT/NodeComponent.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeExtensionData.h"
#include "MuT/NodeExtensionDataConstant.h"
#include "MuT/NodeImageTable.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeColourSwitch.h"
#include "MuT/NodeColourVariation.h"
#include "MuT/NodeColourConstant.h"
#include "MuT/NodeColourParameter.h"
#include "MuT/NodeColourSampleImage.h"
#include "MuT/NodeColourArithmeticOperation.h"
#include "MuT/NodeColourFromScalars.h"
#include "MuT/NodeColourTable.h"

namespace mu
{

	// Static initialisation
	FNodeType Node::StaticType = FNodeType(Node::EType::Node, nullptr);

	FNodeType NodeComponent::StaticType = FNodeType(Node::EType::Component, Node::GetStaticType());
	FNodeType NodeComponentNew::StaticType = FNodeType(Node::EType::ComponentNew, NodeComponent::GetStaticType());
	FNodeType NodeLOD::StaticType = FNodeType(Node::EType::LOD, Node::GetStaticType());
	FNodeType NodeExtensionData::StaticType = FNodeType(Node::EType::ExtensionData, Node::GetStaticType());
	FNodeType NodeExtensionDataConstant::StaticType = FNodeType(Node::EType::ExtensionDataConstant, NodeExtensionData::GetStaticType());
	FNodeType NodeImageTable::StaticType = FNodeType(Node::EType::ImageTable, NodeImage::GetStaticType());

	FNodeType NodeColour::StaticType = FNodeType(Node::EType::Color, Node::GetStaticType());
	FNodeType NodeColourConstant::StaticType = FNodeType(Node::EType::ColorConstant, NodeColour::GetStaticType());
	FNodeType NodeColourParameter::StaticType = FNodeType(Node::EType::ColorParameter, NodeColour::GetStaticType());
	FNodeType NodeColourSwitch::StaticType = FNodeType(Node::EType::ColorSwitch, NodeColour::GetStaticType());
	FNodeType NodeColourVariation::StaticType = FNodeType(Node::EType::ColorVariation, NodeColour::GetStaticType());
	FNodeType NodeColourTable::StaticType = FNodeType(Node::EType::ColorTable, NodeColour::GetStaticType());
	FNodeType NodeColourArithmeticOperation::StaticType = FNodeType(Node::EType::ColorArithmeticOperation, NodeColour::GetStaticType());
	FNodeType NodeColourSampleImage::StaticType = FNodeType(Node::EType::ColorSampleImage, NodeColour::GetStaticType());
	FNodeType NodeColourFromScalars::StaticType = FNodeType(Node::EType::ColorFromScalars, NodeColour::GetStaticType());


	FNodeType::FNodeType()
	{
		Type = Node::EType::None;
		Parent = nullptr;
	}


	FNodeType::FNodeType(Node::EType InType, const FNodeType* pParent )
	{
		Type = InType;
		Parent = pParent;
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


