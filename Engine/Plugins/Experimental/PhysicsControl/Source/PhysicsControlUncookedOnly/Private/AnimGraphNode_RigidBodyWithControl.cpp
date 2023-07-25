// Copyright Epic Games, Inc. All Rights Reserved.

/* <<<< THIS IS TEMPORARY PROTOTYPE CODE >>>>
 *
 * This is essentially a copy + paste of the 
 * equivalent file in engine code and is 
 * intended for prototyping work as part of 
 * the Ch5 locomotion initiative.
 * 
 */

#include "AnimGraphNode_RigidBodyWithControl.h"
#include "AnimNodeEditModes.h"
#include "Features/IModularFeatures.h"
#include "Kismet2/CompilerResultsLog.h"
#include "AnimNode_RigidBodyWithControl.h"
#include "EditorModeManager.h"
#include "IPhysicsAssetRenderInterface.h"
// #include "IAnimNodeEditMode.h" <- removed as it can't be found during build and doesn't appear to be necessary

// Details includes
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"


#include "SceneManagement.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_RigidBodyWithControl

#define LOCTEXT_NAMESPACE "RigidBodyWithControl"

UAnimGraphNode_RigidBodyWithControl::UAnimGraphNode_RigidBodyWithControl(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_RigidBodyWithControl::GetControllerDescription() const
{
	return LOCTEXT("AnimGraphNode_RigidBodyWithControl_ControllerDescription", "Rigid body simulation with Control for physics asset");
}

FText UAnimGraphNode_RigidBodyWithControl::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_RigidBodyWithControl_Tooltip", "This simulates based on the skeletal mesh component's physics asset with control options");
}

FText UAnimGraphNode_RigidBodyWithControl::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText(LOCTEXT("AnimGraphNode_RigidBodyWithControl_NodeTitle", "RigidBodyWithControl"));
}

FLinearColor UAnimGraphNode_RigidBodyWithControl::GetNodeTitleColor() const
{
	return FLinearColor(1.0f, 0.0f, 1.0f); // <- Magenta - as a warning
}

FString UAnimGraphNode_RigidBodyWithControl::GetNodeCategory() const
{
	return TEXT("Animation|Dynamics");
}

void UAnimGraphNode_RigidBodyWithControl::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

void UAnimGraphNode_RigidBodyWithControl::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp, const bool bIsSelected, const bool bIsPoseWatchEnabled) const
{
	if (const FAnimNode_RigidBodyWithControl* const RuntimeRigidBodyNode = GetDebuggedAnimNode<FAnimNode_RigidBodyWithControl>())
	{
		if (UPhysicsAsset* const PhysicsAsset = RuntimeRigidBodyNode->GetPhysicsAsset())
		{
			IPhysicsAssetRenderInterface& PhysicsAssetRenderInterface = IModularFeatures::Get().GetModularFeature<IPhysicsAssetRenderInterface>("PhysicsAssetRenderInterface");

			// Draw Bodies.
			if (bIsSelected || (bIsPoseWatchEnabled && PoseWatchElementBodies.IsValid() && PoseWatchElementBodies->GetIsVisible()))
			{
				FColor PrimitiveColorOverride = FColor::Transparent;

				// Get primitive color from pose watch component.
				if (!bIsSelected)
				{
					PrimitiveColorOverride = PoseWatchElementBodies->GetColor();
					PrimitiveColorOverride.A = 255;
				}

				PhysicsAssetRenderInterface.DebugDrawBodies(PreviewSkelMeshComp, PhysicsAsset, PDI, PrimitiveColorOverride);
			}

			// Draw Constraints.
			if (bIsSelected || (bIsPoseWatchEnabled && PoseWatchElementConstraints.IsValid() && PoseWatchElementConstraints->GetIsVisible()))
			{
				PhysicsAssetRenderInterface.DebugDrawConstraints(PreviewSkelMeshComp, PhysicsAsset, PDI);
			}
		}
		
		
		// Draw Space Controls
		if (bIsSelected || (bIsPoseWatchEnabled && PoseWatchElementWorldSpaceControls.IsValid() && PoseWatchElementWorldSpaceControls->GetIsVisible()))
		{
			for (const TMap<FName, FControlRecord>::ElementType& NameRecordPair : RuntimeRigidBodyNode->ControlRecords)
			{
				const FControlRecord& ControlRecord = NameRecordPair.Value;
				if (!ControlRecord.JointHandle || !ControlRecord.IsEnabled())
				{
					continue;
				}
#if 0
				// TODO get the targets from the control record and draw them with respect to the physical bodies
				PDI->DrawPoint(ControlRootTransform.GetTranslation(), FLinearColor::Blue, 5.0f, SDPG_Foreground);
				PDI->DrawLine(BodyPosition, TargetPosition, FLinearColor::Yellow, SDPG_Foreground);
#endif
			}
		}
	}
}

