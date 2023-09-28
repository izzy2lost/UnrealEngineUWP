// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimatedAttributeManager.h"

////////////////////////////////////////////////////////////////////////////////
// TAnimatedAttributeBase
////////////////////////////////////////////////////////////////////////////////

TAnimatedAttributeBase::TAnimatedAttributeBase()
	: bIsRegistered(false)
{
}

TAnimatedAttributeBase::~TAnimatedAttributeBase()
{
}

void TAnimatedAttributeBase::Register()
{
	if(!bIsRegistered)
	{
		FAnimatedAttributeManager::Get().RegisterAttribute(AsShared());
		bIsRegistered = true;
	}
}

void TAnimatedAttributeBase::Unregister()
{
	if(bIsRegistered)
	{
		FAnimatedAttributeManager::Get().UnregisterAttribute(AsShared());
		bIsRegistered = false;
	}
}

////////////////////////////////////////////////////////////////////////////////
// FAnimatedAttributeManager
////////////////////////////////////////////////////////////////////////////////

FAnimatedAttributeManager::FAnimatedAttributeManager()
{
}

FAnimatedAttributeManager::~FAnimatedAttributeManager()
{
}

bool FAnimatedAttributeManager::Tick(float InDeltaTime)
{
	// remove state attributes
	Attributes.RemoveAll([](const TWeakPtr<TAnimatedAttributeBase>& Attribute) {
		return !Attribute.IsValid();
	});

	// at this point we expect the remaining attributes to be valid
	for(const TWeakPtr<TAnimatedAttributeBase>& Attribute : Attributes)
	{
		Attribute.Pin()->Tick(InDeltaTime);
	}

	return true;
}

void FAnimatedAttributeManager::SetupTick()
{
	const FTickerDelegate TickDelegate = FTickerDelegate::CreateRaw(this, &FAnimatedAttributeManager::Tick);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate);
}

void FAnimatedAttributeManager::TeardownTick()
{
	if(TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
}

void FAnimatedAttributeManager::RegisterAttribute(const TSharedRef<TAnimatedAttributeBase>& InAttribute)
{
	Attributes.Add(InAttribute.ToWeakPtr());
}

void FAnimatedAttributeManager::UnregisterAttribute(const TSharedRef<TAnimatedAttributeBase>& InAttribute)
{
	Attributes.Remove(InAttribute.ToWeakPtr());
}

FAnimatedAttributeManager& FAnimatedAttributeManager::Get()
{
	static FAnimatedAttributeManager Manager;
	return Manager;
}
