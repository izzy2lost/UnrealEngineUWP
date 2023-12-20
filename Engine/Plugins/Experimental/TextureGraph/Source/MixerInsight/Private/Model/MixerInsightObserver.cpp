// Copyright Epic Games, Inc. All Rights Reserved.
#include "Model/MixerInsightObserver.h"

#include "MixerInsight.h"
#include "Model/MixerInsightSession.h"

#include <Device/DeviceManager.h>

//DEFINE_LOG_CATEGORY(LogMixerInsight);


MixerInsightDeviceObserver::MixerInsightDeviceObserver()
{
}
MixerInsightDeviceObserver::MixerInsightDeviceObserver(DeviceType InDevType) : DevType(InDevType)
{
}
MixerInsightDeviceObserver::~MixerInsightDeviceObserver()
{
}
void MixerInsightDeviceObserver::DeviceBuffersUpdated(HashNDescArray&& AddedBuffers, HashArray&& RemovedBuffers)
{
	MixerInsight::Instance()->GetSession()->DeviceBuffersUpdated(DevType, std::move(AddedBuffers), std::move(RemovedBuffers));

}

MixerInsightBlobberObserver::MixerInsightBlobberObserver()
{
}
MixerInsightBlobberObserver::~MixerInsightBlobberObserver()
{
}
void MixerInsightBlobberObserver::BlobberUpdated(HashArray&& AddedHashes, HashArray&& RemappedHashes)
{
	MixerInsight::Instance()->GetSession()->BlobberHashesUpdated(std::move(AddedHashes), std::move(RemappedHashes));
}


MixerInsightSchedulerObserver::MixerInsightSchedulerObserver()
{
}
MixerInsightSchedulerObserver::~MixerInsightSchedulerObserver()
{
}

void MixerInsightSchedulerObserver::Start()
{
	UE_LOG(LogMixerInsight, Log, TEXT("MixerInsightSchedulerObserver::Start"));
}

void MixerInsightSchedulerObserver::UpdateIdle()
{
	if (MixerEngine::IsTestMode())
		return;

	MixerInsight::Instance()->GetSession()->UpdateIdle();
}

void MixerInsightSchedulerObserver::Stop()
{
	UE_LOG(LogMixerInsight, Log, TEXT("MixerInsightSchedulerObserver::Stop"));
}

void MixerInsightSchedulerObserver::BatchAdded(JobBatchPtr Batch)
{
	if (MixerEngine::IsTestMode())
		return;

	MixerInsight::Instance()->GetSession()->BatchAdded(Batch);
}

void MixerInsightSchedulerObserver::BatchDone(JobBatchPtr Batch)
{
	if (MixerEngine::IsTestMode())
		return;

	MixerInsight::Instance()->GetSession()->BatchDone(Batch);
}

void MixerInsightSchedulerObserver::BatchJobsDone(JobBatchPtr Batch)
{
	if (MixerEngine::IsTestMode())
		return;

	MixerInsight::Instance()->GetSession()->BatchJobsDone(Batch);
}

MixerInsightEngineObserver::MixerInsightEngineObserver()
{
	UE_LOG(LogMixerInsight, Log, TEXT("MixerInsightEngineObserver::Constructor"));

	for (int i = 0; i < (uint32)DeviceType::Count; ++i)
	{
		_deviceObservers[i] = std::make_shared<MixerInsightDeviceObserver>((DeviceType)i);
	}
	BlobberObserver = std::make_shared<MixerInsightBlobberObserver>();
	SchedulerObserver = std::make_shared<MixerInsightSchedulerObserver>();
}

MixerInsightEngineObserver::~MixerInsightEngineObserver()
{
	UE_LOG(LogMixerInsight, Log, TEXT("MixerInsightEngineObserver::Destructor"));
}

void MixerInsightEngineObserver::Created()
{
	if (MixerEngine::IsTestMode())
		return;

	UE_LOG(LogMixerInsight, Log, TEXT("MixerInsightEngineObserver::Created"));

	/// Engine is created when notified, this should be true
	if (MixerEngine::GetInstance())
	{
		for (int i = 0; i < (uint32)DeviceType::Count; ++i)
		{
			auto Dev = MixerEngine::GetInstance()->GetDeviceManager()->GetDevice(i);
			if (Dev)
				Dev->RegisterObserverSource(_deviceObservers[i]);
		}
		MixerEngine::GetInstance()->GetBlobber()->RegisterObserverSource(BlobberObserver);
		MixerEngine::GetInstance()->GetScheduler()->RegisterObserverSource(SchedulerObserver);
	}

	/// This is a brand new session engine
	MixerInsight::Instance()->GetSession()->EngineCreated();
}


void MixerInsightEngineObserver::Destroyed()
{
	if (MixerEngine::IsTestMode())
		return;

	UE_LOG(LogMixerInsight, Log, TEXT("MixerInsightEngineObserver::Destroyed"));

	/// Engine is already destroyed when notified
	if (MixerEngine::GetInstance())
	{
		for (int i = 0; i < (uint32)DeviceType::Count; ++i)
		{
			auto Dev = MixerEngine::GetInstance()->GetDeviceManager()->GetDevice(i);
			if (Dev)
				Dev->RegisterObserverSource(nullptr);
		}
		MixerEngine::GetInstance()->GetBlobber()->RegisterObserverSource(nullptr); /// remove blobber observer from the engine
		MixerEngine::GetInstance()->GetScheduler()->RegisterObserverSource(nullptr); /// remove scheduler observer from the engine
	}

	MixerInsight::Instance()->GetSession()->EngineDestroyed();
}