void UAnimGraphNode_RigidBodyWithControl::OnPoseWatchChanged(const bool IsPoseWatchEnabled, TObjectPtr<UPoseWatch> InPoseWatch, FEditorModeTools& InModeTools, FAnimNode_Base* InRuntimeNode)
{
	Super::OnPoseWatchChanged(IsPoseWatchEnabled, InPoseWatch, InModeTools, InRuntimeNode);

	UPoseWatch* const PoseWatch = InPoseWatch.Get();

	if (PoseWatch)
	{
		// A new pose watch has been created for this node - add node specific pose watch components.
		PoseWatchElementBodies = InPoseWatch.Get()->FindOrAddElement(FText(LOCTEXT("PoseWatchElementLabel_RigidBodyWithControl_PhysicsBodies", "Physics Bodies")), TEXT("PhysicsAssetEditor.Tree.Body"));
		PoseWatchElementConstraints = InPoseWatch.Get()->FindOrAddElement(FText(LOCTEXT("PoseWatchElementLabel__RigidBodyWithControl_PhysicsConstraints", "Physics Constraints")), TEXT("PhysicsAssetEditor.Tree.Constraint"));
		PoseWatchElementParentSpaceControls = InPoseWatch.Get()->FindOrAddElement(FText(LOCTEXT("PoseWatchElementLabel_RigidBodyWithControl_ParentSpaceControls", "Parent Space Controls")), TEXT("PhysicsAssetEditor.Tree.Body"));
		PoseWatchElementWorldSpaceControls = InPoseWatch.Get()->FindOrAddElement(FText(LOCTEXT("PoseWatchElementLabel_RigidBodyWithControl_WorldSpaceControls", "World Space Controls")), TEXT("PhysicsAssetEditor.Tree.Body"));

		check(PoseWatchElementConstraints.IsValid()); // Expect to find a valid component;
		if (PoseWatchElementConstraints.IsValid())
		{
			PoseWatchElementConstraints->SetHasColor(false);
		}
	}
}

void UAnimGraphNode_RigidBodyWithControl::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	Super::CustomizeDetails(DetailBuilder);

	IDetailCategoryBuilder& ViewportCategory = DetailBuilder.EditCategory(TEXT("Debug Visualization"));
	FDetailWidgetRow& WidgetRow = ViewportCategory.AddCustomRow(LOCTEXT("ToggleDebugVisualizationButtonRow", "DebugVisualization"));
	FAnimNode_RigidBodyWithControl* const RigidBodyNode = static_cast<FAnimNode_RigidBodyWithControl*>(GetDebuggedAnimNode());

	WidgetRow
		[
			SNew(SHorizontalBox)
			// Show/Hide Bodies button.
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda([this](){ this->ToggleBodyVisibility(); return FReply::Handled(); })
				.ButtonColorAndOpacity_Lambda([this](){ return (AreAnyBodiesHidden()) ? FAppStyle::Get().GetSlateColor("Colors.AccentRed") : FAppStyle::Get().GetSlateColor("Colors.AccentGreen"); })
				.Content()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return (AreAnyBodiesHidden()) ? LOCTEXT("ShowAllBodiesButtonText", "Show All Bodies") : LOCTEXT("HideAllBodiesButtonText", "Hide All Bodies"); })
					.ToolTipText(LOCTEXT("ToggleBodyVisibilityButtonToolTip", "Toggle debug visualization of all physics bodies"))
				]
			]
			// Show/Hide Constraints button.
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda([this](){ this->ToggleConstraintVisibility(); return FReply::Handled(); })
				.ButtonColorAndOpacity_Lambda([this]() { return (AreAnyConstraintsHidden()) ? FAppStyle::Get().GetSlateColor("Colors.AccentRed") : FAppStyle::Get().GetSlateColor("Colors.AccentGreen"); })
				.Content()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return (AreAnyConstraintsHidden()) ? LOCTEXT("ShowAllConstraintsButtonText", "Show All Constraints") : LOCTEXT("HideAllConstraintsButtonText", "Hide All Constraints"); })
					.ToolTipText(LOCTEXT("ToggleConstraintVisibilityButtonToolTip", "Toggle debug visualization of all physics constriants"))
				]
			]
		];
}

