// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ExtensionData.h"
#include "MuT/NodeExtensionDataConstant.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodePrivate.h"


namespace mu
{

class NodeExtensionDataConstant::Private : public Node::Private
{
public:

	static FNodeType s_type;

	ExtensionDataPtrConst Value;
	
};

}
