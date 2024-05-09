// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodePatchImage.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeImage.h"
#include "MuT/NodePatchImagePrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{


    //---------------------------------------------------------------------------------------------
    // Static initialisation
    //---------------------------------------------------------------------------------------------
    FNodeType NodePatchImage::Private::s_type = FNodeType( "PatchTexture", Node::GetStaticType() );


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodePatchImage, EType::PatchImage, Node, Node::EType::None)


    //---------------------------------------------------------------------------------------------
    // Own Interface
    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetImage( Ptr<NodeImage> pImage )
    {
        m_pD->m_pImage = pImage;
    }


    //---------------------------------------------------------------------------------------------
	Ptr<NodeImage> NodePatchImage::GetImage() const
    {
        return m_pD->m_pImage.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetMask(Ptr<NodeImage> pMask )
    {
        m_pD->m_pMask = pMask;
    }


    //---------------------------------------------------------------------------------------------
	Ptr<NodeImage> NodePatchImage::GetMask() const
    {
        return m_pD->m_pMask.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetBlockCount( int32 c )
    {
        m_pD->BlockIndices.SetNum( c );
    }


    //---------------------------------------------------------------------------------------------
    int32 NodePatchImage::GetBlockCount() const
    {
        return (int32)m_pD->BlockIndices.Num();
    }


    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetBlock( int32 index, int32 LayoutBlockIndex )
    {
        check( index>=0 && index<m_pD->BlockIndices.Num() );
        m_pD->BlockIndices[ index ] = LayoutBlockIndex;
    }


    //---------------------------------------------------------------------------------------------
    int32 NodePatchImage::GetBlock( int32 index ) const
    {
        check( index>=0 && index<m_pD->BlockIndices.Num() );
        return m_pD->BlockIndices[ index ];
    }


    //---------------------------------------------------------------------------------------------
	EBlendType NodePatchImage::GetBlendType() const
    {
        return m_pD->m_blendType;
    }


    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetBlendType(EBlendType InType)
    {
        m_pD->m_blendType = InType;
    }


    //---------------------------------------------------------------------------------------------
    bool NodePatchImage::GetApplyToAlphaChannel() const
    {
        return m_pD->m_applyToAlpha;
    }


    //---------------------------------------------------------------------------------------------
    void NodePatchImage::SetApplyToAlphaChannel(bool a )
    {
        m_pD->m_applyToAlpha = a;
    }

}