void UAnimGraphNode_RigidBodyWithControl::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	IPhysicsAssetRenderInterface& PhysicsAssetRenderInterface = IModularFeatures::Get().GetModularFeature<IPhysicsAssetRenderInterface>("PhysicsAssetRenderInterface");
	PhysicsAssetRenderInterface.SaveConfig();
}

void UAnimGraphNode_RigidBodyWithControl::ToggleBodyVisibility()
{
	FAnimNode_RigidBodyWithControl* const RigidBodyNode = static_cast<FAnimNode_RigidBodyWithControl*>(GetDebuggedAnimNode());
	
	IPhysicsAssetRenderInterface& PhysicsAssetRenderInterface = IModularFeatures::Get().GetModularFeature<IPhysicsAssetRenderInterface>("PhysicsAssetRenderInterface");

	if (RigidBodyNode)
	{
		PhysicsAssetRenderInterface.ToggleShowAllBodies(RigidBodyNode->GetPhysicsAsset());
	}
}

void UAnimGraphNode_RigidBodyWithControl::ToggleConstraintVisibility()
{
	FAnimNode_RigidBodyWithControl* const RigidBodyNode = static_cast<FAnimNode_RigidBodyWithControl*>(GetDebuggedAnimNode());
	IPhysicsAssetRenderInterface& PhysicsAssetRenderInterface = IModularFeatures::Get().GetModularFeature<IPhysicsAssetRenderInterface>("PhysicsAssetRenderInterface");

	if (RigidBodyNode)
	{
		PhysicsAssetRenderInterface.ToggleShowAllConstraints(RigidBodyNode->GetPhysicsAsset());
	}
}

bool UAnimGraphNode_RigidBodyWithControl::AreAnyBodiesHidden() const
{
	FAnimNode_RigidBodyWithControl* const RigidBodyNode = static_cast<FAnimNode_RigidBodyWithControl*>(GetDebuggedAnimNode());
	IPhysicsAssetRenderInterface& PhysicsAssetRenderInterface = IModularFeatures::Get().GetModularFeature<IPhysicsAssetRenderInterface>("PhysicsAssetRenderInterface");

	if (RigidBodyNode)
	{
		return PhysicsAssetRenderInterface.AreAnyBodiesHidden(RigidBodyNode->GetPhysicsAsset());
	}

	return false;
}

bool UAnimGraphNode_RigidBodyWithControl::AreAnyConstraintsHidden() const
{
	FAnimNode_RigidBodyWithControl* const RigidBodyNode = static_cast<FAnimNode_RigidBodyWithControl*>(GetDebuggedAnimNode());
	IPhysicsAssetRenderInterface& PhysicsAssetRenderInterface = IModularFeatures::Get().GetModularFeature<IPhysicsAssetRenderInterface>("PhysicsAssetRenderInterface");

	if (RigidBodyNode)
	{
		return PhysicsAssetRenderInterface.AreAnyConstraintsHidden(RigidBodyNode->GetPhysicsAsset());
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
