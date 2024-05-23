// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

enum class EVCamTargetViewportID : uint8;
class AActor;
class UVCamComponent;
class UVCamOutputProviderBase;

namespace UE::VCamCore
{
	class IViewportLocker;

	/** Uses UVCamComponent's lock settings for locking the editor or game viewport. */
	class FViewportLockManager : public FNoncopyable
	{
		friend class FViewportLockingSpec;
	public:

		DECLARE_DELEGATE_RetVal_OneParam(bool, FHasViewportOwnership, const UVCamOutputProviderBase&);
		FViewportLockManager(
			IViewportLocker& ViewportLocker UE_LIFETIMEBOUND,
			FHasViewportOwnership HasViewportOwnershipDelegate
		);

		/** Checks which of the output providers in the given VCam array should lock the viewport. */
		void UpdateViewportLockState(TConstArrayView<TWeakObjectPtr<UVCamComponent>> RegisteredVCams);

	private:

		/** Used to lock the viewport(s). */
		IViewportLocker& ViewportLocker;
		
		/** Looks up whether the given output provider has ownership over the viewport. */
		const FHasViewportOwnership HasViewportOwnershipDelegate;

		/** Whether to update the viewport locks at the end of the frame. */
		bool bRequestedRefresh = false;

		struct FViewportLockState
		{
			TWeakObjectPtr<const UVCamOutputProviderBase> LockReason;
		} LockState[4];

		FViewportLockState& GetLockState(EVCamTargetViewportID ViewportID);
		
		void UpdateViewport(TConstArrayView<TWeakObjectPtr<UVCamComponent>> RegisteredVCams, EVCamTargetViewportID ViewportID);
		void UpdateLockStateFor(UVCamOutputProviderBase& OutputProvider);
	};
}
