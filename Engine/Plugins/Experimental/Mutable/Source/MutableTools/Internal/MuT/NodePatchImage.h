// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Image.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{

    // Forward definitions
    class NodeImage;
 

    //! Node that allows to modify an image from an object by blending other images on specific
    //! layout blocks.
	class MUTABLETOOLS_API NodePatchImage : public Node
    {
    public:

        NodePatchImage();


        //-----------------------------------------------------------------------------------------
        // Node Interface
        //-----------------------------------------------------------------------------------------

        const FNodeType* GetType() const override;
        static const FNodeType* GetStaticType();

        //-----------------------------------------------------------------------------------------
        // Own Interface
        //-----------------------------------------------------------------------------------------

        //! Set the image that will be blended on the destination.
        void SetImage(Ptr<NodeImage>);

        //! Get the image that will be blended on the destination.
		Ptr<NodeImage> GetImage() const;

        //! Set the blending mask image.
        void SetMask(Ptr<NodeImage>);

        //! Get the blending mask image.
		Ptr<NodeImage> GetMask() const;

        //! Set the number of blocks in the layout that will be patched
        void SetBlockCount( int32 );

        //! Get the number of blocks in the layout that will be patched
        int32 GetBlockCount() const;

        //! Set a block of the layout that will be patched.
        //! \param index is the index in the patching list, from 0 to GetBlockCount()-1
        //! \param LayoutBlockIndex is the block index in the layout of the image were this patch operation will be applied.
        void SetBlock( int32 index, int32 LayoutBlockIndex);

        //! Get a block index of the layout that will be patched.
        //! \param index is the index in the patching list, from 0 to GetBlockCount()-1
        int32 GetBlock( int32 index ) const;

        //! Set the blending operation to use to combine the pixels
        EBlendType GetBlendType() const;
        void SetBlendType(EBlendType);

        //! Enable patching the alpha channel if present
        bool GetApplyToAlphaChannel() const;
        void SetApplyToAlphaChannel( bool );


        //-----------------------------------------------------------------------------------------
        // Interface pattern
        //-----------------------------------------------------------------------------------------
        class Private;
        Private* GetPrivate() const;

    protected:

        //! Forbidden. Manage with the Ptr<> template.
        ~NodePatchImage();

    private:

        Private* m_pD;

    };



}
