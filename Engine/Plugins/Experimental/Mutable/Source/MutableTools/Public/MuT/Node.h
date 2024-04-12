// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/RefCounted.h"


namespace mu
{
	//! \defgroup tools Tools library classes
	//! Tools library classes.

	//! \defgroup model Model nodes
	//! \ingroup tools
	//! This group contains the nodes that can be used to compose models.

	//! \defgroup transform Transform nodes
	//! \ingroup tools
	//! This group contains the nodes that can be used to compose model transformations.


	// Forward declarations
	class Node;
	typedef Ptr<Node> NodePtr;
	typedef Ptr<const Node> NodePtrConst;

	class NodeMap;
	typedef Ptr<NodeMap> NodeMapPtr;
	typedef Ptr<const NodeMap> NodeMapPtrConst;


	/** Information about the type of a node, to provide some means to the tools to deal generically with nodes. */
	struct FNodeType
	{
		FNodeType();
		FNodeType( const char* strName, const FNodeType* pParent );

		const char* m_strName;
		const FNodeType* m_pParent;

		inline bool IsA(const FNodeType* CandidateType) const
		{
			if (CandidateType == this)
			{
				return true;
			}

			if (m_pParent)
			{
				return m_pParent->IsA(CandidateType);
			}

			return false;
		}
	};


    //! %Base class for all graphs used in the source data to define models and transforms.
	class MUTABLETOOLS_API Node : public RefCounted
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			Colour = 0,
			Component = 1,
			Image = 2,
			Layout = 3,
			LOD = 4,
			Mesh = 5,
			Object = 6,
			PatchImage = 8,
			Scalar = 9,
			PatchMesh = 13,
			Volume_Deprecated = 14,
			Projector = 15,
			Surface = 16,
			Modifier = 18,
			Range = 19,
			String = 20,
			Bool = 21,
			ExtensionData = 22,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		/** Node type hierarchy data. */
        virtual const FNodeType* GetType() const;
		static const FNodeType* GetStaticType();

		/** Set the opaque context returned in messages in the compiler log. */
		void SetMessageContext( const void* context );

		//-----------------------------------------------------------------------------------------
        // Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		virtual Private* GetBasePrivate() const = 0;

	protected:

		inline ~Node() {}

		//!
		EType Type = EType::None;

	};


}
