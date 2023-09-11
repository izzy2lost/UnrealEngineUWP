// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfiguration.h"

#include "DisplayClusterViewportConfiguration_Viewport.h"
#include "DisplayClusterViewportConfiguration_ViewportManager.h"
#include "DisplayClusterViewportConfiguration_Postprocess.h"
#include "DisplayClusterViewportConfiguration_ProjectionPolicy.h"
#include "DisplayClusterViewportConfiguration_ICVFX.h"
#include "DisplayClusterViewportConfiguration_ICVFXCamera.h"

#include "DisplayClusterViewportConfigurationHelpers_RenderFrameSettings.h"

#include "DisplayClusterConfigurationStrings.h"

#include "DisplayClusterRootActor.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/Postprocess/DisplayClusterViewportPostProcessManager.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"

#include "Render/IPDisplayClusterRenderManager.h"
#include "Misc/DisplayClusterLog.h"

#include "Engine/RendererSettings.h"

namespace UE::DisplayCluster::ConfigurationHelpers
{
	static inline ADisplayClusterRootActor* ImplGetRootActor(const FDisplayClusterActorRef& InConfigurationRootActorRef)
	{
		if (AActor* ActorPtr = InConfigurationRootActorRef.GetOrFindSceneActor())
		{
			if (ActorPtr->IsA<ADisplayClusterRootActor>())
			{
				return static_cast<ADisplayClusterRootActor*>(ActorPtr);
			}
		}

		return nullptr;
	}

	static inline bool ImplSetRootActor(ADisplayClusterRootActor* InRootActor, FDisplayClusterActorRef& InOutConfigurationRootActorRef)
	{
		if (ImplGetRootActor(InOutConfigurationRootActorRef) != InRootActor)
		{
			InOutConfigurationRootActorRef.SetSceneActor(InRootActor);

			return true;
		}

		return false;
	}
};
using namespace UE::DisplayCluster;

///////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfiguration
///////////////////////////////////////////////////////////////////
FDisplayClusterViewportConfiguration::FDisplayClusterViewportConfiguration()
	: Proxy(MakeShared<FDisplayClusterViewportConfigurationProxy, ESPMode::ThreadSafe>())
{ }

FDisplayClusterViewportConfiguration::~FDisplayClusterViewportConfiguration()
{ }

void FDisplayClusterViewportConfiguration::Initialize(FDisplayClusterViewportManager& InViewportManager)
{
	// Set weak refs to viewport manager and proxy
	ViewportManagerWeakPtr = InViewportManager.AsShared();
	Proxy->Initialize_GameThread(InViewportManager.GetViewportManagerProxy());
}

void FDisplayClusterViewportConfiguration::SetRootActor(ADisplayClusterRootActor* InRootActor, const EDisplayClusterRootActorType InRootActorType)
{
	check(IsInGameThread());

	switch (InRootActorType)
	{
	case EDisplayClusterRootActorType::Preview:
		bCurrentSceneNeedsToBeUpdated |= ConfigurationHelpers::ImplSetRootActor(InRootActor, PreviewRootActorRef);
		break;

	case EDisplayClusterRootActorType::Scene:
		bCurrentSceneNeedsToBeUpdated |= ConfigurationHelpers::ImplSetRootActor(InRootActor, SceneRootActorRef);
		break;

	case EDisplayClusterRootActorType::Configuration:
		ConfigurationHelpers::ImplSetRootActor(InRootActor, ConfigurationRootActorRef);
		break;

	case EDisplayClusterRootActorType::Any:
		bCurrentSceneNeedsToBeUpdated |= ConfigurationHelpers::ImplSetRootActor(InRootActor, PreviewRootActorRef);
		bCurrentSceneNeedsToBeUpdated |= ConfigurationHelpers::ImplSetRootActor(InRootActor, SceneRootActorRef);

		ConfigurationHelpers::ImplSetRootActor(InRootActor, ConfigurationRootActorRef);
		break;

	default:
		break;
	}
}

