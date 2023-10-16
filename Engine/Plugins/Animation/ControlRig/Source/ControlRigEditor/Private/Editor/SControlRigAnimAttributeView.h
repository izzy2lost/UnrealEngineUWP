// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FControlRigEditor;
class UControlRigBlueprint;
class UBaseControlRig;

class SAnimAttributeView;


class SControlRigAnimAttributeView : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SControlRigAnimAttributeView )
	{}
	
	SLATE_END_ARGS()

	~SControlRigAnimAttributeView();
	
	void Construct( const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor);

	void HandleSetObjectBeingDebugged(UObject* InObject);
	
	void StartObservingNewControlRig(UBaseControlRig* InControlRig);
	void StopObservingCurrentControlRig();

	void HandleControlRigPostForwardSolve(UBaseControlRig* InControlRig, const FName& InEventName) const;
	
	TWeakPtr<FControlRigEditor> ControlRigEditor;
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprint;
	TWeakObjectPtr<UBaseControlRig> ControlRigBeingDebuggedPtr;
	TSharedPtr<SAnimAttributeView> AttributeView;
};
