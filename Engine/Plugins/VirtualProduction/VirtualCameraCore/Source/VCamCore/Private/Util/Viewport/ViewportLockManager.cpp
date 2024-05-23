// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewportLockManager.h"

#include "VCamComponent.h"
#include "Interfaces/IViewportLocker.h"

#include "Output/VCamOutputProviderBase.h"

namespace UE::VCamCore
{
	FViewportLockManager::FViewportLockManager(IViewportLocker& ViewportLocker, FHasViewportOwnership HasViewportOwnershipDelegate)
		: ViewportLocker(ViewportLocker)
		, HasViewportOwnershipDelegate(MoveTemp(HasViewportOwnershipDelegate))
	{}

	FViewportLockManager::FViewportLockState& FViewportLockManager::GetLockState(EVCamTargetViewportID ViewportID)
	{
		static_assert(static_cast<int32>(EVCamTargetViewportID::Viewport1) == 0, "Update this location");
		return LockState[static_cast<int32>(ViewportID)];
	}

	void FViewportLockManager::UpdateViewportLockState(TConstArrayView<TWeakObjectPtr<UVCamComponent>> RegisteredVCams)
	{
		UpdateViewport(RegisteredVCams, EVCamTargetViewportID::Viewport1);
		UpdateViewport(RegisteredVCams, EVCamTargetViewportID::Viewport2);
		UpdateViewport(RegisteredVCams, EVCamTargetViewportID::Viewport3);
		UpdateViewport(RegisteredVCams, EVCamTargetViewportID::Viewport4);
	}
	
	void FViewportLockManager::UpdateViewport(TConstArrayView<TWeakObjectPtr<UVCamComponent>> RegisteredVCams, EVCamTargetViewportID ViewportID)
	{
		const TWeakObjectPtr<const UVCamOutputProviderBase>& WeakLockReason = GetLockState(ViewportID).LockReason;
		const UVCamOutputProviderBase* LockReason = WeakLockReason.Get();
		const bool bWasLockReasonInvalidated = LockReason
			&& (!HasViewportOwnershipDelegate.Execute(*LockReason)
				|| LockReason->GetTargetViewport() != ViewportID);
		if (bWasLockReasonInvalidated || WeakLockReason.IsStale())
		{
			ViewportLocker.SetActorLock(ViewportID, FActorLockContext{ nullptr });
		}
		
		for (const TWeakObjectPtr<UVCamComponent>& VCamComponent : RegisteredVCams)
		{
			for (UVCamOutputProviderBase* OutputProvider : VCamComponent->GetOutputProviders())
			{
				if (ensure(OutputProvider) && OutputProvider->GetTargetViewport() == ViewportID)
				{
					UpdateLockStateFor(*OutputProvider);
				}
			}
		}
	}

	void FViewportLockManager::UpdateLockStateFor(UVCamOutputProviderBase& OutputProvider)
	{
		const EVCamTargetViewportID TargetViewportID = OutputProvider.GetTargetViewport();
		const TWeakObjectPtr<AActor> ActorLock = ViewportLocker.GetActorLock(TargetViewportID);
		const TWeakObjectPtr<AActor> CinematicActorLock = ViewportLocker.GetCinematicActorLock(TargetViewportID);

		AActor* OwningActor = OutputProvider.GetTypedOuter<AActor>();
		const bool bIsExternalLockInPlace = ViewportLocker.ShouldLockViewport(TargetViewportID) && (ActorLock != nullptr || CinematicActorLock != nullptr);
		const bool bWantsLock = HasViewportOwnershipDelegate.Execute(OutputProvider)
			&& OutputProvider.GetVCamComponent()->GetViewportLockState().ShouldLock(TargetViewportID);
		if (ensure(OwningActor) && !bIsExternalLockInPlace && bWantsLock)
		{
			ViewportLocker.SetActorLock(TargetViewportID, FActorLockContext{ &OutputProvider });
			GetLockState(TargetViewportID).LockReason = &OutputProvider;
		}
	}
}
