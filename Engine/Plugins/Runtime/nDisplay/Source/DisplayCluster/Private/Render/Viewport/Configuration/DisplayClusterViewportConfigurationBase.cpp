// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfigurationBase.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/Postprocess/DisplayClusterViewportPostProcessManager.h"
#include "Render/Viewport/Postprocess/DisplayClusterViewportPostProcessOutputRemap.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "DisplayClusterViewportConfigurationHelpers.h"
#include "DisplayClusterConfigurationTypes.h"

#include "DisplayClusterRootActor.h"

#include "Engine/StaticMesh.h"
#include "ProceduralMeshComponent.h"

#include "Misc/DisplayClusterLog.h"

#include "Components/DisplayClusterCameraComponent.h"

///////////////////////////////////////////////////////////////////
// Copied from "TextureShareDisplayCluster/Misc/TextureShareDisplayClusterStrings.h"
namespace TextureShareDisplayClusterStrings
{
	namespace Postprocess
	{
		static constexpr auto TextureShare = TEXT("TextureShare");
	}

	namespace Projection
	{
		static constexpr auto TextureShare = TEXT("textureshare");
	}
};

namespace UE::DisplayCluster::Viewport::Configuration
{
	/**
	 * Getting a valid cluster node name.
	 * An empty cluster node name means using the viewports of the entire cluster.
	 */
	static inline FString GetValidClusterNodeId(const FString& InClusterNodeId)
	{
		// when InClusterNodeId==PreviewNodeAll, means that it is an undefined cluster node
		const FString ClusterNodeId = InClusterNodeId == DisplayClusterConfigurationStrings::gui::preview::PreviewNodeAll ? TEXT("") : InClusterNodeId;

		return ClusterNodeId;
	}
};
using namespace UE::DisplayCluster::Viewport::Configuration;

TArray<FString> FDisplayClusterViewportConfigurationBase::DisabledPostprocessNames;

///////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationBase
///////////////////////////////////////////////////////////////////
void FDisplayClusterViewportConfigurationBase::UpdateClusterNodeViewports(const FString& InClusterNodeId)
{
	// The input cluster node name may contain special values. Obtain a valid value.
	const FString ClusterNodeId = GetValidClusterNodeId(InClusterNodeId);

	// Initialize variable EntireClusterViewports from configuration
	ImplInitializeEntireClusterViewportsList();

	if (ClusterNodeId.IsEmpty())
	{
		// If the cluster node name is empty, we use the viewports of the entire cluster 
		CurrentFrameViewports = EntireClusterViewports;
	}
	else
	{
		// Otherwise, we only use the viewports for one cluster node
		CurrentFrameViewports.Reset();
		for (const FDisplayClusterViewportConfigurationInstanceData& ViewportData : EntireClusterViewports)
		{
			if (ViewportData.ClusterNodeId == ClusterNodeId)
			{
				CurrentFrameViewports.Add(ViewportData);
			}
		}
	}

	// Create or update viewports from CurrentFrameViewports
	ImplUpdateViewports();

	// Updates warp policies only if the cluster node name is not empty.
	if (!ClusterNodeId.IsEmpty())
	{
		// when the cluster node name is empty, it means that the DCRA preview right now is initializing for the entire cluster.
		ImplUpdateViewportsWarpPolicy();
	}

	// Use only valid values of cluster node id
	RenderFrameSettings.ClusterNodeId = ClusterNodeId;
}

void FDisplayClusterViewportConfigurationBase::UpdateCustomViewports(const TArray<FString>& InViewportNames)
{
	// Initialize variable EntireClusterViewports from configuration
	ImplInitializeEntireClusterViewportsList();

	// Get the list of viewport instance data for the specified InViewportNames
	for (const FDisplayClusterViewportConfigurationInstanceData& ViewportData : EntireClusterViewports)
	{
		if (InViewportNames.Contains(ViewportData.ViewportId))
		{
			CurrentFrameViewports.Add(ViewportData);
		}
	}

	// Create or update viewports from CurrentFrameViewports
	ImplUpdateViewports();

	// Do not use the cluster node name for this pass type
	RenderFrameSettings.ClusterNodeId.Empty();
}

