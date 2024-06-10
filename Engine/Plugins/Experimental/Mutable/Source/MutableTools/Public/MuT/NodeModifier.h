// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
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

	//! This class is the parent of all nodes that output a component.
	//! \ingroup model
	class MUTABLETOOLS_API NodeModifier : public Node
	{
	public:

		//-----------------------------------------------------------------------------------------
        // Node interface
		//-----------------------------------------------------------------------------------------

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

        //-----------------------------------------------------------------------------------------
        // Own interface
        //-----------------------------------------------------------------------------------------

        /** Add a tag to the surface, which will be affected by modifier nodes with the same tag. */
        void AddTag(const FString& TagName);

		/** Set the policy to interprete the tags when there is more than one. */
		void SetMultipleTagPolicy(EMutableMultipleTagPolicy);

		/** Set the stage to apply this modifier in.Default is before normal operations. */
		void SetStage( bool bBeforeNormalOperation );

		/** Tags that target surface need to have enabled to receive this modifier. */
		TArray<FString> RequiredTags;

		/** In case of multiple tags in RequiredTags: are they all required, or one is enough? */
		EMutableMultipleTagPolicy MultipleTagsPolicy = EMutableMultipleTagPolicy::OnlyOneRequired;

		// Wether the modifier has to be applied after the normal node operations or before
		bool bApplyBeforeNormalOperations = true;

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		inline ~NodeModifier() {}

	};


}
