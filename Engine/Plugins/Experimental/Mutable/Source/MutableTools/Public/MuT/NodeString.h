// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	// Forward definitions
	class NodeString;
	typedef Ptr<NodeString> NodeStringPtr;
	typedef Ptr<const NodeString> NodeStringPtrConst;


    //! %Base class of any node that outputs a string value.
	class MUTABLETOOLS_API NodeString : public Node
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			Constant = 0,
			Parameter = 1,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();


		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		inline ~NodeString() {}

		//!
		EType Type = EType::None;


	};


}