void FDisplayClusterViewportConfigurationBase::ImplUpdateViewports()
{
	// Delete an existing viewport when it is removed from the configuration or its configuration state is set as disabled.
	const TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> EntireClusterDCViewports = ViewportManager.ImplGetEntireClusterViewports();
	for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& Viewport : EntireClusterDCViewports)
	{
		// ignore internal resources
		if (Viewport.IsValid() && !EnumHasAnyFlags(Viewport->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource))
		{
			// Delete an existing viewport when it is removed from the configuration
			if (!ImplFindViewportInEntireCluster(Viewport->GetId()))
			{
				// we can safely remove viewports in a loop, because we use our own local array
				ViewportManager.ImplDeleteViewport(Viewport);
			}
		}
	}

	// Create or update viewports for current rendering frame
	for (FDisplayClusterViewportConfigurationInstanceData& ViewportData : CurrentFrameViewports)
	{
		ViewportData.CreateOrUpdateViewportInstance(RootActor, ViewportManager, RenderFrameSettings);
	}

	// Clear viewports warp policies for entire cluster
	for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& Viewport : ViewportManager.ImplGetEntireClusterViewports())
	{
		FDisplayClusterViewportConfigurationInstanceData::SetViewportWarpPolicy(Viewport, nullptr);
	}
}

void FDisplayClusterViewportConfigurationBase::ImplUpdateViewportsWarpPolicy()
{
	// Collect DC origin components used in current frame
	TArray<UDisplayClusterCameraComponent*> CurrentFrameViewPointCameraComponent;
	for (const FDisplayClusterViewportConfigurationInstanceData& ViewportData : CurrentFrameViewports)
	{
		if (ViewportData.ViewPointCameraComponent)
		{
			CurrentFrameViewPointCameraComponent.AddUnique(ViewportData.ViewPointCameraComponent);
		}
	}

	// Create reffered viewports from other cluster nodes and update warp policy on it
	for (UDisplayClusterCameraComponent* ViewPointComponent : CurrentFrameViewPointCameraComponent)
	{
		// The viewpoint camera component expects all viewports (from the entire cluster) that refer to them to be exists (created)  and updated.
		// this is for cluster rendering
		if (ViewPointComponent->ShouldUseEntireClusterViewports(&ViewportManager))
		{
			if (IDisplayClusterWarpPolicy* WarpPolicy = ViewPointComponent->GetWarpPolicy(&ViewportManager))
			{
				// Find the viewports that use this ViewPointComponent in the entire cluster.
				TArray<FDisplayClusterViewportConfigurationInstanceData> ViewPointViewports;
				const FString ViewPointComponentId = ViewPointComponent->GetName();
				for (const FDisplayClusterViewportConfigurationInstanceData& ViewportData : EntireClusterViewports)
				{
					if (ViewportData.Configuration.Camera == ViewPointComponentId)
					{
						ViewPointViewports.Add(ViewportData);
					}
				}

				// Initialize the warp policy for all found viewports:
				for (FDisplayClusterViewportConfigurationInstanceData& ViewportData : ViewPointViewports)
				{
					// If the viewport already exists in the current rendering frame, we simply assign a warp policy
					if (FDisplayClusterViewportConfigurationInstanceData const* CurrentFrameViewportData = ImplFindCurrentFrameViewports(ViewportData.ViewportId))
					{
						// Set the warp projection for the viewport from the current cluster node
						FDisplayClusterViewportConfigurationInstanceData::SetViewportWarpPolicy(CurrentFrameViewportData->Viewport, WarpPolicy);
					}
					else
					// when the viewport is on another cluster node, we must create it, because the warp policy requires viewports from the whole cluster.
					{
						// Create or update this viewport from another cluster node
						ViewportData.CreateOrUpdateViewportInstance(RootActor, ViewportManager, RenderFrameSettings);

						// Set the warp projection for the viewport from another cluster node
						FDisplayClusterViewportConfigurationInstanceData::SetViewportWarpPolicy(ViewportData.Viewport, WarpPolicy);
					}
				}
			}
		}
	}
}

