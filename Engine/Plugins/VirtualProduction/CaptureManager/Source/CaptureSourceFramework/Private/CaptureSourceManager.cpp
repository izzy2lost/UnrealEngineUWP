// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceManager.h"

#include "CaptureSourceFrameworkLog.h"

DEFINE_LOG_CATEGORY(LogCaptureSourceManager);

template<typename T>
class FMonitor
{
public:
	template<typename ...Args>
	FMonitor(Args&&... InArgs) 
		: Object(Forward<Args>(InArgs)...)
	{

	}

	FMonitor(T InObject) 
		: Object(MoveTemp(InObject))
	{
	}

	class FHelper
	{
	public:
		FHelper(FMonitor* InOwner)
			: Owner(InOwner)
			, ScopeLock(&InOwner->Mutex)
		{
		}

		T* operator->()
		{
			return &Owner->Object;
		}

	private:

		FMonitor* Owner;
		FScopeLock ScopeLock;
	};

	FHelper operator->()
	{
		return FHelper(this);
	}

	FHelper Lock()
	{
		return FHelper(this);
	}

	T& GetUnsafe()
	{
		return Object;
	}

	T Claim()
	{
		return MoveTemp(Object);
	}

private:

	FCriticalSection Mutex;
	T Object;
};

FCaptureSourceManager::FCaptureSourceManager()
{
}

FCaptureSourceHandle FCaptureSourceManager::GetCaptureSource(FCaptureSourceId InCaptureSourceId)
{
	check(CaptureSources.Contains(InCaptureSourceId));
	return FCaptureSourceHandle(CaptureSources[InCaptureSourceId].Get());
}

TArray<FCaptureSourceId> FCaptureSourceManager::GetCaptureSourceIds() const
{
	TArray<FCaptureSourceId> CaptureSourceIds;
	CaptureSourceIds.Reserve(CaptureSources.Num());

	CaptureSources.GetKeys(CaptureSourceIds);

	return CaptureSourceIds;
}

void FCaptureSourceManager::RemoveCaptureSource(FCaptureSourceId InCaptureSourceId)
{
	check(CaptureSources.Contains(InCaptureSourceId));
	CaptureSources.Remove(InCaptureSourceId);
}

void FCaptureSourceManager::DiscoverCaptureSources(FDiscoverCallback InCallback)
{
	TSharedPtr<FCallbackSynchronizer> CallbackSynchronizer = FCallbackSynchronizer::Create();

	TSharedPtr<FMonitor<TArray<FCaptureSourceResult>>> Collector
		= MakeShared<FMonitor<TArray<FCaptureSourceResult>>>();

	for (const TPair<FString, TUniquePtr<FCaptureSourceFactory>>& CaptureSourceFactoryPair : CaptureSourceFactoryList)
	{
		const TUniquePtr<FCaptureSourceFactory>& CaptureSourceFactory = CaptureSourceFactoryPair.Value;

		auto Callback = CallbackSynchronizer->CreateCallback(
			[this, CaptureSourceFactoryId = CaptureSourceFactory->GetCaptureSourceFactoryId(), Collector = Collector.Get()](TArray<FCaptureSourceFactory::FCaptureSourceResult> InCaptureSourceResults)
		{
			for (FCaptureSourceFactory::FCaptureSourceResult& CaptureSourceResult : InCaptureSourceResults)
			{
				if (CaptureSourceResult.IsValid())
				{
					FCaptureSourceId Id = CurrentCaptureSourceId++;
					CaptureSources.Emplace(Id, CaptureSourceResult.StealValue());

					(*Collector)->Emplace(MakeValue(Id));
				}
				else
				{
					FCaptureSourceError Error = CaptureSourceResult.StealError();
					UE_LOG(LogCaptureSourceManager, Error, TEXT("Failed to create a capture source %s: %s"), *CaptureSourceFactoryId, *Error.GetMessage());

					(*Collector)->Emplace(MakeError(MoveTemp(Error)));
				}
			}
		});

		FCaptureVoidResult Result = CaptureSourceFactory->Discover(FCaptureSourceFactory::FDiscoverCallback::CreateLambda(MoveTemp(Callback)));

		if (Result.HasError())
		{
			UE_LOG(LogCaptureSourceManager, Error, TEXT("Failed to start discovery process for %s: %s"), *CaptureSourceFactory->GetCaptureSourceFactoryId(), *Result.GetError().GetMessage());
		}
	}

	CallbackSynchronizer->AfterAll(
		FCallbackSynchronizer::FAfterAllDelegate::CreateLambda([this, UserCallback = MoveTemp(InCallback), Collector = MoveTemp(Collector)]()
	{
		UserCallback.ExecuteIfBound(Collector->Claim());
	}), true);
}

