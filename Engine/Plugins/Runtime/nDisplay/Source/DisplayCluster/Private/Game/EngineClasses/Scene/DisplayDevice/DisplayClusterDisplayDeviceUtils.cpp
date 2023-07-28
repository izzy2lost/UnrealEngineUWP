// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDisplayDeviceUtils.h"

#include "Components/DisplayDevice/DisplayClusterDisplayDeviceBaseComponent.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterRootActor.h"
#include "Misc/DisplayClusterLog.h"

UDisplayClusterDisplayDeviceBaseComponent* UE::DisplayClusterDisplayDeviceUtils::FindAndSyncDisplayDeviceFromViewport(
	UDisplayClusterConfigurationViewport* InViewport)
{
	if (IsValid(InViewport))
	{
		if (ADisplayClusterRootActor* RootActor = InViewport->GetTypedOuter<ADisplayClusterRootActor>())
		{
			UDisplayClusterDisplayDeviceBaseComponent* DeviceBaseComponent = nullptr;

			const bool bUseDefaultDevice = InViewport->DisplayDeviceName.IsEmpty();
			if (bUseDefaultDevice)
			{
				DeviceBaseComponent = RootActor->GetDefaultDisplayDevice();
			}
			else
			{
				// Find the component from the device name
				TArray<UDisplayClusterDisplayDeviceBaseComponent*> Components;
				RootActor->GetComponents<UDisplayClusterDisplayDeviceBaseComponent>(Components);

				if (UDisplayClusterDisplayDeviceBaseComponent** FoundComponent =
					Components.FindByPredicate(
						[&](const UDisplayClusterDisplayDeviceBaseComponent* Component)
						{
							return Component->GetName() == InViewport->DisplayDeviceName;
						}))
				{
					DeviceBaseComponent = *FoundComponent;
				}
			}

			if (!DeviceBaseComponent && !bUseDefaultDevice)
			{
				// Could not locate device on the instance, attempt reset to CDO value first in case there is a desync.
				// This might be needed if a display device was renamed, the blueprint compiled, then the action undone.
				if (UDisplayClusterConfigurationClusterNode* ClusterNodeInstance = Cast<UDisplayClusterConfigurationClusterNode>(InViewport->GetOuter()))
				{
					if (const FString* ViewportKeyPtr = ClusterNodeInstance->Viewports.FindKey(InViewport))
					{
						const FString ViewportId = *ViewportKeyPtr;
						if (const UDisplayClusterConfigurationCluster* ClusterNodeInstanceParent = Cast<UDisplayClusterConfigurationCluster>(ClusterNodeInstance->GetOuter()))
						{
							// Find the cluster key on the instance
							if (const FString* KeyPtr = ClusterNodeInstanceParent->Nodes.FindKey(ClusterNodeInstance))
							{
								const FString ClusterName = *KeyPtr;
								// Use the instance cluster key to find the correct CDO viewport
								const ADisplayClusterRootActor* RootActorCDO = CastChecked<ADisplayClusterRootActor>(RootActor->GetClass()->ClassDefaultObject);
								if (const UDisplayClusterConfigurationData* ConfigDataCDO = RootActorCDO->GetConfigData())
								{
									if (UDisplayClusterConfigurationViewport* ViewportCDO = ConfigDataCDO->GetViewport(ClusterName, ViewportId))
									{
										if (InViewport->DisplayDeviceName != ViewportCDO->DisplayDeviceName)
										{
											UE_LOG(LogDisplayClusterGame, Log, TEXT("Could not find display device '%s' on instance. Using CDO default device '%s'."),
												*InViewport->DisplayDeviceName, *ViewportCDO->DisplayDeviceName);
											InViewport->Modify();
											InViewport->DisplayDeviceName = ViewportCDO->DisplayDeviceName;
									
											return FindAndSyncDisplayDeviceFromViewport(InViewport);
										}
									}
								}
							}
						}
					}
				}
			
				UE_LOG(LogDisplayClusterGame, Warning, TEXT("Could not find display device '%s'. Using root actor default device."), *InViewport->DisplayDeviceName);
			}

			if (!DeviceBaseComponent)
			{
				DeviceBaseComponent = RootActor->GetDefaultDisplayDevice();
			}

			return DeviceBaseComponent;
		}
	}

	return nullptr;
}
