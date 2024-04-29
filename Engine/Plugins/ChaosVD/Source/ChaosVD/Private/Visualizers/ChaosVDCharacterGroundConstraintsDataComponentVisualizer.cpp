// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visualizers/ChaosVDCharacterGroundConstraintsDataComponentVisualizer.h"

#include "ChaosVDScene.h"
#include "ChaosVDTabsIDs.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "Actors/ChaosVDSolverInfoActor.h"
#include "Components/ChaosVDSolverCharacterGroundConstraintDataComponent.h"
#include "Settings/ChaosVDCharacterConstraintsVisualizationSettings.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "Utils/ChaosVDUserInterfaceUtils.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"
#include "Widgets/SChaosVDEnumFlagsMenu.h"
#include "Widgets/SChaosVDMainTab.h"
#include "Widgets/SChaosVDViewportToolbar.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

bool FChaosVDCharacterGroundConstraintVisualizationDataContext::IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags Flag) const
{
	const EChaosVDCharacterGroundConstraintDataVisualizationFlags FlagsAsParticleFlags = static_cast<EChaosVDCharacterGroundConstraintDataVisualizationFlags>(VisualizationFlags);
	return EnumHasAnyFlags(FlagsAsParticleFlags, Flag);
}

IMPLEMENT_HIT_PROXY(HChaosVDCharacterGroundConstraintProxy, HComponentVisProxy)


FChaosVDCharacterGroundConstraintDataComponentVisualizer::FChaosVDCharacterGroundConstraintDataComponentVisualizer()
{
	RegisterVisualizerMenus();
}

void FChaosVDCharacterGroundConstraintDataComponentVisualizer::RegisterVisualizerMenus()
{
		UToolMenus* ToolMenus = UToolMenus::Get();
	
	if (!ensure(ToolMenus))
	{
		return;
	}

	if (UToolMenu* Menu = ToolMenus->ExtendMenu(SChaosVDViewportToolbar::ShowMenuName))
	{
		FToolMenuSection& Section = Menu->AddSection("CharacterConstraintDataVisualization.Show", LOCTEXT("CharacterConstraintDataVisualizationShowMenuLabel", "Character Constraints Data Visualization"));
		
		Section.AddSubMenu(TEXT("CharacterConstraintDataVisualizationFlags"), LOCTEXT("CharacterConstraintDataVisualizationFlagsMenuLabel", "Character Constraints Data Flags"), LOCTEXT("CharacterConstraintDataVisualizationFlagsMenuToolTip", "Set of flags to enable/disable visibility of specific types of Character Constraints data"), FNewToolMenuDelegate::CreateLambda([](UToolMenu* Menu)
		                   {
			                   TSharedRef<SWidget> VisualizationFlagsWidget = SNew(SChaosVDEnumFlagsMenu<EChaosVDCharacterGroundConstraintDataVisualizationFlags>)
				                   .CurrentValue_Static(&UChaosVDCharacterConstraintsVisualizationSettings::GetCharacterGroundConstraintDataVisualizationFlags)
				                   .OnEnumSelectionChanged_Static(&UChaosVDCharacterConstraintsVisualizationSettings::SetCharacterGroundConstraintDataVisualizationFlags);
			
			                   FToolMenuEntry FlagsMenuEntry = FToolMenuEntry::InitWidget("CharacterConstraintDataVisualizationFlags", VisualizationFlagsWidget,FText::GetEmpty());
			                   Menu->AddMenuEntry(NAME_None, FlagsMenuEntry);
		                   }),
		                   false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), TEXT("ClassIcon.Character")));

		using namespace Chaos::VisualDebugger::Utils;
		Section.AddSubMenu(TEXT("CharacterConstraintDataVisualizationSettings"), LOCTEXT("CharacterConstraintDataVisualizationMenuLabel", "Character Constraints Visualization Settings"), LOCTEXT("CharacterConstraintDataVisualizationMenuToolTip", "Options to change how the recorded Character Constraints data is debug drawn"), FNewToolMenuDelegate::CreateStatic(&CreateMenuEntryForDefaultObject<UChaosVDCharacterConstraintsVisualizationSettings>, EChaosVDSaveSettingsOptions::ShowSaveButton),
		                   false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), TEXT("Icons.Toolbar.Settings")));
	}
}

void FChaosVDCharacterGroundConstraintDataComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UChaosVDSolverCharacterGroundConstraintDataComponent* ConstraintDataComponent = Cast<UChaosVDSolverCharacterGroundConstraintDataComponent>(Component);
	if (!ConstraintDataComponent)
	{
		return;
	}
	
	AChaosVDSolverInfoActor* SolverInfoActor = Cast<AChaosVDSolverInfoActor>(Component->GetOwner());
	if (!SolverInfoActor)
	{
		return;
	}

	if (!SolverInfoActor->IsVisible())
	{
		return;
	}

	const TSharedPtr<FChaosVDScene> CVDScene = SolverInfoActor->GetScene().Pin();
	if (!CVDScene)
	{
		return;
	}

	const TSharedPtr<FChaosVDRecording> CVDRecording = CVDScene->LoadedRecording;
	if (!CVDRecording)
	{
		return;
	}

	FChaosVDCharacterGroundConstraintVisualizationDataContext VisualizationContext;
	VisualizationContext.CVDScene = CVDScene;
	VisualizationContext.SpaceTransform = SolverInfoActor->GetSimulationTransform();
	VisualizationContext.SolverInfoActor = SolverInfoActor;

	VisualizationContext.VisualizationFlags = static_cast<uint32>(UChaosVDCharacterConstraintsVisualizationSettings::GetCharacterGroundConstraintDataVisualizationFlags());
	VisualizationContext.DebugDrawSettings = GetDefault<UChaosVDCharacterConstraintsVisualizationSettings>();

	if (!VisualizationContext.IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags::EnableDraw))
	{
		return;
	}

	// If nothing is selected, fallback to draw all character ground constraints
	const bool bDrawOnlySelected = VisualizationContext.IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags::OnlyDrawSelected) && ConstraintDataComponent->GetCurrentSelectionHandle().IsSelected();
	if (bDrawOnlySelected)
	{
		if (const TSharedPtr<FChaosVDCharacterGroundConstraint> Constraint = ConstraintDataComponent->GetCurrentSelectionHandle().GetData().Pin())
		{
			VisualizationContext.DataSelectionHandle = FChaosVDCharacterGroundConstraintSelectionHandle(Constraint);
			DrawConstraint(Component, *Constraint, VisualizationContext, View, PDI);
		}
	}
	else
	{
		for (const TSharedPtr<FChaosVDCharacterGroundConstraint>& Constraint : ConstraintDataComponent->GetAllConstraints())
		{
			if (Constraint)
			{
				VisualizationContext.DataSelectionHandle = FChaosVDCharacterGroundConstraintSelectionHandle(Constraint);
				DrawConstraint(Component, *Constraint, VisualizationContext, View, PDI);
			}
		}
	}
}

bool FChaosVDCharacterGroundConstraintDataComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	const HChaosVDCharacterGroundConstraintProxy* ConstraintDataProxy = HitProxyCast<HChaosVDCharacterGroundConstraintProxy>(VisProxy);
	if (ConstraintDataProxy == nullptr)
	{
		return false;
	}
	
	if (const UChaosVDSolverCharacterGroundConstraintDataComponent* ConstraintDataComponent = Cast<UChaosVDSolverCharacterGroundConstraintDataComponent>(ConstraintDataProxy->Component.Get()))
	{
		// Bring the constraint details tab into focus if available
		const TSharedPtr<SChaosVDMainTab> MainTabToolkitHost = InViewportClient->GetModeTools() ? StaticCastSharedPtr<SChaosVDMainTab>(InViewportClient->GetModeTools()->GetToolkitHost()) : nullptr;
		if (const TSharedPtr<FTabManager> TabManager = MainTabToolkitHost ? MainTabToolkitHost->GetTabManager() : nullptr)
		{
			TabManager->TryInvokeTab(FChaosVDTabID::CharacterGroundConstraintDataDetails);
		}

		const_cast<UChaosVDSolverCharacterGroundConstraintDataComponent*>(ConstraintDataComponent)->SelectConstraint(ConstraintDataProxy->DataSelectionHandle);

		return true;
	}

	return false;
}

