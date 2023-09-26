// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/ObjectMacros.h"
#include "AsyncLoadingTests_RecursiveLoads.generated.h"

UCLASS()
class UAsyncLoadingTests_RecursiveLoads : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSoftObjectPtr<UObject> SoftReference;

	// Those delegates allow to easily change behavior between tests
	DECLARE_TS_MULTICAST_DELEGATE_OneParam(FOnPostLoadEvent, UAsyncLoadingTests_RecursiveLoads* /*Object*/);
	static FOnPostLoadEvent OnPostLoadEvent;

	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FOnSerializeEvent, FArchive& Ar, UAsyncLoadingTests_RecursiveLoads* /*Object*/);
	static FOnSerializeEvent OnSerializeEvent;

	virtual void PostLoad() override
	{
		Super::PostLoad();

		OnPostLoadEvent.Broadcast(this);
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		OnSerializeEvent.Broadcast(Ar, this);
	}
};
