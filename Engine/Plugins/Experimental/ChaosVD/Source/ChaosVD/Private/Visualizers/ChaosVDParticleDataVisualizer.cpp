// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visualizers/ChaosVDParticleDataVisualizer.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "ChaosVDScene.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"

void FChaosVDParticleDataVisualizer::DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	EChaosVDParticleDataVisualizationFlags VisualizationFlagsToUse = static_cast<EChaosVDParticleDataVisualizationFlags>(LocalVisualizationFlags);
	if (VisualizationFlagsToUse == EChaosVDParticleDataVisualizationFlags::None)
	{
		if (const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>())
		{
			VisualizationFlagsToUse = static_cast<EChaosVDParticleDataVisualizationFlags>(EditorSettings->GlobalParticleDataVisualizationFlags);
		}
		else
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Failed to retrive global visualization setting. Falling back to local settings"), ANSI_TO_TCHAR(__FUNCTION__));
		}	
	}
	
	FChaosVDVisualizationContext VisualizationContext;
	DataProvider.GetVisualizationContext(VisualizationContext);

	const FChaosVDParticleDataWrapper* ParticleDataViewer = DataProvider.GetParticleData();
	if (!ensure(ParticleDataViewer))
	{
		return;
	}

	// TODO: Implement scale settings for the vectors, and take into account the simulation space transform
	const float VelScale = 0.5f;		// [cm/s]
	const float AngVelScale = 50.0f;	// [rad/s]
	const float AccScale = 0.005f;		// [cm/s2]
	const float AngAccScale = 0.5f;		// [rad/s2]
	const float ImpScale = 0.001;		// [g.m/s]
	const float AngImpScale = 0.1f;		// [g.m2/s]

	const FVector& OwnerLocation = ParticleDataViewer->ParticlePositionRotation.MX;
	const FQuat& OwnerRotation = ParticleDataViewer->ParticlePositionRotation.MR;
	const FVector OwnerCoMLocation = OwnerLocation + OwnerRotation * ParticleDataViewer->ParticleMassProps.MCenterOfMass;


	if (ParticleDataViewer->ParticleVelocities.HasValidData())
	{
		if (EnumHasAnyFlags(VisualizationFlagsToUse, EChaosVDParticleDataVisualizationFlags::Velocity))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerCoMLocation, OwnerCoMLocation + VelScale * ParticleDataViewer->ParticleVelocities.MV, TEXT("Velocity"), FColor::Green);
		}

		if (EnumHasAnyFlags(VisualizationFlagsToUse, EChaosVDParticleDataVisualizationFlags::AngularVelocity))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerCoMLocation, OwnerCoMLocation + AngVelScale * ParticleDataViewer->ParticleVelocities.MW, TEXT("Angular Velocity"), FColor::Blue);
		}
	}

	if (ParticleDataViewer->ParticleDynamics.HasValidData())
	{
		if (EnumHasAnyFlags(VisualizationFlagsToUse, EChaosVDParticleDataVisualizationFlags::Acceleration))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerCoMLocation, OwnerCoMLocation + AccScale * ParticleDataViewer->ParticleDynamics.MAcceleration, TEXT("Acceleration"), FColor::Orange);
		}
		
		if (EnumHasAnyFlags(VisualizationFlagsToUse, EChaosVDParticleDataVisualizationFlags::AngularAcceleration))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerCoMLocation, OwnerCoMLocation + AngAccScale * ParticleDataViewer->ParticleDynamics.MAngularAcceleration, TEXT("Angular Acceleration"), FColor::Purple);
		}

		if (EnumHasAnyFlags(VisualizationFlagsToUse, EChaosVDParticleDataVisualizationFlags::LinearImpulse))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerCoMLocation, OwnerCoMLocation + ImpScale * ParticleDataViewer->ParticleDynamics.MLinearImpulseVelocity, TEXT("Linear Implulse Velocity"), FColor::Purple);
		}

		if (EnumHasAnyFlags(VisualizationFlagsToUse, EChaosVDParticleDataVisualizationFlags::AngularImpulse))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerCoMLocation, OwnerCoMLocation + AngImpScale * ParticleDataViewer->ParticleDynamics.MAngularImpulseVelocity, TEXT("Angular Implulse Velocity"), FColor::Emerald);
		}
	}

	// TODO: This is a Proof of concept to test how debug draw connectivity data will look
	// This will get re-implemented when we move particle data visualization to the new visualization system currently used for Collision Data
	if (ParticleDataViewer->ParticleCluster.HasValidData())
	{
		if (EnumHasAnyFlags(VisualizationFlagsToUse, EChaosVDParticleDataVisualizationFlags::ClusterConnectivityEdge))
		{
			for (const FChaosVDConnectivityEdge& ConnectivityEdge : ParticleDataViewer->ParticleCluster.ConnectivityEdges)
			{
				if (TSharedPtr<FChaosVDScene> ScenePtr = VisualizationContext.CVDScene.Pin())
				{
					if (AChaosVDParticleActor* SiblingParticle = ScenePtr->GetParticleActor(VisualizationContext.SolverID, ConnectivityEdge.SiblingParticleID))
					{
						if (const FChaosVDParticleDataWrapper* SiblingParticleData = SiblingParticle->GetParticleData())
						{
							FVector BoxExtents(2,2,2);
							FTransform BoxTransform(ParticleDataViewer->ParticlePositionRotation.MR, ParticleDataViewer->ParticlePositionRotation.MX);
							FChaosVDDebugDrawUtils::DrawBox(PDI, BoxExtents, FColor::Black, BoxTransform, nullptr, ESceneDepthPriorityGroup::SDPG_Foreground);
							FChaosVDDebugDrawUtils::DrawLine(PDI, ParticleDataViewer->ParticlePositionRotation.MX, SiblingParticleData->ParticlePositionRotation.MX, FColor::Blue, FString::Printf(TEXT("Strain [%f]"), ConnectivityEdge.Strain), ESceneDepthPriorityGroup::SDPG_Foreground);
						}
					}	
				}
			}
		}
	}
}
