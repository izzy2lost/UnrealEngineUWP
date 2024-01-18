// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TextureGraphEngine.h"
#include "Device/Device.h"
#include "Device/DeviceObserverSource.h"
#include "Data/Blobber.h"
#include "Job/Scheduler.h"

using HashArray = DeviceObserverSource::HashArray;

DECLARE_LOG_CATEGORY_EXTERN(LogMixerInsightObserver, Log, All);

/// Concrete DeviceObserver interface
class MIXERINSIGHT_API MixerInsightDeviceObserver : public DeviceObserverSource
{
protected:
	DeviceType DevType;

	/// Protected interface of emitters called by the device to notify the observers
	virtual void DeviceBuffersUpdated(HashNDescArray&& AddedBuffers, HashArray&& RemovedBuffers) override;
public:
	MixerInsightDeviceObserver();
	explicit MixerInsightDeviceObserver(DeviceType);
	virtual ~MixerInsightDeviceObserver() override;
};

/// Concrete BlobberObserver interface
class MIXERINSIGHT_API MixerInsightBlobberObserver : public BlobberObserverSource
{
private:
protected:
	/// Protected interface of emitters called by the device to notify the observers
	virtual void BlobberUpdated(HashArray&& AddedHashes, HashArray&& RemappedHashes) override;

public:
	MixerInsightBlobberObserver();
	virtual ~MixerInsightBlobberObserver() override;
};

/// Concrete SchedulerObserver interface
class MIXERINSIGHT_API MixerInsightSchedulerObserver : public SchedulerObserverSource
{
private:
protected:
	/// Protected interface of emitters called by the scheduler to notify the observers
	virtual void Start() override;
	virtual void UpdateIdle() override;
	virtual void Stop() override;
	virtual void BatchAdded(JobBatchPtr Batch) override;
	virtual void BatchDone(JobBatchPtr Batch) override;
	virtual void BatchJobsDone(JobBatchPtr Batch) override;

public:

	MixerInsightSchedulerObserver();
	virtual ~MixerInsightSchedulerObserver() override;
};

/// Concrete EngineObserver interface
/// Responsible for:
///	  1/ watching the engine life cycle
///   2/ owning the other system observers, and installing them appropirately when an Engine is active
///	  3/ notifying Insight
class MIXERINSIGHT_API MixerInsightEngineObserver : public EngineObserverSource
{
protected:
	/// Protected interface of emitters called by the engine to notify the observers
	virtual void Created() override;
	virtual void Destroyed() override;

public:
	MixerInsightEngineObserver();
	virtual ~MixerInsightEngineObserver() override;

	std::shared_ptr<MixerInsightDeviceObserver>			_deviceObservers[(uint32)DeviceType::Count];
	std::shared_ptr<MixerInsightBlobberObserver>		BlobberObserver;
	std::shared_ptr<MixerInsightSchedulerObserver>		SchedulerObserver;
};
