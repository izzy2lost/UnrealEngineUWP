// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuT/Node.h"
#include "Containers/UnrealString.h"

#include "NodeModifier.generated.h"


/** Despite being an UEnum, this is not always version-serialized (in MutableTools).
* Beware of changing the enum options or order.
*/
UENUM()
enum class EMutableMultipleTagPolicy : uint8
{
	OnlyOneRequired,
	AllRequired
};


namespace mu
{

	// Forward definitions
	class NodeModifier;
	typedef Ptr<NodeModifier> NodeModifierPtr;
	typedef Ptr<const NodeModifier> NodeModifierConst;

	/** Parent of all node classes that apply modifiers to surfaces. */
	class MUTABLETOOLS_API NodeModifier : public Node
	{
	public:

		/** Tags that target surface need to have enabled to receive this modifier. */
		TArray<FString> RequiredTags;

		/** In case of multiple tags in RequiredTags: are they all required, or one is enough? */
		EMutableMultipleTagPolicy MultipleTagsPolicy = EMutableMultipleTagPolicy::OnlyOneRequired;

		// Wether the modifier has to be applied after the normal node operations or before
		bool bApplyBeforeNormalOperations = true;


	public:

		// Node interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		inline ~NodeModifier() {}

	private:

		static FNodeType StaticType;

	};


}
