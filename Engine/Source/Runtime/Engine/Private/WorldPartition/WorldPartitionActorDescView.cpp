// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionActorDescView.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDesc.h"

FWorldPartitionActorDescView::FWorldPartitionActorDescView() : FWorldPartitionActorDescView(nullptr) {}
FWorldPartitionActorDescView::FWorldPartitionActorDescView(const FWorldPartitionActorDesc* InActorDesc)	: ActorDesc(InActorDesc) {}
const FGuid& FWorldPartitionActorDescView::GetGuid() const { return ActorDesc->GetGuid(); }
FTopLevelAssetPath FWorldPartitionActorDescView::GetBaseClass() const {	return ActorDesc->GetBaseClass(); }
FTopLevelAssetPath FWorldPartitionActorDescView::GetNativeClass() const { return ActorDesc->GetNativeClass(); }
UClass* FWorldPartitionActorDescView::GetActorNativeClass() const {	return ActorDesc->GetActorNativeClass(); }
FName FWorldPartitionActorDescView::GetRuntimeGrid() const { return ActorDesc->GetRuntimeGrid(); }
bool FWorldPartitionActorDescView::GetIsSpatiallyLoaded() const { return ActorDesc->GetIsSpatiallyLoaded(); }
bool FWorldPartitionActorDescView::GetActorIsEditorOnly() const { return ActorDesc->GetActorIsEditorOnly(); }
bool FWorldPartitionActorDescView::GetActorIsRuntimeOnly() const { return ActorDesc->GetActorIsRuntimeOnly(); }
bool FWorldPartitionActorDescView::GetActorIsHLODRelevant() const {	return ActorDesc->GetActorIsHLODRelevant(); }
FSoftObjectPath FWorldPartitionActorDescView::GetHLODLayer() const { return ActorDesc->GetHLODLayer(); }
bool FWorldPartitionActorDescView::HasResolvedDataLayerInstanceNames() const { return ActorDesc->HasResolvedDataLayerInstanceNames(); }
const TArray<FName>& FWorldPartitionActorDescView::GetDataLayerInstanceNames() const { return ActorDesc->GetDataLayerInstanceNames(); }
const TArray<FName>& FWorldPartitionActorDescView::GetTags() const { return ActorDesc->GetTags(); }
FName FWorldPartitionActorDescView::GetActorPackage() const { return ActorDesc->GetActorPackage(); }
FSoftObjectPath FWorldPartitionActorDescView::GetActorSoftPath() const { return ActorDesc->GetActorSoftPath(); }
FName FWorldPartitionActorDescView::GetActorLabel() const {	return ActorDesc->GetActorLabel(); }
FBox FWorldPartitionActorDescView::GetEditorBounds() const { return ActorDesc->GetEditorBounds(); }
FBox FWorldPartitionActorDescView::GetRuntimeBounds() const { return ActorDesc->GetRuntimeBounds(); }
const TArray<FGuid>& FWorldPartitionActorDescView::GetReferences() const { return ActorDesc->GetReferences(); }
FString FWorldPartitionActorDescView::ToString() const { return ActorDesc->ToString(); }
const FGuid& FWorldPartitionActorDescView::GetParentActor() const {	return ActorDesc->GetParentActor(); }
FName FWorldPartitionActorDescView::GetActorName() const { return ActorDesc->GetActorName(); }
const FGuid& FWorldPartitionActorDescView::GetFolderGuid() const { return ActorDesc->GetFolderGuid(); }
FGuid FWorldPartitionActorDescView::GetContentBundleGuid() const { return ActorDesc->GetContentBundleGuid(); }
FName FWorldPartitionActorDescView::GetContainerPackage() const { return ActorDesc->GetContainerPackage(); }
bool FWorldPartitionActorDescView::IsContainerInstance() const { return ActorDesc->IsContainerInstance(); }
bool FWorldPartitionActorDescView::GetContainerInstance(FWorldPartitionActorDesc::FContainerInstance& OutContainerInstance) const {	return ActorDesc->GetContainerInstance(OutContainerInstance); }
EWorldPartitionActorFilterType FWorldPartitionActorDescView::GetContainerFilterType() const { return ActorDesc->GetContainerFilterType(); }
const FWorldPartitionActorFilter* FWorldPartitionActorDescView::GetContainerFilter() const { return ActorDesc->GetContainerFilter(); }
void FWorldPartitionActorDescView::CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const { return ActorDesc->CheckForErrors(ErrorHandler); }
FName FWorldPartitionActorDescView::GetActorLabelOrName() const { return ActorDesc->GetActorLabelOrName(); }
AActor* FWorldPartitionActorDescView::GetActor() const { return ActorDesc->GetActor(); }
bool FWorldPartitionActorDescView::IsEditorOnlyReference(const FGuid& ReferenceGuid) const { return ActorDesc->IsEditorOnlyReference(ReferenceGuid); }
bool FWorldPartitionActorDescView::GetProperty(FName PropertyName, FName* PropertyValue) const { return ActorDesc->GetProperty(PropertyName, PropertyValue); }
bool FWorldPartitionActorDescView::HasProperty(FName PropertyName) const { return ActorDesc->HasProperty(PropertyName); }

const TArray<FName>& FWorldPartitionActorDescView::GetRuntimeDataLayerInstanceNames() const { static TArray<FName> EmptyDataLayers; return EmptyDataLayers; }
const TArray<FGuid>& FWorldPartitionActorDescView::GetEditorReferences() const { static TArray<FGuid> EmptyEditorReferences; return EmptyEditorReferences; }

#endif
