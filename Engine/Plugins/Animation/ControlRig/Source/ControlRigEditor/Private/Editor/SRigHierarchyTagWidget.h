// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Rigs/RigHierarchyDefines.h"
#include "DragAndDrop/DecoratedDragDropOp.h"

class SRigHierarchyTagWidget;

DECLARE_DELEGATE_OneParam(FOnRigTreeElementKeyTagDragDetected, const FRigElementKey&);

class SRigHierarchyTagWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRigHierarchyTagWidget)
		: _Text()
		, _TooltipText()
		, _Icon(nullptr)
		, _Color(FLinearColor(.2, .2, .2))
		, _TextColor(FLinearColor(.8, .8, .8))
		, _Radius(5)
		, _Padding(10, 0, 0, 0)
		, _ContentPadding(2, 2, 2, 2)
		, _Identifier()
		, _AllowDragDrop(false)
		, _OnClicked()
	{}
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ATTRIBUTE(FText, TooltipText)
		SLATE_ATTRIBUTE(const FSlateBrush*, Icon)
		SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_ATTRIBUTE(FSlateColor, TextColor)
		SLATE_ARGUMENT(float, Radius)
		SLATE_ARGUMENT(FMargin, Padding)
		SLATE_ARGUMENT(FMargin, ContentPadding)
		SLATE_ATTRIBUTE(FString, Identifier)
		SLATE_ARGUMENT(bool, AllowDragDrop)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	FOnRigTreeElementKeyTagDragDetected& OnElementKeyDragDetected() { return OnElementKeyDragDetectedDelegate; }
private:

	FMargin GetTextPadding() const;

	TAttribute<FText> Text;
	TAttribute<const FSlateBrush*> Icon;
	TAttribute<FLinearColor> Color;
	float Radius = 5.f;
	FMargin Padding;
	FMargin ContentPadding;
	TAttribute<FString> Identifier;
	bool bAllowDragDrop = false;
	FSimpleDelegate OnClicked;
	FOnRigTreeElementKeyTagDragDetected OnElementKeyDragDetectedDelegate;

	friend class FRigHierarchyTagDragDropOp;
};
