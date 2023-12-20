// Copyright Epic Games, Inc. All Rights Reserved.
#include "Scheduler.h"
#include "Job.h"
#include "TempHashService.h"
#include "MixerEngine.h"
#include "Device/DeviceManager.h"
#include "Device/Device.h"

DECLARE_CYCLE_STAT(TEXT("Scheduler_Update"), STAT_Scheduler_Update, STATGROUP_MixerEngine);

const double Scheduler::IdleTimeInterval = 500.0;
const double Scheduler::IdleBatchTimeLimit = 100.0;
const double Scheduler::CurrentBatchWarningLimit = 2000.0;

Scheduler::Scheduler()
{
	ObserverSource = std::make_shared<SchedulerObserverSource>();

	TempHashServicePtr TempHashSPtr = std::make_shared<TempHashService>();
	BlobHasherServicePtr BlobHasherSPtr = std::make_shared<BlobHasherService>();
	DeviceTransferServicePtr DeviceTransferSPtr = std::make_shared<DeviceTransferService>();
	ThumbnailsServicePtr ThumbnailsSPtr = std::make_shared<ThumbnailsService>();
	MipMapServicePtr MipmapSPtr = std::make_shared<MipMapService>();
	MinMaxServicePtr MinmaxSPtr = std::make_shared<MinMaxService>();
	HistogramServicePtr HistogramSPtr = std::make_shared<HistogramService>();

	BlobHasherServiceObj = BlobHasherSPtr;
	DeviceTransferServiceObj = DeviceTransferSPtr;
	ThumbnailsServiceObj = ThumbnailsSPtr;
	MipMapServiceObj = MipmapSPtr;
	MinMaxServiceObj = MinmaxSPtr;
	HistogramServiceObj = HistogramSPtr;

	AddIdleService(BlobHasherSPtr);
	AddIdleService(DeviceTransferSPtr);
	AddIdleService(ThumbnailsSPtr);
	AddIdleService(MipmapSPtr);
	AddIdleService(MinmaxSPtr);
	AddIdleService(HistogramSPtr);
}

Scheduler::~Scheduler()
{
	bIsRunning = false;
	StopServices();

	/// Add a bit of a delay to allow proper clean-up
	std::this_thread::sleep_for(std::chrono::milliseconds((int)IdleBatchTimeLimit * 2));

	Stop();
}