void FDisplayClusterViewportConfigurationBase::ImplInitializeEntireClusterViewportsList()
{
	if (ConfigurationData.Cluster)
	{
		// Update and Create new viewports
		for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& ClusterNodeConfigurationIt : ConfigurationData.Cluster->Nodes)
		{
			if (const UDisplayClusterConfigurationClusterNode* ClusterNodeConfiguration = ClusterNodeConfigurationIt.Value)
			{
				for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportIt : ClusterNodeConfiguration->Viewports)
				{
					if (const UDisplayClusterConfigurationViewport* ConfigurationViewport = ViewportIt.Value)
					{
						const FString& ClusterNodeId = ClusterNodeConfigurationIt.Key;
						const FString& ViewportId = ViewportIt.Key;
						if (!ClusterNodeId.IsEmpty() && !ViewportId.IsEmpty() && ConfigurationViewport->IsViewportEnabled())
						{
							EntireClusterViewports.Add(FDisplayClusterViewportConfigurationInstanceData(ClusterNodeId, ViewportId, *ConfigurationViewport));
						}
					}
				}
			}
		}
	}
}

FDisplayClusterViewportConfigurationInstanceData const* FDisplayClusterViewportConfigurationBase::ImplFindViewportInEntireCluster(const FString& InViewportId) const
{
	return EntireClusterViewports.FindByPredicate([InViewportId](const FDisplayClusterViewportConfigurationInstanceData& ViewportItem)
		{
			return ViewportItem.ViewportId == InViewportId;
		});
}

FDisplayClusterViewportConfigurationInstanceData const* FDisplayClusterViewportConfigurationBase::ImplFindCurrentFrameViewports(const FString& InViewportId) const
{
	return CurrentFrameViewports.FindByPredicate([InViewportId](const FDisplayClusterViewportConfigurationInstanceData& ViewportItem)
		{
			return ViewportItem.ViewportId == InViewportId;
		});
}

void FDisplayClusterViewportConfigurationBase::AddInternalPostprocess(const FString& InPostprocessName)
{
	if (DisabledPostprocessNames.Find(InPostprocessName) == INDEX_NONE)
	{
		InternalPostprocessNames.AddUnique(InPostprocessName);
	}
}

