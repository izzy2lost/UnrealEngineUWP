// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;

namespace UE::ConcertClientSharedSlate
{
	class IReplicationStreamModel;
}

namespace UE::MultiUserClient
{
	class FGlobalAuthorityCache;

	/**
	 * Displays a text "Replicating x Objects for y Actors".
	 *
	 * This view sums up all objects being replicated and shows the distinct actors being replicated.
	 * Example: You are replicating AActor::ActorGuid and USceneComponent::RelativeLocation on an actor called Floor.
	 * Result: "Replicating 2 Objects for 1 Actor".
	 */
	class SReplicationStatus : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationStatus)
		{}
			/** The clients to show statistics for */
			SLATE_ATTRIBUTE(TSet<FGuid>, DisplayedClients)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const ConcertClientSharedSlate::IReplicationStreamModel& InObjectModel, FGlobalAuthorityCache& InAuthorityCache);
		virtual ~SReplicationStatus() override;

	private:

		/** Used to look up registered objects and subobjects. */
		const ConcertClientSharedSlate::IReplicationStreamModel* ObjectModel;
		/** Used to get authority state of objects and informs us when authority changes. */
		FGlobalAuthorityCache* AuthorityCache;

		/** The clients to show statistics for */
		TAttribute<TSet<FGuid>> DisplayedClientsAttribute;

		/** Updated when authority changes. Displays subobjects in bold. */
		TSharedPtr<STextBlock> ObjectsText;
		/** Updated when authority changes. Displays actors in bold. */
		TSharedPtr<STextBlock> ActorsText;

		void OnAuthorityCacheChanged(const FGuid& ClientId) { RefreshStatusText(); }
		void RefreshStatusText();
	};
}