void Scheduler::Update(float dt)
{
	SCOPE_CYCLE_COUNTER(STAT_Scheduler_Update);
	check(IsInGameThread());

	/// Make sure value is initialised correct
	if (TimeSinceIdle < 0.001)
		TimeSinceIdle = Util::Time();

	if (!bIsRunning)
		Start();

	{
		FScopeLock Lock(&CurrentBatchMutex);
		if (CurrentBatch)
		{
			double currentTime = Util::Time();
			double delta = currentTime - CurrentBatchStartTime;
			double timeoutLimit = !MixerEngine::IsTestMode() ? CurrentBatchWarningLimit : CurrentBatchWarningLimit * 4.0;

			if (delta > timeoutLimit)
			{
				UE_LOG(LogBatch, Warning, TEXT("Current Batch: %llu been running for %f ms [Max threshold: %f, Jobs: %u/%u]!"), 
					CurrentBatch->GetBatchId(), (float)delta, (float)CurrentBatchWarningLimit, CurrentBatch->GetNumJobsRunning(), 
					(uint32)CurrentBatch->NumJobs());
				CurrentBatch->DebugDumpUnfinishedJobs();
				//_currentBatch->Terminate();
			}

			return;
		}
	}

	JobBatchPtr batchToRun = nullptr;
	{
		FScopeLock Lock(&BatchMutex);
		if (Batches.size())
		{
			/// TODO: Enable this at some point
			if (Batches.size() == 1)
			{
				batchToRun = Batches.front();
				Batches.pop_front();
			}
			else
			{
				check(!MixerEngine::IsTestMode());

				/// OK here, we're going to merge batches belonging to the same mix together
				
				// figure out the prioritized mix from the Batch
				batchToRun = Batches.front();
				UMixInterface* currentBatchMix = batchToRun->GetCycle()->GetMix();

				// find the latest Batch of required mix
				auto iter = Batches.begin();
				JobBatchPtr lastMixbatch = batchToRun;

				while (iter != Batches.end())
				{
					JobBatchPtr futureBatch = *iter;

					if (futureBatch->GetCycle()->GetMix() == currentBatchMix)
					{
						lastMixbatch = *iter;
					}

					iter++;
				}

				// we get latest Batch to run for the mix
				batchToRun = lastMixbatch;

				// Merge invalidation details of the mix from multiple batches.
				// Remove the batches from the list after merging.
				iter = Batches.begin();
				while (iter != Batches.end())
				{
					JobBatchPtr futureBatch = *iter;

					if (futureBatch->GetCycle()->GetMix() == currentBatchMix)
					{
						batchToRun->GetCycle()->MergeDetails(futureBatch->GetCycle()->GetDetails());
						iter = Batches.erase(iter);
					}
					else
						iter++;
				}
			}
		}
	}

	if (batchToRun)
	{
		FScopeLock Lock(&CurrentBatchMutex);
		CurrentBatch = batchToRun;
		CurrentBatchStartTime = Util::Time();

		UE_LOG(LogBatch, Log, TEXT("Next Batch: %llu [Prev: %llu]"), CurrentBatch->GetBatchId(), PreviousBatch ? PreviousBatch->GetBatchId() : 0);

		if (!CurrentBatch->WasGeneratedFromIdleService())
			TimeSinceIdle = Util::Time();

		if (bCaptureNextBatch)
		{
			CurrentBatch->SetCaptureRenderDoc(true);
			bCaptureNextBatch = false;
		}

		CurrentBatch->Exec([this] (JobBatch*)	/// Instead of passing it as an argument, JobBatch should be a return type; this is to keep it from cyclic dependancy
			{
				/// This closure will be called when all the jobs execution is really over
				/// tell the world through the observer
				/// Clear out the current Batch
				FScopeLock Lock(&CurrentBatchMutex);
				PreviousBatch = CurrentBatch;
				CurrentBatch = nullptr;

				if (PreviousBatch && !PreviousBatch->WasGeneratedFromIdleService())
					TimeSinceIdle = Util::Time();

				UE_LOG(LogBatch, Log, TEXT("Scheduler, Batch fully queued: %llu. Triggering Observer::BatchJobsDone ..."), PreviousBatch ? PreviousBatch->GetBatchId() : -1);

				ObserverSource->BatchJobsDone(PreviousBatch);

				UE_LOG(LogBatch, Log, TEXT("Scheduler Observer::BatchJobsDone finished for Batch: %llu"), PreviousBatch ? PreviousBatch->GetBatchId() : -1);
			})
			.then([this]()
				{
					if (PreviousBatch && !PreviousBatch->WasGeneratedFromIdleService())
						TimeSinceIdle = Util::Time();

					UE_LOG(LogBatch, Log, TEXT("Scheduler triggering Observer::BatchDone for Batch: %llu ..."), PreviousBatch ? PreviousBatch->GetBatchId() : -1);

					ObserverSource->BatchDone(CurrentBatch);

					UE_LOG(LogBatch, Log, TEXT("Scheduler Observer::BatchDone finished for Batch: %llu"), PreviousBatch ? PreviousBatch->GetBatchId() : -1);
				})
				.then([this]()
					{
						// Trigger cycle->invalidateDetails.DoneCallBackLambda()
						
						//Adding logs to debug and safty checks here to avoid the crash
						//Crash was happening in cases when we do not have a valid Cycle or Mix
						//This could happen when we open a script run it and then delete it and then try to run another script.
						// 
						//TODO: Investigate why we need to call the PreviousBatch->BroadcastOnDone instead of calling Currentbatch->BroadcastOnDone
						//As we are setting the PreviousBatch = CurrentBatch in the callback of Exec function and that gets called from EndBatch after the
						//execution of this .then block 
						//looks like a bug though 
						//fix would be to remove this .then block and call PreviousBatch->GetCycle()->GetDetails().BroadcastOnDone(); in the callback of Exec function
						if (!PreviousBatch)
						{
							UE_LOG(LogBatch, Log, TEXT("Scheduler Previous Batch is null"));
							return;
						}
						if (!PreviousBatch->GetCycle())
						{
							UE_LOG(LogBatch, Warning, TEXT("Scheduler Previous Batch : %llu Cycle is null") , PreviousBatch->GetBatchId());
							return;
						}
						if (!PreviousBatch->GetCycle()->GetDetails().Mix.IsValid())
						{
							UE_LOG(LogBatch, Warning, TEXT("Scheduler Previous Batch : %llu Cycle Details Mix is null"), PreviousBatch->GetBatchId());
							return;
						}

						PreviousBatch->GetCycle()->GetDetails().BroadcastOnDone();
					});
	}
	else
	{
		double Delta = Util::TimeDelta(TimeSinceIdle);
		if (Delta > IdleTimeInterval)
		{
			UpdateIdle();

			// Since we have time, notify the observer update
			ObserverSource->UpdateIdle();
		}
	}
}

