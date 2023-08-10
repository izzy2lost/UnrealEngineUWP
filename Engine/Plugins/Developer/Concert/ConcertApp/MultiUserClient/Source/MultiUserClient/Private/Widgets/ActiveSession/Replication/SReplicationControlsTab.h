// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SSplitter.h"

class SExpandableArea;

namespace UE::MultiUserClient
{
	class SReplicationControlsTab : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationControlsTab)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:

		/** In this area the user can control the replication connection, e.g. join and leave. */
		TSharedPtr<SExpandableArea> ConnectionArea;
		/** In this area the user can view and edit stream attributes, which will be replicated back to the server. */
		TSharedPtr<SExpandableArea> ClientAttributesArea;
		/** In this area the user can view the replication streams as well as toggle sent objects and its properties. */
		TSharedPtr<SExpandableArea> StreamsArea;

		bool bConnectionAreaExpanded = false;
		bool bAttributesAreaExpanded = false;
		bool bStreamsAreaExpanded = false;

		/** Handles how much space the 'ConnectionArea' area uses with respect to its expansion state. */
		SSplitter::ESizeRule GetConnectionAreaSizeRule() const { return SSplitter::ESizeRule::SizeToContent; }
		void OnConnectionAreaExpansionChanged(bool bExpanded) { bConnectionAreaExpanded = bExpanded; }

		/** Handles how much space the 'ClientAttributesArea' area uses with respect to its expansion state. */
		SSplitter::ESizeRule GetClientAttributesAreaSizeRule() const { return bAttributesAreaExpanded ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent; }
		void OnAClientsttributesAreaExpansionChanged(bool bExpanded) { bAttributesAreaExpanded = bExpanded; }
		
		/** Handles how much space the 'StreamsArea' area uses with respect to its expansion state. */
		SSplitter::ESizeRule GetStreamsAreaSizeRule() const { return bStreamsAreaExpanded ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent; }
		void OnStreamsAreaExpansionChanged(bool bExpanded) { bStreamsAreaExpanded = bExpanded; }
	};
}

