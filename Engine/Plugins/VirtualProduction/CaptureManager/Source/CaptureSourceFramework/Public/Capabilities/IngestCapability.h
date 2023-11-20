// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSourceCapability.h"

#include "Templates/ValueOrError.h"

class CAPTURESOURCEFRAMEWORK_API FIngestCapabilityError
{
public:

	enum ECode
	{
		Ok = 0,
		AbortedByUser = 1,
		InternalError = 2
	};

	FIngestCapabilityError(ECode InCode, FString InMessage);

	ECode GetCode() const;
	const FString& GetMessage() const;

private:

	ECode Code;
	FString Message;
};

using FTakeId = int32;

struct FTakeInfo
{
	FTakeId Id;

	FString SlateName;
	int16 TakeNumber;

	int32 NumFrames;
	double FrameRate;

	FDateTime Date;

	FString GetName()
	{
		return FString::Format(TEXT("{0}_{1}"), { SlateName, TakeNumber });
	}
};

class CAPTURESOURCEFRAMEWORK_API FImportTakeCmd : public FCommandBase
{
public:
	static const FString Name;

	DECLARE_DELEGATE_TwoParams(FProcessProgressReporter, FTakeId, double);
	using FProcessFinished = TDelegate<void(FTakeId, TValueOrError<void, FIngestCapabilityError>)>;

	FImportTakeCmd(FProcessProgressReporter InProgressReporter, 
				   FProcessFinished InProcessFinished);

	FProcessProgressReporter ProgressReporter;
	FProcessFinished ProcessFinished;
};

class CAPTURESOURCEFRAMEWORK_API FUpdateTakeListCmd : public FCommandBase
{
public:

	static const FString Name;

	DECLARE_DELEGATE_OneParam(FCallback, TArray<FTakeId>);

	FUpdateTakeListCmd(FCallback InCallback);

	FCallback Callback;
};

class CAPTURESOURCEFRAMEWORK_API FIngestCapabilityBase : public FCaptureSourceCapability
{
public:
	class CAPTURESOURCEFRAMEWORK_API FTakeProcessHandle
	{
	public:

		FTakeProcessHandle() = default;
		FTakeProcessHandle(FIngestCapabilityBase* InOwner, FTakeId InId);

		void Cancel();

	private:

		FIngestCapabilityBase* Owner;
		FTakeId Id;
	};

	static const FString Name;

	static const FString CancelCmd;

	static const FString Id;
	static const FString Location;

	FIngestCapabilityBase();

	virtual ~FIngestCapabilityBase() = default;

	virtual FTakeProcessHandle ImportTake(FTakeId InTakeId,
										  FString InLocation,
										  FImportTakeCmd::FProcessFinished InProcessFinished,
										  FImportTakeCmd::FProcessProgressReporter InProcessProgressReported) = 0;

	virtual void UpdateTakeList(FUpdateTakeListCmd::FCallback InUpdateTakeListCallback) = 0;

	FTakeId AddTakeInfo(FTakeInfo InTakeInfo);
	FTakeInfo GetTakeInfo(FTakeId InTakeId) const;
	TArray<FTakeId> GetTakeIds() const;

protected:

	virtual void Cancel(FTakeId InId) = 0;

	TMap<FTakeId, FTakeInfo> Takes;

private:

	std::atomic<FTakeId> CurrentTakeId;
};

class CAPTURESOURCEFRAMEWORK_API FIngestCapability : public FIngestCapabilityBase
{
public:

	FIngestCapability();

	FTakeProcessHandle ImportTake(FTakeId InTakeId,
								  FString InLocation,
								  FImportTakeCmd::FProcessFinished InProcessFinished,
								  FImportTakeCmd::FProcessProgressReporter InProcessProgressReported) override;
	void UpdateTakeList(FUpdateTakeListCmd::FCallback InUpdateTakeListCallback) override;

	void SetImportTakeHandler(FCommandHandler InCommandHandler);
	void SetUpdateTakeListHandler(FCommandHandler InCommandHandler);
	void SetCancelHandler(FCommandHandler InCommandHandler);

private:

	void Cancel(FTakeId InTakeId);
};