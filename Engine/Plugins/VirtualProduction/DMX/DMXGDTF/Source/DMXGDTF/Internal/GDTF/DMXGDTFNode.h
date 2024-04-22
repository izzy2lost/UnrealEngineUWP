// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class FXmlNode;

namespace UE::DMX::GDTF
{
	class FDMXGDTFFixtureType;

	/**
	 * Base class for GDTF nodes.
	 *
	 * The tree of GDTF nodes is constructed by calling InitializeFromXmlNode from its root. Use DMXGDTFNodeParser to parse attributes and create children.
	 */
	class DMXGDTF_API FDMXGDTFNode
		: public TSharedFromThis<FDMXGDTFNode>
	{
		// Allow node initializer to set the fixture type
		template <typename NodeType> friend class FDMXGDTFNodeInitializer;

	public:
		virtual ~FDMXGDTFNode() {}

		/** Gets the Xml Tag corresponding to this node */
		virtual const TCHAR* GetXmlTag() const = 0;

		/** Initializes the node from an Xml node. Called after the node was constructed. */
		virtual void Initialize(const FXmlNode& InXmlNode) = 0;

		/** Returns the fixture type this node resides in */
		TWeakPtr<FDMXGDTFFixtureType> GetFixtureType() const { return WeakFixtureType; }

	private:
		/** The fixture type this node resides in */
		TWeakPtr<FDMXGDTFFixtureType> WeakFixtureType;
	};
}
