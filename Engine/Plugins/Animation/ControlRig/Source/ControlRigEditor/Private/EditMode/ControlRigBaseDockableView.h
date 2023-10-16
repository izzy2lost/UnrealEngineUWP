// Copyright Epic Games, Inc. All Rights Reserved.
/**
* Base View for Dockable Control Rig Animation widgets Details/Outliner
*/
#pragma once

#include "CoreMinimal.h"
#include "EditorModeManager.h"


class UBaseControlRig;
class ISequencer;
class FControlRigEditMode;
struct FRigControlElement;
struct FRigElementKey;

class FControlRigBaseDockableView 
{
public:
	FControlRigBaseDockableView();
	virtual ~FControlRigBaseDockableView();
	TArray<UBaseControlRig*> GetControlRigs() const;

	virtual void SetEditMode(FControlRigEditMode& InEditMode);

protected:
	virtual void HandleControlSelected(UBaseControlRig* Subject, FRigControlElement* InControl, bool bSelected);
	virtual void HandleControlAdded(UBaseControlRig* ControlRig, bool bIsAdded);

	void HandlElementSelected(UBaseControlRig* Subject, const FRigElementKey& Key, bool bSelected);

	ISequencer* GetSequencer() const;

	FEditorModeTools* ModeTools = nullptr;

};

