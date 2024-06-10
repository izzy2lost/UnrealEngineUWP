// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourConstant.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MutableMath.h"
#include "MuT/NodeColourConstantPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	FNodeType NodeColourConstant::Private::s_type = FNodeType(Node::EType::ColorConstant, NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourConstant)



	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	FVector4f NodeColourConstant::GetValue() const
	{
		return m_pD->m_value;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourConstant::SetValue(FVector4f Value)
	{
		m_pD->m_value = Value;
	}


}

