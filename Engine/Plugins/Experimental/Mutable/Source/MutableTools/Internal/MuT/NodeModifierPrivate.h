// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeModifier.h"

namespace mu
{

	MUTABLE_DEFINE_ENUM_SERIALISABLE(EMutableMultipleTagPolicy);

	class NodeModifier::Private : public Node::Private
	{
	public:

		/** Tags that target surface need to have enabled to receive this modifier. */
		TArray<FString> RequiredTags;

		/** In case of multiple tags in RequiredTags: are they all required, or one is enough? */
		EMutableMultipleTagPolicy MultipleTagsPolicy = EMutableMultipleTagPolicy::OnlyOneRequired;

		// Wether the modifier has to be applied after the normal node operations or before
		bool bApplyBeforeNormalOperations = true;

	};

}
