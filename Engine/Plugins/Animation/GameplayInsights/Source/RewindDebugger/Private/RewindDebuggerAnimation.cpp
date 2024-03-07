// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerAnimation.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Editor.h"
#include "Engine/PoseWatch.h"
#include "IAnimationProvider.h"
#include "IGameplayProvider.h"
#include "Insights/IUnrealInsightsModule.h"
#include "IRewindDebugger.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "SLevelViewport.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "ObjectTrace.h"
#include "TraceServices/Model/Frames.h"

#define LOCTEXT_NAMESPACE "RewindDebuggerAnimation"

FRewindDebuggerAnimation::FRewindDebuggerAnimation()
{
}

void FRewindDebuggerAnimation::RecordingStarted(IRewindDebugger*)
{
	UE::Trace::ToggleChannel(TEXT("Animation"), true);
}

void FRewindDebuggerAnimation::RecordingStopped(IRewindDebugger*)
{
	UE::Trace::ToggleChannel(TEXT("Animation"), false);
}

void FRewindDebuggerAnimation::Initialize()
{
	FEditorDelegates::ResumePIE.AddRaw(this, &FRewindDebuggerAnimation::OnPIEResumed);
	FEditorDelegates::EndPIE.AddRaw(this, &FRewindDebuggerAnimation::OnPIEStopped);
	FEditorDelegates::SingleStepPIE.AddRaw(this, &FRewindDebuggerAnimation::OnPIESingleStepped);
}

void FRewindDebuggerAnimation::Shutdown()
{
	FEditorDelegates::ResumePIE.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);
	FEditorDelegates::SingleStepPIE.RemoveAll(this);
}