void FDisplayClusterViewportConfigurationBase::UpdateClusterNodePostProcess(const FString& InClusterNodeId)
{
	// The input cluster node name may contain special values. Obtain a valid value.
	const FString ClusterNodeId = GetValidClusterNodeId(InClusterNodeId);

	if (ClusterNodeId.IsEmpty())
	{
		// this function expects a exists cluster node name
		return;
	}

	const UDisplayClusterConfigurationClusterNode* ClusterNode = ConfigurationData.Cluster->GetNode(ClusterNodeId);
	if (ClusterNode)
	{
		TSharedPtr<FDisplayClusterViewportPostProcessManager, ESPMode::ThreadSafe> PPManager = ViewportManager.GetPostProcessManager();
		if (PPManager.IsValid())
		{
			// Add TextureShare postprocess:
			if (ClusterNode->bEnableTextureShare && !RenderFrameSettings.bIsPreviewRendering)
			{
				AddInternalPostprocess(TextureShareDisplayClusterStrings::Postprocess::TextureShare);
			}

			{
				// Find unused PP:
				TArray<FString> UnusedPP;
				for (const FString& It : PPManager->GetPostprocess())
				{
					// Leave defined postprocess (dynamic reconf)
					bool IsDefinedPostProcess = ClusterNode->Postprocess.Contains(It);

					// Support InternalPostprocess dynamic reconf
					if (InternalPostprocessNames.Find(It) != INDEX_NONE)
					{
						IsDefinedPostProcess = true;
					}

					if (!IsDefinedPostProcess)
					{
						UnusedPP.Add(It);
					}
				}

				// Delete unused PP:
				for (const FString& It : UnusedPP)
				{
					PPManager->RemovePostprocess(It);
				}
			}

			// Create InternalPostprocess
			for (const FString& InternalPostprocessId : InternalPostprocessNames)
			{
				TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe> ExistPostProcess = PPManager->FindPostProcess(InternalPostprocessId);
				if (!ExistPostProcess.IsValid())
				{
					// Create postprocess instance
					FDisplayClusterConfigurationPostprocess ConfigurationPostprocess;
					ConfigurationPostprocess.Type = InternalPostprocessId;

					if (!PPManager->CreatePostprocess(InternalPostprocessId, &ConfigurationPostprocess))
					{
						// Can't create... Disable this postprocess
						DisabledPostprocessNames.AddUnique(InternalPostprocessId);

						UE_LOG(LogDisplayClusterViewport, Error, TEXT("Can't create postprocess '%s' required by cluster node '%s': Disabled"), *InternalPostprocessId, *ClusterNodeId);
					}
				}
			}

			// Create and update PP
			for (const TPair<FString, FDisplayClusterConfigurationPostprocess>& It : ClusterNode->Postprocess)
			{
				TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe> ExistPostProcess = PPManager->FindPostProcess(It.Key);
				if (ExistPostProcess.IsValid())
				{
					if (ExistPostProcess->IsConfigurationChanged(&It.Value))
					{
						PPManager->UpdatePostprocess(It.Key, &It.Value);
					}
				}
				else
				{
					PPManager->CreatePostprocess(It.Key, &It.Value);
				}
			}

			// Update OutputRemap PP
			{
				const struct FDisplayClusterConfigurationFramePostProcess_OutputRemap& OutputRemapCfg = ClusterNode->OutputRemap;
				if (OutputRemapCfg.bEnable)
				{
					switch (OutputRemapCfg.DataSource)
					{
					case EDisplayClusterConfigurationFramePostProcess_OutputRemapSource::StaticMesh:
						PPManager->GetOutputRemap()->UpdateConfiguration_StaticMesh(OutputRemapCfg.StaticMesh);
						break;

					case EDisplayClusterConfigurationFramePostProcess_OutputRemapSource::MeshComponent:
						if (!OutputRemapCfg.MeshComponentName.IsEmpty())
						{
							// Get the StaticMeshComponent
							UStaticMeshComponent* StaticMeshComponent = RootActor.GetComponentByName<UStaticMeshComponent>(OutputRemapCfg.MeshComponentName);
							if (StaticMeshComponent != nullptr)
							{
								PPManager->GetOutputRemap()->UpdateConfiguration_StaticMeshComponent(StaticMeshComponent);
							}
							else
							{
								// Get the procedural mesh component
								UProceduralMeshComponent* ProceduralMeshComponent = RootActor.GetComponentByName<UProceduralMeshComponent>(OutputRemapCfg.MeshComponentName);
								PPManager->GetOutputRemap()->UpdateConfiguration_ProceduralMeshComponent(ProceduralMeshComponent);
							}
						}
						else
						{
							PPManager->GetOutputRemap()->UpdateConfiguration_Disabled();
						}
						break;

					case EDisplayClusterConfigurationFramePostProcess_OutputRemapSource::ExternalFile:
						PPManager->GetOutputRemap()->UpdateConfiguration_ExternalFile(OutputRemapCfg.ExternalFile);
						break;

					default:
						PPManager->GetOutputRemap()->UpdateConfiguration_Disabled();
					}
				}
				else
				{
					PPManager->GetOutputRemap()->UpdateConfiguration_Disabled();
				}
			}
		}
	}
}