void FCaptureSourceManager::DiscoverCaptureSourcesForFactory(FString InCaptureSourceFactoryId, FDiscoverCallback InCallback)
{
	check(CaptureSourceFactoryList.Contains(InCaptureSourceFactoryId));
	
	const TUniquePtr<FCaptureSourceFactory>& CaptureSourceFactory = CaptureSourceFactoryList[InCaptureSourceFactoryId];

	FCaptureSourceFactory::FDiscoverCallback Callback = 
		FCaptureSourceFactory::FDiscoverCallback::CreateLambda(
			[this, InCaptureSourceFactoryId, UserCallback = MoveTemp(InCallback)](TArray<FCaptureSourceFactory::FCaptureSourceResult> InCaptureSourceResults)
	{
		TArray<FCaptureSourceResult> CaptureSourceIds;
		for (FCaptureSourceFactory::FCaptureSourceResult& CaptureSourceResult : InCaptureSourceResults)
		{
			if (CaptureSourceResult.IsValid())
			{
				FCaptureSourceId Id = CurrentCaptureSourceId++;
				CaptureSources.Emplace(Id, CaptureSourceResult.StealValue());

				CaptureSourceIds.Emplace(MakeValue(Id));
			}
			else
			{
				FCaptureSourceError Error = CaptureSourceResult.StealError();
				UE_LOG(LogCaptureSourceManager, Error, TEXT("Failed to create a capture source %s: %s"), *InCaptureSourceFactoryId, *Error.GetMessage());

				CaptureSourceIds.Emplace(MakeError(MoveTemp(Error)));
			}
		}

		UserCallback.ExecuteIfBound(MoveTemp(CaptureSourceIds));
	});

	FCaptureVoidResult Result = CaptureSourceFactory->Discover(MoveTemp(Callback));

	if (Result.HasError())
	{
		UE_LOG(LogCaptureSourceManager, Error, TEXT("Failed to start discovery process for %s: %s"), *InCaptureSourceFactoryId, *Result.GetError().GetMessage());
	}
}

void FCaptureSourceManager::RegisterCaptureSourceFactory(TUniquePtr<FCaptureSourceFactory> InCaptureSourceFactory)
{
	FString CaptureSourceFactoryId = InCaptureSourceFactory->GetCaptureSourceFactoryId();
	CaptureSourceFactoryList.Emplace(MoveTemp(CaptureSourceFactoryId), MoveTemp(InCaptureSourceFactory));
}

FCaptureSourceDescriptor FCaptureSourceManager::GetCaptureSourceFactory(FString InCaptureSourceFactoryId)
{
	return CaptureSourceFactoryList[InCaptureSourceFactoryId]->CreateCaptureSourceDescriptor();
}

TArray<FCaptureSourceDescriptor> FCaptureSourceManager::GetCaptureSourceFactoryList()
{
	TArray<FCaptureSourceDescriptor> Descriptors;
	Descriptors.Reserve(CaptureSourceFactoryList.Num());

	for (const TPair<FString, TUniquePtr<FCaptureSourceFactory>>& CaptureSourceFactory : CaptureSourceFactoryList)
	{
		Descriptors.Add(CaptureSourceFactory.Value->CreateCaptureSourceDescriptor());
	}

	return Descriptors;
}

FCaptureSourceManager::FCaptureSourceResult FCaptureSourceManager::CreateCaptureSource(FString InCaptureSourceFactoryId, FString InName, TMap<FString, FPropertyValue> InCreationParamsValue)
{
	TUniquePtr<FCaptureSourceFactory>* CaptureSourceFactory = CaptureSourceFactoryList.Find(InCaptureSourceFactoryId);
	
	if (!CaptureSourceFactory)
	{
		return MakeError(FCaptureSourceError(ECaptureSourceError::NotFound, TEXT("Capture Source Factory doesn't exist")));
	}

	FCaptureSourceFactory::FCaptureSourceResult CaptureSourceResult = (*CaptureSourceFactory)->CreateCaptureSource(InName, MoveTemp(InCreationParamsValue));

	if (CaptureSourceResult.IsValid())
	{
		FCaptureSourceId Id = CurrentCaptureSourceId++;
		CaptureSources.Emplace(Id, CaptureSourceResult.StealValue());

		return MakeValue(Id);
	}
	else
	{
		return MakeError(CaptureSourceResult.StealError());
	}
}

FString FCaptureSourceManager::GetCaptureSourceFactoryById(FCaptureSourceId InCaptureSourceId)
{
	check(CaptureSources.Contains(InCaptureSourceId));
	return CaptureSources[InCaptureSourceId]->GetCaptureSourceFactoryId();
}