void Scheduler::SetCaptureRenderDocNextBatch(bool bCapture /* = true */)
{
	bCaptureNextBatch = bCapture;
}

AsyncJobResultPtr Scheduler::UpdateIdleBatch(size_t Index)
{
	check(Index < IdleServices.size());
	IdleServicePtr Batch = IdleServices[Index];
	double StartTime = Util::Time();

	return Batch->Tick().then([this, Batch, StartTime](JobResultPtr result)
		{
			double EndTime = Util::Time();
			double Duration = EndTime - StartTime;
			bool bDidOffendTimeLimit = false;

			if (Duration > IdleBatchTimeLimit)
			{
				bDidOffendTimeLimit = true;
				UE_LOG(LogBatch, Warning, TEXT("Idle Batch [%s] time limit offense @ %f"), *Batch->GetName(), (float)Util::Time());
			}

			Batch->UpdateStats(EndTime, StartTime, bDidOffendTimeLimit);

			return result;
		});
}

void Scheduler::UpdateIdle()
{
	/// Currently we don't do the idle update in test mode
	if (MixerEngine::IsTestMode())
		return;

	// if no Batch to run, check if we have an idle Batch we should run
	{
		/// Shouldn't run idle services if a Batch is still being processed
		FScopeLock Lock(&CurrentBatchMutex);
		if (CurrentBatch)
			return;
	}

	{
		/// Shouldn't run idle services if we still have batches to process
		FScopeLock Lock(&BatchMutex);
		if (Batches.size())
			return;
	}

	UE_LOG(LogIdle_Svc, Verbose, TEXT("Scheduler::UpdateIdle"));

	if (MixerEngine::IsDestroying())
		return;

	size_t NumIdleBatches = 0;

	{
		FScopeLock Lock(&IdlServiceMutex);
		NumIdleBatches = IdleServices.size();
	}

	std::vector<AsyncJobResultPtr> Promises;

	Promises.emplace_back(MixerEngine::GetBlobber()->UpdateIdle());

	/// Idle update for all the devices
	for (size_t DeviceIndex = 0; DeviceIndex < MixerEngine::GetDeviceManager()->GetNumDevices(); DeviceIndex++)
	{
		Device* Dev = MixerEngine::GetDeviceManager()->GetDevice(DeviceIndex);
		if (Dev)
			Promises.emplace_back(Dev->UpdateIdle());
	}

	for (size_t IdleBatchIndex = 0; IdleBatchIndex < NumIdleBatches; IdleBatchIndex++)
	{
		auto Promise = UpdateIdleBatch(IdleBatchIndex);
		Promises.push_back(std::move(Promise));
	}

	if (Promises.size())
	{
		cti::when_all(Promises.begin(), Promises.end()).then([this]()
			{
				/// TODO:
			});
	}
}

void Scheduler::AddIdleService(IdleServicePtr Service)
{
	FScopeLock Lock(&IdlServiceMutex);
	IdleServices.push_back(Service);
}

void Scheduler::CaptureRenderDocLastRunBatch()
{
}

void Scheduler::ClearCache()
{
	check(IsInGameThread() && MixerEngine::IsTestMode());

	check(!CurrentBatch);
	check(!PreviousBatch || PreviousBatch->IsFinished());
	check(Batches.empty());

	PreviousBatch = nullptr;
}

void Scheduler::AddBatch(JobBatchPtr Batch)
{
	check(IsInGameThread());

	FScopeLock Lock(&BatchMutex);

	if (Batches.size())
	{
		check(Batches.back()->GetFrameId() <= Batch->GetFrameId());
	}

	check(Batch->GetFrameId())
		UE_LOG(LogBatch, Log, TEXT("Adding Batch: %llu [FrameId: %llu, Num batches: %llu]"), Batch->GetBatchId(), Batch->GetFrameId(), Batches.size());

	Batches.push_back(Batch);
	ObserverSource->BatchAdded(Batch); // notify observer
	TimeSinceIdle = Util::Time();
}

void Scheduler::Start()
{
	check(IsInGameThread());
	bIsRunning = true;

	if (ObserverSource)
		ObserverSource->Start();
}

void Scheduler::StopServices()
{
	check(IsInGameThread());
	for (auto Service : IdleServices)
		Service->Stop();

	IdleServices.clear();
}

void Scheduler::Stop()
{
	check(IsInGameThread());
	bIsRunning = false;

	if (ObserverSource)
		ObserverSource->Stop();
}

void Scheduler::RegisterObserverSource(const SchedulerObserverSourcePtr& InObserverSource)
{
	if (InObserverSource)
	{
		ObserverSource = InObserverSource;
	}
	else
	{
		ObserverSource = std::make_shared<SchedulerObserverSource>();
	}
}