void FRewindDebuggerAnimation::OnPIEResumed(bool bSimulating)
{
	// restore all relative transforms of any meshes that may have been moved while scrubbing
	for (TTuple<uint64, FMeshComponentResetData>& MeshData : MeshComponentsToReset)
	{
		if (USkeletalMeshComponent* MeshComponent = MeshData.Value.Component.Get())
		{
			MeshComponent->SetRelativeTransform(MeshData.Value.RelativeTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	MeshComponentsToReset.Empty();
}

void FRewindDebuggerAnimation::OnPIESingleStepped(bool bSimulating)
{
	// restore all relative transforms of any meshes that may have been moved while scrubbing
	for (TTuple<uint64, FMeshComponentResetData>& MeshData : MeshComponentsToReset)
	{
		if (USkeletalMeshComponent* MeshComponent = MeshData.Value.Component.Get())
		{
			MeshComponent->SetRelativeTransform(MeshData.Value.RelativeTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	MeshComponentsToReset.Empty();
}


void FRewindDebuggerAnimation::OnPIEStopped(bool bSimulating)
{
	MeshComponentsToReset.Empty();
}


void FRewindDebuggerAnimation::Update(float DeltaTime, IRewindDebugger* RewindDebugger)
{
	check(RewindDebugger);
	
	if (RewindDebugger->IsPIESimulating() || RewindDebugger->GetRecordingDuration() == 0.0)
	{
		return;
	}

	if (const TraceServices::IAnalysisSession* Session = RewindDebugger->GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);
		double CurrentTraceTime = RewindDebugger->CurrentTraceTime();
		
		if (CurrentTraceTime != LastScrubTime)
		{
			const TraceServices::IFrameProvider& FrameProvider = TraceServices::ReadFrameProvider(*Session);
			TraceServices::FFrame Frame;
			if (FrameProvider.GetFrameFromTime(ETraceFrameType::TraceFrameType_Game, CurrentTraceTime, Frame))
			{
				const IAnimationProvider* AnimationProvider = Session->ReadProvider<IAnimationProvider>("AnimationProvider");
				const IGameplayProvider* GameplayProvider = Session->ReadProvider<IGameplayProvider>("GameplayProvider");
				
				if (AnimationProvider && GameplayProvider)
				{
					// UWorld* World = GetWorldToVisualize();

					// update pose on all SkeletalMeshComponents:
					// - enumerate all skeletal mesh pose timelines
					// - check if the corresponding mesh component still exists
					// - apply the recorded pose for the current Frame
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(FRewindDebugger::Tick_UpdatePoses);
						AnimationProvider->EnumerateSkeletalMeshPoseTimelines([this, &Frame, AnimationProvider, GameplayProvider](uint64 ObjectId, const IAnimationProvider::SkeletalMeshPoseTimeline& TimelineData)
						{
#if OBJECT_TRACE_ENABLED
							if(UObject* ObjectInstance = FObjectTrace::GetObjectFromId(ObjectId))
							{
								if(USkeletalMeshComponent* MeshComponent = Cast<USkeletalMeshComponent>(ObjectInstance))
								{
									AnimationProvider->ReadSkeletalMeshPoseTimeline(ObjectId, [this, &Frame, ObjectId, MeshComponent, AnimationProvider](const IAnimationProvider::SkeletalMeshPoseTimeline& TimelineData, bool bHasCurves)
									{
										const FSkeletalMeshPoseMessage * PoseMessage = nullptr;

										// Get last pose in frame
										TimelineData.EnumerateEvents(Frame.StartTime, Frame.EndTime,
											[&PoseMessage](double InStartTime, double InEndTime, uint32 InDepth, const FSkeletalMeshPoseMessage& InPoseMessage)
											{
												PoseMessage = &InPoseMessage;
												return TraceServices::EEventEnumerate::Continue;
											});

										// Update mesh based on pose
										if (PoseMessage)
										{
											FTransform ComponentWorldTransform;
											if (const FSkeletalMeshInfo* SkeletalMeshInfo = AnimationProvider->FindSkeletalMeshInfo(PoseMessage->MeshId))
											{
												AnimationProvider->GetSkeletalMeshComponentSpacePose(*PoseMessage, *SkeletalMeshInfo, ComponentWorldTransform, MeshComponent->GetEditableComponentSpaceTransforms());
												MeshComponent->ApplyEditedComponentSpaceTransforms();

												if (MeshComponentsToReset.Find(ObjectId) == nullptr)
												{
													FMeshComponentResetData ResetData;
													ResetData.Component = MeshComponent;
													ResetData.RelativeTransform = MeshComponent->GetRelativeTransform();
													MeshComponentsToReset.Add(ObjectId, ResetData);
												}

												MeshComponent->SetWorldTransform(ComponentWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
												MeshComponent->SetForcedLOD(PoseMessage->LodIndex + 1);
												MeshComponent->UpdateChildTransforms(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);
											}
										}
									});
								}
							}
#endif // OBJECT_TRACE_ENABLED
							});
						}
        
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(FRewindDebugger::Tick_AnimBlueprintsDebug);
							// Apply Animation Blueprint Debugging Data:
							// - enumerate over all anim graph timelines
							// - check if their instance class still exists and is the debugging target for the Animation Blueprint Editor
							// - if it is copy that debug data into the class debug data for the blueprint debugger
							AnimationProvider->EnumerateAnimGraphTimelines([&Frame, AnimationProvider, GameplayProvider](uint64 ObjectId, const IAnimationProvider::AnimGraphTimeline& AnimGraphTimeline)
							{
#if OBJECT_TRACE_ENABLED
								if(UObject* ObjectInstance = FObjectTrace::GetObjectFromId(ObjectId))
								{
									if(UAnimInstance* AnimInstance = Cast<UAnimInstance>(ObjectInstance))
									{
										if(UAnimBlueprintGeneratedClass* InstanceClass = Cast<UAnimBlueprintGeneratedClass>(AnimInstance->GetClass()))
										{
											if(UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(InstanceClass->ClassGeneratedBy))
											{
												// for child Animation Blueprints, we actually want to debug the root blueprint (since the child doesn't contain any anim graphs)
												if (UAnimBlueprint* RootAnimBP = UAnimBlueprint::FindRootAnimBlueprint(AnimBlueprint))
												{
													if (UAnimBlueprintGeneratedClass* RootInstanceClass = Cast<UAnimBlueprintGeneratedClass>(RootAnimBP->GeneratedClass))
													{
														AnimBlueprint = RootAnimBP;
														InstanceClass = RootInstanceClass;
													}
												}

												if(AnimBlueprint->IsObjectBeingDebugged(AnimInstance))
												{
													TRACE_CPUPROFILER_EVENT_SCOPE(FRewindDebugger::Tick_UpdateBlueprintDebug);
													// update debug info for attached Animation Blueprint editors
													uint64 Id = FObjectTrace::GetObjectId(AnimInstance);
													const int32 NodeCount = InstanceClass->GetAnimNodeProperties().Num();
							
													FAnimBlueprintDebugData& DebugData = InstanceClass->GetAnimBlueprintDebugData();
													{
														TRACE_CPUPROFILER_EVENT_SCOPE(ResetNodeVisitStates);
														DebugData.ResetNodeVisitSites();
													}
													
													// Anim node values can come from all phases
													AnimationProvider->ReadAnimNodeValuesTimeline(ObjectId, [&Frame,AnimationProvider, &DebugData](const IAnimationProvider::AnimNodeValuesTimeline& InNodeValuesTimeline)
													{
														TRACE_CPUPROFILER_EVENT_SCOPE(AnimGraphNodeValues);
														InNodeValuesTimeline.EnumerateEvents(Frame.StartTime, Frame.EndTime, [AnimationProvider, &DebugData](double InStartTime, double InEndTime, uint32 InDepth, const FAnimNodeValueMessage& InMessage)
														{
															// don't send "Name" Node value for display in the graph
															if (FPlatformString::Strcmp(InMessage.Key, TEXT("Name")) != 0)
															{
																FText Text = AnimationProvider->FormatNodeKeyValue(InMessage);
																DebugData.RecordNodeValue(InMessage.NodeId, Text.ToString());
															}
															return TraceServices::EEventEnumerate::Continue;
														});
													});

													DebugData.DisableAllPoseWatches();
						
													AnimGraphTimeline.EnumerateEvents(Frame.StartTime, Frame.EndTime, [Id, AnimationProvider, GameplayProvider, &DebugData, NodeCount](double InGraphStartTime, double InGraphEndTime, uint32 InDepth, const FAnimGraphMessage& InMessage)
													{
														TRACE_CPUPROFILER_EVENT_SCOPE(AnimGraphTimelineEvent);
																
														// Basic verification - check node count is the same
														// @TODO: could add some form of node hash/CRC to the class to improve this
														if(InMessage.NodeCount == NodeCount)
														{
															// Check for an update phase (which contains weights)
															if(InMessage.Phase == EAnimGraphPhase::Update)
															{
																AnimationProvider->ReadAnimNodesTimeline(Id, [InGraphStartTime, InGraphEndTime, &DebugData](const IAnimationProvider::AnimNodesTimeline& InNodesTimeline)
																{
																	TRACE_CPUPROFILER_EVENT_SCOPE(AnimGraphDebugNodeVisits);
																	InNodesTimeline.EnumerateEvents(InGraphStartTime, InGraphEndTime, [&DebugData](double InStartTime, double InEndTime, uint32 InDepth, const FAnimNodeMessage& InMessage)
																	{
																		DebugData.RecordNodeVisit(InMessage.NodeId, InMessage.PreviousNodeId, InMessage.Weight);
																		return TraceServices::EEventEnumerate::Continue;
																	});
																});
						
																AnimationProvider->ReadStateMachinesTimeline(Id, [InGraphStartTime, InGraphEndTime, &DebugData](const IAnimationProvider::StateMachinesTimeline& InStateMachinesTimeline)
																{
																	TRACE_CPUPROFILER_EVENT_SCOPE(AnimGraphDebugStateMachine);
																	InStateMachinesTimeline.EnumerateEvents(InGraphStartTime, InGraphEndTime, [&DebugData](double InStartTime, double InEndTime, uint32 InDepth, const FAnimStateMachineMessage& InMessage)
																	{
																		DebugData.RecordStateData(InMessage.StateMachineIndex, InMessage.StateIndex, InMessage.StateWeight, InMessage.ElapsedTime);
																		return TraceServices::EEventEnumerate::Continue;
																	});
																});
						
																AnimationProvider->ReadAnimSequencePlayersTimeline(Id, [InGraphStartTime, InGraphEndTime, GameplayProvider, &DebugData](const IAnimationProvider::AnimSequencePlayersTimeline& InSequencePlayersTimeline)
																{
																	TRACE_CPUPROFILER_EVENT_SCOPE(AnimGraphDebugSequencePlayers);
																	InSequencePlayersTimeline.EnumerateEvents(InGraphStartTime, InGraphEndTime, [&DebugData](double InStartTime, double InEndTime, uint32 InDepth, const FAnimSequencePlayerMessage& InMessage)
																	{
																		DebugData.RecordSequencePlayer(InMessage.NodeId, InMessage.Position, InMessage.Length, InMessage.FrameCounter);
																		return TraceServices::EEventEnumerate::Continue;
																	});
																});
						
																AnimationProvider->ReadAnimBlendSpacePlayersTimeline(Id, [InGraphStartTime, InGraphEndTime, GameplayProvider, &DebugData](const IAnimationProvider::BlendSpacePlayersTimeline& InBlendSpacePlayersTimeline)
																{
																	TRACE_CPUPROFILER_EVENT_SCOPE(AnimGraphBlendSpaces);
																	InBlendSpacePlayersTimeline.EnumerateEvents(InGraphStartTime, InGraphEndTime, [GameplayProvider, &DebugData](double InStartTime, double InEndTime, uint32 InDepth, const FBlendSpacePlayerMessage& InMessage)
																	{
																		UBlendSpace* BlendSpace = nullptr;
																		const FObjectInfo* BlendSpaceInfo = GameplayProvider->FindObjectInfo(InMessage.BlendSpaceId);
																		if(BlendSpaceInfo)
																		{
																			BlendSpace = TSoftObjectPtr<UBlendSpace>(FSoftObjectPath(BlendSpaceInfo->PathName)).LoadSynchronous();
																		}
						
																		DebugData.RecordBlendSpacePlayer(InMessage.NodeId, BlendSpace, FVector(InMessage.PositionX, InMessage.PositionY, InMessage.PositionZ), FVector(InMessage.FilteredPositionX, InMessage.FilteredPositionY, InMessage.FilteredPositionZ));
																		return TraceServices::EEventEnumerate::Continue;
																	});
																});
						
																AnimationProvider->ReadAnimSyncTimeline(Id, [InGraphStartTime, InGraphEndTime, AnimationProvider, &DebugData](const IAnimationProvider::AnimSyncTimeline& InAnimSyncTimeline)
																{
																	TRACE_CPUPROFILER_EVENT_SCOPE(AnimGraphAnimSync);
																	InAnimSyncTimeline.EnumerateEvents(InGraphStartTime, InGraphEndTime, [AnimationProvider, &DebugData](double InStartTime, double InEndTime, uint32 InDepth, const FAnimSyncMessage& InMessage)
																	{
																		const TCHAR* GroupName = AnimationProvider->GetName(InMessage.GroupNameId);
																		if(GroupName)
																		{
																			DebugData.RecordNodeSync(InMessage.SourceNodeId, FName(GroupName));
																		}
															
																		return TraceServices::EEventEnumerate::Continue;
																	});
																});
															}
						
															// Some traces come from both update and evaluate phases
															if(InMessage.Phase == EAnimGraphPhase::Update || InMessage.Phase == EAnimGraphPhase::Evaluate)
															{
																AnimationProvider->ReadAnimAttributesTimeline(Id, [InGraphStartTime, InGraphEndTime, AnimationProvider, &DebugData](const IAnimationProvider::AnimAttributeTimeline& InAnimAttributeTimeline)
																{
																	TRACE_CPUPROFILER_EVENT_SCOPE(AnimGraphAttributes);
																	InAnimAttributeTimeline.EnumerateEvents(InGraphStartTime, InGraphEndTime, [AnimationProvider, &DebugData](double InStartTime, double InEndTime, uint32 InDepth, const FAnimAttributeMessage& InMessage)
																	{
																		const TCHAR* AttributeName = AnimationProvider->GetName(InMessage.AttributeNameId);
																		if(AttributeName)
																		{
																			DebugData.RecordNodeAttribute(InMessage.TargetNodeId, InMessage.SourceNodeId, FName(AttributeName));
																		}
															
																		return TraceServices::EEventEnumerate::Continue;
																	});
																});

																
																AnimationProvider->ReadPoseWatchTimeline(Id, [InGraphStartTime, InGraphEndTime, AnimationProvider, &DebugData](const IAnimationProvider::PoseWatchTimeline& InPoseWatchTimeline)
																	{
																		InPoseWatchTimeline.EnumerateEvents(InGraphStartTime, InGraphEndTime, [AnimationProvider, &DebugData](double InStartTime, double InEndTime, uint32 InDepth, const FPoseWatchMessage& InMessage)
																			{
																				for (FAnimNodePoseWatch& PoseWatch : DebugData.AnimNodePoseWatch)
																				{
																					if (PoseWatch.NodeID == InMessage.PoseWatchId)
																					{
																						TArray<FBoneIndexType> RequiredBones;
																						TArray<FTransform> BoneTransforms;
																						AnimationProvider->GetPoseWatchData(InMessage, BoneTransforms, RequiredBones);

																						PoseWatch.SetPose(RequiredBones, BoneTransforms);
																						PoseWatch.SetWorldTransform(InMessage.WorldTransform);

																						PoseWatch.PoseWatch->SetIsNodeEnabled(true);
																						break;
																					}
																				}
																				return TraceServices::EEventEnumerate::Continue;
																			});
																	});

															}
						
														}
														return TraceServices::EEventEnumerate::Continue;
													});
												}
											}
										}
									}
								}
#endif // OBJECT_TRACE_ENABLED
								return TraceServices::EEventEnumerate::Continue;
							});
						}
					}
				}
			}
	}
}

#undef LOCTEXT_NAMESPACE