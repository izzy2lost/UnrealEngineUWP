// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeModifier.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeModifierPrivate.h"


namespace mu
{
	// Static initialisation
	static FNodeType s_nodeModifierType = FNodeType(Node::EType::Modifier, Node::GetStaticType() );

	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(EMutableMultipleTagPolicy);

	const FNodeType* NodeModifier::GetType() const
	{
		return GetStaticType();
	}


	const FNodeType* NodeModifier::GetStaticType()
	{
		return &s_nodeModifierType;
    }

	void NodeModifier::AddTag(const FString& Value)
	{
		RequiredTags.Add(Value);
	}


	void NodeModifier::SetMultipleTagPolicy(EMutableMultipleTagPolicy Value)
	{
		MultipleTagsPolicy = Value;
	}


	void NodeModifier::SetStage(bool bBeforeNormalOperation)
	{
		bApplyBeforeNormalOperations = bBeforeNormalOperation; 
	}

}