ADisplayClusterRootActor* FDisplayClusterViewportConfiguration::GetRootActor(const EDisplayClusterRootActorType InRootActorType) const
{
	switch (InRootActorType)
	{
	case EDisplayClusterRootActorType::Preview:
		return ConfigurationHelpers::ImplGetRootActor(PreviewRootActorRef);

	case EDisplayClusterRootActorType::Scene:
		return ConfigurationHelpers::ImplGetRootActor(SceneRootActorRef);

	case EDisplayClusterRootActorType::Configuration:
		return ConfigurationHelpers::ImplGetRootActor(ConfigurationRootActorRef);

	case EDisplayClusterRootActorType::Any:
	{
		ADisplayClusterRootActor* OutRootActor = ConfigurationHelpers::ImplGetRootActor(PreviewRootActorRef);
		if (!OutRootActor)
		{
			OutRootActor = ConfigurationHelpers::ImplGetRootActor(SceneRootActorRef);
			if (!OutRootActor)
			{
				OutRootActor = ConfigurationHelpers::ImplGetRootActor(ConfigurationRootActorRef);
			}
		}

		return OutRootActor;
	}

	default:
		break;
	}

	return nullptr;
}

void FDisplayClusterViewportConfiguration::SetCurrentWorld(UWorld* InWorld)
{
	if (GetCurrentWorld() != InWorld)
	{
		if (FDisplayClusterViewportManager* ViewportManager = GetViewportManagerImpl())
		{
			const bool bNeedRestartScene = bCurrentSceneActive;

			ViewportManager->HandleEndScene();
			CurrentWorldRef.Reset();

			if (InWorld)
			{
				CurrentWorldRef = TWeakObjectPtr<UWorld>(InWorld);

				if (bNeedRestartScene)
				{
					ViewportManager->HandleStartScene();
				}
			}
		}
		else
		{
			CurrentWorldRef.Reset();
		}
	}
}

UWorld* FDisplayClusterViewportConfiguration::GetCurrentWorld() const
{
	check(IsInGameThread());

	if (!CurrentWorldRef.IsValid() || CurrentWorldRef.IsStale())
	{
		return nullptr;
	}

	return CurrentWorldRef.Get();
}

const UDisplayClusterConfigurationData* FDisplayClusterViewportConfiguration::GetConfigurationData() const
{
	ADisplayClusterRootActor* ConfigurationRootActor = GetRootActor(EDisplayClusterRootActorType::Configuration);

	return ConfigurationRootActor ? ConfigurationRootActor->GetConfigData() : nullptr;
}

const FDisplayClusterConfigurationICVFX_StageSettings* FDisplayClusterViewportConfiguration::GetStageSettings() const
{
	if (const UDisplayClusterConfigurationData* ConfigurationData = GetConfigurationData())
	{
		return &ConfigurationData->StageSettings;
	}

	return nullptr;
}

const FDisplayClusterConfigurationRenderFrame* FDisplayClusterViewportConfiguration::GetConfigurationRenderFrameSettings() const
{
	if (const UDisplayClusterConfigurationData* ConfigurationData = GetConfigurationData())
	{
		return &ConfigurationData->RenderFrameSettings;
	}

	return nullptr;
}

bool FDisplayClusterViewportConfiguration::IsCurrentWorldHasAnyType(const EWorldType::Type InWorldType1, const EWorldType::Type InWorldType2, const EWorldType::Type InWorldType3) const
{
	if (UWorld* CurrentWorld = GetCurrentWorld())
	{
		return (CurrentWorld->WorldType == InWorldType1 && InWorldType1 != EWorldType::None)
			|| (CurrentWorld->WorldType == InWorldType2 && InWorldType2 != EWorldType::None)
			|| (CurrentWorld->WorldType == InWorldType3 && InWorldType3 != EWorldType::None);
	}

	return false;
}

bool FDisplayClusterViewportConfiguration::IsRootActorWorldHasAnyType(const EDisplayClusterRootActorType InRootActorType, const EWorldType::Type InWorldType1, const EWorldType::Type InWorldType2, const EWorldType::Type InWorldType3) const
{
	if (ADisplayClusterRootActor* RootActor = GetRootActor(InRootActorType))
	{
		if (UWorld* CurrentWorld = RootActor->GetWorld())
		{
			return (CurrentWorld->WorldType == InWorldType1 && InWorldType1 != EWorldType::None)
				|| (CurrentWorld->WorldType == InWorldType2 && InWorldType2 != EWorldType::None)
				|| (CurrentWorld->WorldType == InWorldType3 && InWorldType3 != EWorldType::None);
		}
	}

	return false;
}

