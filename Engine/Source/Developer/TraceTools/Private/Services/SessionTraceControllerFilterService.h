// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISessionTraceFilterService.h"
#include "Misc/DateTime.h"

namespace TraceServices
{
	class IAnalysisSession;
	typedef uint64 FSessionHandle;
}

namespace UE::TraceTools
{

class IEventInfoProvider;

/** Implementation of ISessionTraceFilterService to query and set channels using the TraceController (MessageBus). */
class FSessionTraceControllerFilterService : public ISessionTraceFilterService
{
public:
	FSessionTraceControllerFilterService(TSharedPtr<ITraceController> InTraceController);
	virtual ~FSessionTraceControllerFilterService();

	/** Begin ISessionTraceFilterService overrides */
	virtual void GetRootObjects(TArray<FTraceObjectInfo>& OutObjects) const override;
	virtual const FTraceObjectInfo* GetObject(const FString& Name) const override;
	
	virtual const FDateTime& GetTimestamp() const override;
	virtual void SetObjectFilterState(const FString& InObjectName, const bool bFilterState) override;
	virtual void UpdateFilterPreset(const TSharedPtr<ITraceFilterPreset> InPreset, bool IsEnabled) override;

	virtual bool HasSettings() const override;
	virtual const FTraceStatus::FSettings& GetSettings() const override;

	virtual bool HasStats() const override;
	virtual const FTraceStats& GetStats() const override;
	/** End ISessionTraceFilterService overrides */

protected:
	/** Callback at end of engine frame, used to dispatch all enabled/disabled channels */
	void OnApplyChannelChanges();

	/** Retrieves channels names from provider and marks them all as disabled */
	void DisableAllChannels();

	void OnTraceStatusUpdated(const FTraceStatus& InStatus, FTraceStatus::EUpdateType InUpdateType, ITraceControllerCommands& Commands);

	void UpdateChannels(const FTraceStatus& InStatus);

	void OnSessionSelectionChanged();

protected:
	TSharedPtr<ITraceController> TraceController;

	/** A map with the key formed by hashing the object name and the object as the value.*/
	TMap<uint64, FTraceObjectInfo> Objects;

	/** Names of channels that were either enabled or disabled during the duration of this frame */
	TSet<FString> FrameEnabledChannels;
	TSet<FString> FrameDisabledChannels;

	/** Timestamp at which contained data (including provider) was last updated */
	FDateTime TimeStamp;

	bool bChannelsReceived = false;

	FTraceStatus::FSettings Settings;
	bool bHasSettings = false;

	FTraceStats Stats;
	bool bHasStats = false;
};

} // namespace UE::TraceTools