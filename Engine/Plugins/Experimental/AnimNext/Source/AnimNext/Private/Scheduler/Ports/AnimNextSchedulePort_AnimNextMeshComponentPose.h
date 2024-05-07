// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Scheduler/AnimNextSchedulePort.h"
#include "Param/AnimNextEditorParam.h"
#include "AnimNextSchedulePort_AnimNextMeshComponentPose.generated.h"

UCLASS(DisplayName = "Skeletal Mesh Component Pose")
class UAnimNextSchedulePort_AnimNextMeshComponentPose : public UAnimNextSchedulePort
{
	GENERATED_BODY()

	// UAnimNextSchedulePort interface
	virtual void Run(const UE::AnimNext::FScheduleTermContext& InContext) const override;
	virtual TConstArrayView<FAnimNextEditorParam> GetRequiredParameters() const override;
	
	// IAnimNextScheduleTermInterface interface
	virtual TConstArrayView<UE::AnimNext::FScheduleTerm> GetTerms() const override;

private:
	mutable TArray<FAnimNextEditorParam> RequiredParams;
};