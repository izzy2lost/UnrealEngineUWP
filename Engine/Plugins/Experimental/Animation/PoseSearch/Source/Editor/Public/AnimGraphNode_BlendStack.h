// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_AssetPlayerBase.h"
#include "PoseSearch/AnimNode_BlendStack.h"
#include "AnimGraphNode_BlendStack.generated.h"

class UAnimGraphNode_BlendStackInput;

UCLASS(MinimalAPI, Abstract, Experimental)
class UAnimGraphNode_BlendStack_Base : public UAnimGraphNode_AssetPlayerBase
{
	GENERATED_BODY()

	virtual void GetOutputLinkAttributes(FNodeAttributeArray& OutAttributes) const override;
	virtual void OnProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData) override;

	virtual bool DoesSupportTimeForTransitionGetter() const override;
	virtual UAnimationAsset* GetAnimationAsset() const override;
	virtual const TCHAR* GetTimePropertyName() const override;
	virtual UScriptStruct* GetTimePropertyStruct() const override;

	virtual void PostPlacedNewNode() override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual void JumpToDefinition() const override;
	virtual void DestroyNode() override;
	virtual void PostPasteNode() override;
	virtual void Serialize(FArchive& Ar) override;

	// Helper function for compilation
	void ExpandGraphAndProcessNodes(
		UEdGraph* SourceGraph, 
		UAnimGraphNode_Base* SourceRootNode, TArrayView<UAnimGraphNode_BlendStackInput*> SourceInputNode,
		IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData,
		UAnimGraphNode_Base*& OutRootNode, TArrayView<UAnimGraphNode_BlendStackInput*> OutInputNodes);

	virtual TArray<UEdGraph*> GetSubGraphs() const override;

protected:
	virtual FAnimNode_BlendStack_Standalone* GetBlendStackNode() const PURE_VIRTUAL(UAnimGraphNode_BlendStack_Base::GetBlendStackNode, return nullptr;);
	virtual int32 GetMaxActiveBlends() const;

private:

	void CreateGraph();

	UPROPERTY()
	TObjectPtr<UEdGraph> BoundGraph = nullptr;
};

UCLASS(MinimalAPI, Experimental)
class UAnimGraphNode_BlendStack : public UAnimGraphNode_BlendStack_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_BlendStack Node;

	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetMenuCategory() const override;
	virtual void BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog) override;

protected:
	virtual FAnimNode_BlendStack_Standalone* GetBlendStackNode() const override { return (FAnimNode_BlendStack_Standalone*)(&Node); }
};