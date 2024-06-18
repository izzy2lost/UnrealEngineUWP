// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeSurfaceNew.h"

#include "MuT/NodeMesh.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeString.h"
#include "MuT/NodeColour.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class NodeSurfaceNew::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		FString m_name;
        uint32 ExternalId =0;
        int32 SharedSurfaceId =INDEX_NONE;

		Ptr<NodeMesh> Mesh;

		struct FImageData
		{
			FString m_name;
			FString m_materialName;
			FString m_materialParameterName;
			NodeImagePtr m_pImage;

			// It could be negative, to indicate no layout.
            int8 m_layoutIndex = 0;
        };

		TArray<FImageData> m_images;

		struct FVectorData
		{
			FString m_name;
			NodeColourPtr m_pVector;
		};

		TArray<FVectorData> m_vectors;

        struct FScalar
        {
			FString m_name;
            NodeScalarPtr m_pScalar;
        };

        TArray<FScalar> m_scalars;

        struct FStringData
        {
			FString m_name;
            NodeStringPtr m_pString;
        };

		TArray<FStringData> m_strings;

        //! Tags in this surface
		TArray<FString> m_tags;

		//! Find an image node index by name or return -1
		int32 FindImage( const FString& strName ) const;

        //! Find a vector node index by name or return -1
        int32 FindVector(const FString& strName) const;

        //! Find a scalar node index by name or return -1
        int32 FindScalar( const FString& strName ) const;

        //! Find a string node index by name or return -1
        int32 FindString( const FString& strName ) const;
    };




}