EDisplayClusterRenderFrameMode FDisplayClusterViewportConfiguration::GetRenderModeForPIE() const
{
#if WITH_EDITOR
	if (ADisplayClusterRootActor* PreviewRootActor = GetRootActor(EDisplayClusterRootActorType::Preview))
	{
		switch (PreviewRootActor->RenderMode)
		{
		case EDisplayClusterConfigurationRenderMode::SideBySide:
			return EDisplayClusterRenderFrameMode::SideBySide;

		case EDisplayClusterConfigurationRenderMode::TopBottom:
			return EDisplayClusterRenderFrameMode::TopBottom;

		default:
			break;
		}
	}
#endif

	return EDisplayClusterRenderFrameMode::Mono;
}

IDisplayClusterViewportManager* FDisplayClusterViewportConfiguration::GetViewportManager() const
{
	return GetViewportManagerImpl();
}

bool FDisplayClusterViewportConfiguration::ImplUpdateConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId, const TArray<FString>* InViewportNames)
{
	check(IsInGameThread());
	
	FDisplayClusterViewportManager* ViewportManager = GetViewportManagerImpl();
	if (!ViewportManager || !FDisplayClusterViewportConfigurationHelpers_RenderFrameSettings::UpdateRenderFrameConfiguration(InRenderMode, *this))
	{
		return false;
	}

	if (bCurrentSceneNeedsToBeUpdated)
	{
		// When the root actor changes, we have to ResetScene() to reinitialize the internal references of the projection policy.
		ViewportManager->HandleEndScene();
		ViewportManager->HandleStartScene();
	}

	FDisplayClusterViewportConfiguration_ViewportManager  ConfigurationViewportManager(*this);
	FDisplayClusterViewportConfiguration_Postprocess      ConfigurationPostprocess(*this);
	FDisplayClusterViewportConfiguration_ProjectionPolicy ConfigurationProjectionPolicy(*this);
	FDisplayClusterViewportConfiguration_ICVFX            ConfigurationICVFX(*this);

	// when InClusterNodeId==PreviewNodeAll, means that it is an undefined cluster node
	const FString ClusterNodeId = InClusterNodeId == DisplayClusterConfigurationStrings::gui::preview::PreviewNodeAll ? TEXT("") : InClusterNodeId;

	if (InViewportNames)
	{
		ConfigurationViewportManager.UpdateCustomViewports(*InViewportNames);

		// Do not use the cluster node name for this pass type
		SetClusterNodeId(FString());
	}
	else
	{
		ConfigurationViewportManager.UpdateClusterNodeViewports(ClusterNodeId);
		// Use only valid values of cluster node id
		SetClusterNodeId(ClusterNodeId);

	}

	ConfigurationICVFX.Update();
	ConfigurationProjectionPolicy.Update();
	ConfigurationICVFX.PostUpdate();

	ImplUpdateConfigurationVisibility();

	if (!InViewportNames)
	{
		// Update postprocess for current cluster node
		ConfigurationPostprocess.UpdateClusterNodePostProcess(ClusterNodeId);
	}

	FDisplayClusterViewportConfigurationHelpers_RenderFrameSettings::PostUpdateRenderFrameConfiguration(*this);

	return true;
}

bool FDisplayClusterViewportConfiguration::UpdateConfigurationForClusterNode(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId)
{
	return ImplUpdateConfiguration(InRenderMode, InClusterNodeId, nullptr);
}

bool FDisplayClusterViewportConfiguration::UpdateConfigurationForViewportsList(EDisplayClusterRenderFrameMode InRenderMode, const TArray<FString>& InViewportNames)
{
	return ImplUpdateConfiguration(InRenderMode, TEXT(""), &InViewportNames);
}

void FDisplayClusterViewportConfiguration::ImplUpdateConfigurationVisibility() const
{
	ADisplayClusterRootActor* SceneRootActor = GetRootActor(EDisplayClusterRootActorType::Scene);
	FDisplayClusterViewportManager* ViewportManager = GetViewportManagerImpl();

	// Hide root actor components for all viewports
	TSet<FPrimitiveComponentId> RootActorHidePrimitivesList;
	if (ViewportManager && SceneRootActor && SceneRootActor->GetHiddenInGamePrimitives(RootActorHidePrimitivesList))
	{
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
		{
			if (ViewportIt.IsValid())
			{
				ViewportIt->GetVisibilitySettingsImpl().SetRootActorHideList(RootActorHidePrimitivesList);
			}
		}
	}
}
