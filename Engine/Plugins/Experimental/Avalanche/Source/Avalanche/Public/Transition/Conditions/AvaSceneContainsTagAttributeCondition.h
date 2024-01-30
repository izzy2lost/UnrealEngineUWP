// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTagHandle.h"
#include "AvaTagHandleContainer.h"
#include "AvaTransitionEnums.h"
#include "Conditions/AvaTransitionCondition.h"
#include "AvaSceneContainsTagAttributeCondition.generated.h"

class UAvaSceneSubsystem;
struct FAvaTransitionScene;

USTRUCT()
struct FAvaSceneContainsAttributeConditionInstanceData
{
	GENERATED_BODY()
};

USTRUCT(DisplayName="Scene contains tag attribute", Category="Scene Attributes")
struct AVALANCHE_API FAvaSceneContainsTagAttributeCondition : public FAvaTransitionCondition
{
	GENERATED_BODY()

	using FInstanceDataType = FAvaSceneContainsAttributeConditionInstanceData;

	//~ Begin FAvaTransitionCondition
	virtual FText GenerateDescription() const override;
	//~ End FAvaTransitionCondition

	//~ Begin FStateTreeNodeBase
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& InLinker) override;
	//~ End FStateTreeNodeBase

	//~ Begin FStateTreeConditionBase
	virtual bool TestCondition(FStateTreeExecutionContext& InContext) const override;
	//~ End FStateTreeConditionBase

	bool ContainsTagAttribute(FStateTreeExecutionContext& InContext) const;

	TArray<const FAvaTransitionScene*> GetTransitionScenes(FStateTreeExecutionContext& InContext) const;

	/** Whether the scene to check should be this scene or other scene */
	UPROPERTY(EditAnywhere, Category="Parameter")
	EAvaTransitionSceneType SceneType = EAvaTransitionSceneType::This;

	/** Which Layer should be queried for the Scene Attributes */
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(EditCondition="SceneType == EAvaTransitionSceneType::Other", EditConditionHides))
	EAvaTransitionLayerCompareType LayerType = EAvaTransitionLayerCompareType::Same;

	/** Specific layer tags to check */
	UPROPERTY(EditAnywhere, Category="Parameter", meta=(EditCondition="SceneType == EAvaTransitionSceneType::Other && LayerType==EAvaTransitionLayerCompareType::MatchingTag", EditConditionHides))
	FAvaTagHandleContainer SpecificLayers;

	/** The Tag Attribute to check if it's contained in the scene(s) or not */
	UPROPERTY(EditAnywhere, Category="Parameter")
	FAvaTagHandle TagAttribute;

	/** Whether to invert the condition (i.e. if true, would become "does NOT contain attribute")*/
	UPROPERTY(EditAnywhere, Category="Parameter")
	bool bInvert = false;

	TStateTreeExternalDataHandle<UAvaSceneSubsystem> SceneSubsystemHandle;
};
