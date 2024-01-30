// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalancheSVGEditor.h"
#include "AvaInteractiveToolsDelegates.h"
#include "IAvalancheInteractiveToolsModule.h"
#include "Modifiers/ActorModifierCoreStack.h"
#include "Modifiers/AvaBevelModifier.h"
#include "Modifiers/AvaExtrudeModifier.h"
#include "ProceduralMeshes/SVGDynamicMeshComponent.h"
#include "ProceduralMeshes/SVGStrokeComponent.h"
#include "SVGEngineSubsystem.h"
#include "SVGImporterEditorCommands.h"
#include "SVGShapesParentActor.h"
#include "Subsystems/ActorModifierCoreSubsystem.h"
#include "Tool/AvaSVGActorTool.h"

#define LOCTEXT_NAMESPACE "AvalancheSVGEditorModule"

void FAvaSVGEditorModule::StartupModule()
{
	FSVGImporterEditorCommands::Register();
	FAvaInteractiveToolsDelegates::GetRegisterToolsDelegate().AddRaw(this, &FAvaSVGEditorModule::RegisterTools);

	USVGEngineSubsystem::OnSVGActorSplit().BindRaw(this, &FAvaSVGEditorModule::OnSVGActorSplit);
}

void FAvaSVGEditorModule::ShutdownModule()
{
	FAvaInteractiveToolsDelegates::GetRegisterToolsDelegate().RemoveAll(this);

	USVGEngineSubsystem::OnSVGActorSplit().Unbind();
}

void FAvaSVGEditorModule::RegisterTools(IAvalancheInteractiveToolsModule* InModule)
{
	InModule->RegisterTool(
		IAvalancheInteractiveToolsModule::Get().CategoryNameActor,
		GetDefault<UAvaSVGActorTool>()->GetToolParameters()
	);
}

void FAvaSVGEditorModule::OnSVGActorSplit(ASVGShapesParentActor* InSVGShapesParent)
{
	if (!InSVGShapesParent)
	{
		return;
	}

	const UActorModifierCoreSubsystem* ModifierCoreSubsystem = UActorModifierCoreSubsystem::Get();

	if (!ModifierCoreSubsystem)
	{
		return;
	}

	TMap<TObjectPtr<AActor>, TObjectPtr<USVGDynamicMeshComponent>> ShapesMap;
	InSVGShapesParent->GetShapes(ShapesMap);
	for (const TPair<TObjectPtr<AActor>, TObjectPtr<USVGDynamicMeshComponent>>& ShapePair : ShapesMap)
	{
		if (USVGDynamicMeshComponent* Shape = ShapePair.Value)
		{
			const float ExtrudeValue = Shape->GetExtrudeDepth();
			ESVGExtrudeType ExtrudeType = Shape->ExtrudeType;
			Shape->FlattenShape();

			if (AActor* ShapeActor = ShapePair.Key)
			{
				UActorModifierCoreStack* ModifierStack = ModifierCoreSubsystem->GetActorModifierStack(ShapeActor);
				if (!ModifierStack)
				{
					ModifierStack = ModifierCoreSubsystem->AddActorModifierStack(ShapeActor);
				}

				FActorModifierCoreStackInsertOp ExtrudeModifierInsertOp;
				ExtrudeModifierInsertOp.NewModifierName = GetDefault<UAvaExtrudeModifier>()->GetModifierName();

				if (!ModifierCoreSubsystem->GetAllowedModifiers(ShapeActor).Contains(ExtrudeModifierInsertOp.NewModifierName))
				{
					continue;
				}

				UActorModifierCoreBase* Modifier = nullptr;

				Modifier = ModifierStack->InsertModifier(ExtrudeModifierInsertOp);
				if (UAvaExtrudeModifier* ExtrudeModifier = Cast<UAvaExtrudeModifier>(Modifier))
				{
					EAvaExtrudeMode ExtrudeMode = EAvaExtrudeMode::Symmetrical;

					switch (ExtrudeType)
					{
						case ESVGExtrudeType::FrontFaceOnly:
							ExtrudeMode = EAvaExtrudeMode::Front;
							break;

						case ESVGExtrudeType::None:
						case ESVGExtrudeType::FrontBackMirror:
							ExtrudeMode = EAvaExtrudeMode::Symmetrical;
							break;

						default: ;
					}

					ExtrudeModifier->SetDepth(ExtrudeValue);
					ExtrudeModifier->SetExtrudeMode(ExtrudeMode);

					if (Shape->IsA<USVGStrokeComponent>())
					{
						ExtrudeModifier->SetCloseBack(false);
					}
				}

				if (Shape->Bevel > 0.0f)
				{
					FActorModifierCoreStackInsertOp BevelModifierInsertOp;
					BevelModifierInsertOp.NewModifierName = GetDefault<UAvaBevelModifier>()->GetModifierName();

					if (!ModifierCoreSubsystem->GetAllowedModifiers(ShapeActor).Contains(BevelModifierInsertOp.NewModifierName))
					{
						continue;
					}

					Modifier = ModifierStack->InsertModifier(BevelModifierInsertOp);
					if (UAvaBevelModifier* BevelModifier = Cast<UAvaBevelModifier>(Modifier))
					{
						BevelModifier->SetInset(Shape->Bevel);
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAvaSVGEditorModule, AvalancheSVGEditorModule)
