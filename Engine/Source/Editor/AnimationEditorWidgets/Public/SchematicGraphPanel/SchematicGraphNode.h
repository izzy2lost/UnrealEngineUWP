// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "SchematicGraphTag.h"

#define SCHEMATICGRAPHNODE_BODY(ClassName, SuperClass) \
SCHEMATICGRAPHELEMENT_BODY(ClassName, SuperClass, FSchematicGraphNode)

class ANIMATIONEDITORWIDGETS_API FSchematicGraphNode
{
public:

	SCHEMATICGRAPHELEMENT_BODY_BASE(FSchematicGraphNode)
	
	const FGuid& GetGuid() const { return Guid; }
	bool HasParentNode() const { return ParentNodeGuid.IsValid(); }
	const FGuid& GetParentNodeGuid() const { return ParentNodeGuid; }
	const FSchematicGraphNode* GetParentNode() const;
	int32 GetNumChildNodes() const { return ChildNodeGuids.Num(); }
	const TArray<FGuid>& GetChildNodeGuids() const { return ChildNodeGuids; }
	const FSchematicGraphNode* GetChildNode(int32 InChildNodeIndex) const;
	virtual ESchematicGraphVisibility::Type GetVisibilityForChildNodes() const { return GetVisibility(); }
	bool IsSelected() const { return bIsSelected; }
	void SetSelected(bool bSelected = true) { bIsSelected = bSelected; }
	virtual FVector2d GetPosition() const { return Position; }
	virtual FVector2d GetPositionOffset() const { return PositionOffset; }
	virtual void SetPositionOffset(const FVector2d& InPositionOffset) { PositionOffset = InPositionOffset; }
	virtual float GetScaleOffset() const { return ScaleOffset; }
	virtual void SetScaleOffset(float InScaleOffset) { ScaleOffset = InScaleOffset; }
	int32 GetNumLayers() const { return FMath::Min(Colors.Num(), Brushes.Num()); }
	virtual FLinearColor GetColor(int32 InLayerIndex) const { return Colors[InLayerIndex]; }
	virtual const FSlateBrush* GetBrush(int32 InLayerIndex) const { return Brushes[InLayerIndex]; }
	virtual bool IsAutoScaleEnabled() const { return false; }
	virtual const FText& GetToolTip() const { return ToolTip; }
	virtual ESchematicGraphPlacementConstraint::Type GetPlacement() const { return Placement; }
	virtual void SetPlacement(ESchematicGraphPlacementConstraint::Type InPlacement) { Placement = InPlacement; }
	virtual ESchematicGraphVisibility::Type GetVisibility() const;
	virtual void SetVisibility(ESchematicGraphVisibility::Type InVisibility) { Visibility = InVisibility; }
	virtual bool IsDragSupported() const { return bDragSupported; }
	virtual void SetDragSupported(bool InDragSupported) { bDragSupported = InDragSupported;}

	virtual FString GetDragDropDecoratorLabel() const;

	template<typename TagType = FSchematicGraphTag>
	TagType* AddTag(bool bNotify = true)
	{
		const TSharedPtr<FSchematicGraphTag> NewTag = MakeShareable(new TagType);
		NewTag->Model = Model;
		Tags.Add(NewTag);
		TagByGuid.Add(NewTag->GetGuid(), NewTag);

		if (bNotify)
		{
			NotifyTagAdded(NewTag);
		}
		return static_cast<TagType*>(NewTag.Get());
	}

	template<typename TagType = FSchematicGraphTag>
	const TagType* FindTag(const FGuid& InTagGuid) const
	{
		if(const TSharedPtr<FSchematicGraphTag>* ExistingTag = TagByGuid.Find(InTagGuid))
		{
			return Cast<TagType>(ExistingTag->Get());
		};
		return nullptr;
	}

	template<typename TagType = FSchematicGraphTag>
	const TagType* FindTagChecked(const FGuid& InTagGuid) const
	{
		if(const TSharedPtr<FSchematicGraphTag>* ExistingTag = TagByGuid.Find(InTagGuid))
		{
			check(ExistingTag->Get()->IsA(TagType::Type));
			return static_cast<TagType*>(ExistingTag->Get());
		};
		return nullptr;
	}

	virtual bool RemoveTag(const FGuid& InTagGuid);

	const TArray<TSharedPtr<FSchematicGraphTag>>& GetTags() const { return Tags; }

protected:

	void NotifyTagAdded(const TSharedPtr<FSchematicGraphTag>& Tag);

	FSchematicGraphModel* Model = nullptr;
	FGuid Guid = FGuid::NewGuid();
	FGuid ParentNodeGuid = FGuid();
	TArray<FGuid> ChildNodeGuids; 
	bool bIsSelected = false;
	FVector2d Position = FVector2d::ZeroVector;
	FVector2d PositionOffset = FVector2d::ZeroVector;
	float ScaleOffset = 1.f;
	TArray<FLinearColor> Colors = { FLinearColor::White };
	TArray<const FSlateBrush*> Brushes = { nullptr };
	FText ToolTip = FText();
	ESchematicGraphPlacementConstraint::Type Placement = ESchematicGraphPlacementConstraint::Free;
	ESchematicGraphVisibility::Type Visibility = ESchematicGraphVisibility::Visible;
	bool bDragSupported = false;

	TArray<TSharedPtr<FSchematicGraphTag>> Tags;
	TMap<FGuid, TSharedPtr<FSchematicGraphTag>> TagByGuid;

	friend class FSchematicGraphModel;
};

class FSchematicGraphGroupNode : public FSchematicGraphNode
{
public:

	SCHEMATICGRAPHNODE_BODY(FSchematicGraphGroupNode, FSchematicGraphNode)

	virtual ~FSchematicGraphGroupNode() override {}
	bool IsExpanded() const { return bExpanded; }
	void SetExpanded(bool InExpanded) { bExpanded = InExpanded; }
	virtual ESchematicGraphVisibility::Type GetVisibilityForChildNodes() const override;
	
protected:

	bool bExpanded = false;

	friend class FControlRigSchematicModel;
};

#endif