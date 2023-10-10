// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class UAnimNextGraph_EditorData;
class URigVMGraph;

namespace UE::AnimNext::Editor
{

class SAnimNextGraphView : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnOpenGraph, URigVMGraph* /*InGraph*/);

	SLATE_BEGIN_ARGS(SAnimNextGraphView) {}

	SLATE_EVENT(SAnimNextGraphView::FOnOpenGraph, OnOpenGraph)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UAnimNextGraph_EditorData* InEditorData);

private:
	UAnimNextGraph_EditorData* EditorData = nullptr;
};

}