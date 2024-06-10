// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeColour.h"


namespace mu
{

	class NodeColourConstant;
	typedef Ptr<NodeColourConstant> NodeColourConstantPtr;
	typedef Ptr<const NodeColourConstant> NodeColourConstantPtrConst;


	//! This node outputs a predefined colour value.
	//! \ingroup model
	class MUTABLETOOLS_API NodeColourConstant : public NodeColour
	{
	public:

		NodeColourConstant();

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the value that this node returns
		FVector4f GetValue() const;

		//! Set the value to be returned by this node
		void SetValue(FVector4f);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeColourConstant();

	private:

		Private* m_pD;

	};


}
