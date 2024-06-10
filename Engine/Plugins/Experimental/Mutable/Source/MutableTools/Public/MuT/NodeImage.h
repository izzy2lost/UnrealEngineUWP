// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"

namespace mu
{

	// Forward definitions
	class NodeImage;
	typedef Ptr<NodeImage> NodeImagePtr;
	typedef Ptr<const NodeImage> NodeImagePtrConst;


    //! %Base class of any node that outputs an image.
	//! \ingroup model
	class MUTABLETOOLS_API NodeImage : public Node
	{
	public:

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
		inline ~NodeImage() {}

	};



}