void FChaosVDCharacterGroundConstraintDataComponentVisualizer::DrawConstraint(const UActorComponent* Component, const FChaosVDCharacterGroundConstraint& InConstraintData, FChaosVDCharacterGroundConstraintVisualizationDataContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	using namespace Chaos::VisualDebugger::Utils;

	if (!VisualizationContext.IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags::DrawDisabled))
	{
		if (InConstraintData.State.bDisabled)
		{
			return;
		}
	}

	if (!Component)
	{
		return;
	}

	const UChaosVDCharacterConstraintsVisualizationSettings* DebugDrawSettings =  Cast<UChaosVDCharacterConstraintsVisualizationSettings>(VisualizationContext.DebugDrawSettings);
	if (!DebugDrawSettings)
	{
		return;
	}

	if (!PDI)
	{
		return;
	}

	if (!View)
	{
		return;
	}

	const FChaosVDParticleDataWrapper* CharacterParticleData = nullptr;

	if (AChaosVDParticleActor* CharacterParticle = VisualizationContext.SolverInfoActor->GetParticleActor(InConstraintData.CharacterParticleIndex))
	{
		CharacterParticleData = CharacterParticle->GetParticleData();
	}

	if (!CharacterParticleData)
	{
		return;
	}

	if (!CharacterParticleData->ParticleMassProps.HasValidData())
	{
		// If we don't have mass data, all the following calculations will be off
		// TODO: Should we draw just a line between the two particles as fallback?
		return;
	}

	PDI->SetHitProxy(new HChaosVDCharacterGroundConstraintProxy(Component, VisualizationContext.DataSelectionHandle));

	const float LineThickness = InConstraintData.bIsSelectedInEditor ?  DebugDrawSettings->BaseLineThickness * 1.5f :  DebugDrawSettings->BaseLineThickness;

	const FVector CharacterPos = CharacterParticleData->ParticlePositionRotation.MX;
	const FVector UpDir = InConstraintData.Settings.VerticalAxis;

	const float GroundDistance = InConstraintData.Data.GroundDistance;
	const float TargetHeight = InConstraintData.Settings.TargetHeight;

	// TODO: The target data is valid for pre-sim positions and the force/torque for post sim
	// but we don't have a way of differentiating here which state the particle is in so just draw
	// everything for now and leave it up to the user to interpret what they're seeing

	if (VisualizationContext.IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags::TargetDeltaPosition))
	{
		FChaosVDDebugDrawUtils::DrawArrowVector(PDI, CharacterPos, CharacterPos + InConstraintData.Data.TargetDeltaPosition, FText::GetEmpty(), FColor::Blue, DebugDrawSettings->DepthPriority, 0.5f * LineThickness);
	}

	if (VisualizationContext.IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags::TargetDeltaFacing))
	{
		const FVector Forward = CharacterParticleData->ParticlePositionRotation.MR * FVector::XAxisVector * DebugDrawSettings->GeneralScale * 10.0f;
		const FVector TargetForward = FQuat(UpDir, InConstraintData.Data.TargetDeltaFacing) * Forward;
		FChaosVDDebugDrawUtils::DrawArrowVector(PDI, CharacterPos, CharacterPos + Forward, FText::GetEmpty(), FColor::Silver, DebugDrawSettings->DepthPriority, 0.25f * LineThickness);
		FChaosVDDebugDrawUtils::DrawArrowVector(PDI, CharacterPos, CharacterPos + TargetForward, FText::GetEmpty(), FColor::White, DebugDrawSettings->DepthPriority, 0.25f * LineThickness);
	}

	if (VisualizationContext.IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags::GroundQueryDistance))
	{
		if (GroundDistance >= TargetHeight)
		{
			if (GroundDistance <= 4.0f * TargetHeight)
			{
				FChaosVDDebugDrawUtils::DrawLine(PDI, CharacterPos, CharacterPos - UpDir * TargetHeight, FColor::Green, FText::GetEmpty(), DebugDrawSettings->DepthPriority, LineThickness);
				FChaosVDDebugDrawUtils::DrawLine(PDI, CharacterPos - UpDir * TargetHeight, CharacterPos - UpDir * GroundDistance, FColor::Silver, FText::GetEmpty(), DebugDrawSettings->DepthPriority, LineThickness);
			}
		}
		else
		{
			FChaosVDDebugDrawUtils::DrawLine(PDI, CharacterPos, CharacterPos - UpDir * GroundDistance, FColor::Green, FText::GetEmpty(), DebugDrawSettings->DepthPriority, LineThickness);
			FChaosVDDebugDrawUtils::DrawLine(PDI, CharacterPos - UpDir * GroundDistance, CharacterPos - UpDir * TargetHeight, FColor::Red, FText::GetEmpty(), DebugDrawSettings->DepthPriority, LineThickness);
		}
	}

	if (VisualizationContext.IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags::GroundQueryNormal))
	{
		if (GroundDistance < 4.0f * TargetHeight)
		{
			const FVector ScaledGroundNormal = 10.0f * InConstraintData.Data.GroundNormal * DebugDrawSettings->GeneralScale;
			const FVector GroundPos = CharacterPos - UpDir * GroundDistance;
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, GroundPos, GroundPos + ScaledGroundNormal, FText::GetEmpty(), FColor::Cyan, DebugDrawSettings->DepthPriority, 0.25f * LineThickness);
		}
	}
	
	if (VisualizationContext.IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags::AppliedNormalForce))
	{
		const FVector NormalForce = DebugDrawSettings->ForceScale * InConstraintData.Data.GroundNormal.Dot(InConstraintData.State.SolverAppliedForce) * InConstraintData.Data.GroundNormal;
		FChaosVDDebugDrawUtils::DrawArrowVector(PDI, CharacterPos, CharacterPos + NormalForce, FText::GetEmpty(), DebugDrawSettings->NormalForceColor, DebugDrawSettings->DepthPriority, LineThickness);
	}

	if (VisualizationContext.IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags::AppliedRadialForce))
	{
		const FVector RadialForce = DebugDrawSettings->ForceScale * (InConstraintData.State.SolverAppliedForce - InConstraintData.Data.GroundNormal.Dot(InConstraintData.State.SolverAppliedForce) * InConstraintData.Data.GroundNormal);
		FChaosVDDebugDrawUtils::DrawArrowVector(PDI, CharacterPos, CharacterPos + RadialForce, FText::GetEmpty(), DebugDrawSettings->NormalForceColor, DebugDrawSettings->DepthPriority, LineThickness);
	}

	if (VisualizationContext.IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags::AppliedTorque))
	{
		const FVector Torque = DebugDrawSettings->TorqueScale * InConstraintData.State.SolverAppliedTorque;
		FChaosVDDebugDrawUtils::DrawArrowVector(PDI, CharacterPos, CharacterPos + Torque, FText::GetEmpty(), DebugDrawSettings->TorqueColor, DebugDrawSettings->DepthPriority, LineThickness);
	}

	PDI->SetHitProxy(nullptr);
}

#undef LOCTEXT_NAMESPACE
