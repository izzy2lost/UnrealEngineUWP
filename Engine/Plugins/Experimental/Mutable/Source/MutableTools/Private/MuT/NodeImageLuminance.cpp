// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageLuminance.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageLuminancePrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageLuminance::Private::s_type =
			NODE_TYPE( "ImageLuminance", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageLuminance, EType::Luminance, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageLuminance::GetSource() const
	{
		return m_pD->m_pSource.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageLuminance::SetSource( NodeImagePtr pNode )
	{
		m_pD->m_pSource = pNode;
	}



}
