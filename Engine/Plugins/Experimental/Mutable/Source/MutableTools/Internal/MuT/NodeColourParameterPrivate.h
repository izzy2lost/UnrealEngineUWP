// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourParameter.h"
#include "MuT/NodeColourPrivate.h"

#include "MuT/NodeRange.h"
#include "MuR/MutableMath.h"


namespace mu
{

	class NodeColourParameter::Private : public NodeColour::Private
	{
	public:

		static FNodeType s_type;

		FVector4f m_defaultValue;
		FString m_name;
		FString m_uid;

        TArray<Ptr<NodeRange>> m_ranges;
	};

}
