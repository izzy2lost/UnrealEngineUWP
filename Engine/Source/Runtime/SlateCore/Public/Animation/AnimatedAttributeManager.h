// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

/**
 * Animated Attribute Base Layer - used for registering the attribute centrally
 */
class TAnimatedAttributeBase : public TSharedFromThis<TAnimatedAttributeBase>
{
protected:
	
	SLATECORE_API TAnimatedAttributeBase();
	SLATECORE_API virtual ~TAnimatedAttributeBase();

	SLATECORE_API void Register();
	SLATECORE_API void Unregister();

	virtual void Tick(float InDeltaTime) = 0;

private:

	TAnimatedAttributeBase(const TAnimatedAttributeBase& InOther) = delete;
	TAnimatedAttributeBase& operator =(const TAnimatedAttributeBase& Other) = delete;

	bool bIsRegistered;

	friend class FAnimatedAttributeManager;
};

/**
 * A central manager for animated attributes
 */
class FAnimatedAttributeManager
{

public:

	FAnimatedAttributeManager();
	~FAnimatedAttributeManager();

	SLATECORE_API static FAnimatedAttributeManager& Get();
	SLATECORE_API bool Tick(float InDeltaTime);

	SLATECORE_API void SetupTick();
	SLATECORE_API void TeardownTick();

private:

	void RegisterAttribute(const TSharedRef<TAnimatedAttributeBase>& InAttribute);
	void UnregisterAttribute(const TSharedRef<TAnimatedAttributeBase>& InAttribute);

	TArray<TWeakPtr<TAnimatedAttributeBase>> Attributes;
	FTSTicker::FDelegateHandle TickHandle;

	friend class TAnimatedAttributeBase;
};
