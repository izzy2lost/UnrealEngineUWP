// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

namespace UE::DMX
{
	struct FDMXMonitoredOutboundDMXData;

	/** An item in the pixel mapping conflict list */
	class FDMXConflictMonitorConflictModel
		: public TSharedFromThis<FDMXConflictMonitorConflictModel>
	{
		friend class SharedPointerInternals::TIntrusiveReferenceController<FDMXConflictMonitorConflictModel, ESPMode::ThreadSafe>;

	public:
		FDMXConflictMonitorConflictModel(const TArray<TSharedRef<FDMXMonitoredOutboundDMXData>>& InConflicts);

		FString GetConflictAsString() const;

	private:
		/** Parses the conflict as string, stores it in Title and Details members */
		void ParseConflict();

		FString Title;
		TArray<FString> Details;

		/** Returns the name of the first port in which a conflict occurs */
		FString GetPortNameText() const;

		/** Returns the universe text */
		FString GetUniverseText() const;

		/** Returns the channels text */
		FString GetChannelsText() const;

		/** Applies the style to the string */
		[[nodiscard]] FString StyleString(FString String, FString MarkupString) const;

		FDMXConflictMonitorConflictModel() = default;

		const TArray<TSharedRef<FDMXMonitoredOutboundDMXData>> Conflicts;
	};
}
