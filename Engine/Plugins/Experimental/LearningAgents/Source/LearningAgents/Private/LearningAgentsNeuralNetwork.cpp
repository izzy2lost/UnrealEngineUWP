// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsNeuralNetwork.h"

#include "LearningAgentsNeuralNetworkData.h"

#include "LearningLog.h"
#include "LearningArray.h"
#include "LearningNeuralNetwork.h"

#include "UObject/Package.h"
#include "Misc/FileHelper.h"

ULearningAgentsNeuralNetwork::ULearningAgentsNeuralNetwork() = default;
ULearningAgentsNeuralNetwork::ULearningAgentsNeuralNetwork(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsNeuralNetwork::~ULearningAgentsNeuralNetwork() = default;

void ULearningAgentsNeuralNetwork::ResetNetwork()
{
	NeuralNetworkData->ConditionalBeginDestroy();
	NeuralNetworkData = nullptr;
	
	ForceMarkDirty();
}

void ULearningAgentsNeuralNetwork::LoadNetworkFromSnapshot(const FFilePath& File)
{
	TArray<uint8> NetworkData;

	if (FFileHelper::LoadFileToArray(NetworkData, *File.FilePath))
	{
		ULearningAgentsNeuralNetworkData* TempNeuralNetworkData = NewObject<ULearningAgentsNeuralNetworkData>(this);
		int32 Offset = 0;
		bool bSuccess = TempNeuralNetworkData->GetNetworkInterface()->DeserializeFromBytes(Offset, NetworkData);
		
		if (!bSuccess)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network. Invalid Format: \"%s\""), *GetName(), *File.FilePath);
			return;
		}
		
		NetworkData.Empty();

		if (NeuralNetworkData)
		{
			// If we already have a neural network check settings match

			if (TempNeuralNetworkData->GetNetworkInterface()->GetInputNum() != NeuralNetworkData->GetNetworkInterface()->GetInputNum() ||
				TempNeuralNetworkData->GetNetworkInterface()->GetOutputNum() != NeuralNetworkData->GetNetworkInterface()->GetOutputNum())
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network from snapshot as settings don't match."), *GetName());
				return;
			}
			else
			{
				NeuralNetworkData->CopyFrom(TempNeuralNetworkData);
			}
		}
		else
		{
			// Otherwise use loaded neural network as-is

			NeuralNetworkData = TempNeuralNetworkData;
		}

		ForceMarkDirty();
	}
	else
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network. File not found: \"%s\""), *GetName(), *File.FilePath);
	}
}

void ULearningAgentsNeuralNetwork::SaveNetworkToSnapshot(const FFilePath& File)
{
	if (!NeuralNetworkData)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: No network data to save"), *GetName());
		return;
	}

	TArray<uint8> NetworkData;
	NetworkData.SetNumUninitialized(NeuralNetworkData->GetNetworkInterface()->GetSerializationByteNum());

	int32 Offset = 0;
	NeuralNetworkData->GetNetworkInterface()->SerializeToBytes(Offset, NetworkData);
	UE_LEARNING_CHECK(Offset == NetworkData.Num());

	if (!FFileHelper::SaveArrayToFile(NetworkData, *File.FilePath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Failed to save network to file: \"%s\""), *GetName(), *File.FilePath);
	}
}

void ULearningAgentsNeuralNetwork::LoadNetworkFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!NeuralNetworkAsset || !NeuralNetworkAsset->NeuralNetworkData)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is invalid."), *GetName());
		return;
	}

	if (NeuralNetworkAsset == this)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is same as the current network."), *GetName());
		return;
	}

	if (NeuralNetworkData)
	{
		if (NeuralNetworkAsset->NeuralNetworkData->GetNetworkInterface()->GetInputNum() != NeuralNetworkData->GetNetworkInterface()->GetInputNum() ||
			NeuralNetworkAsset->NeuralNetworkData->GetNetworkInterface()->GetOutputNum() != NeuralNetworkData->GetNetworkInterface()->GetOutputNum())
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network from asset as settings don't match."), *GetName());
			return;
		}
	}
	else
	{
		NeuralNetworkData = NewObject<ULearningAgentsNeuralNetworkData>(this);
	}

	NeuralNetworkData->CopyFrom(NeuralNetworkAsset->NeuralNetworkData);
	ForceMarkDirty();
}

void ULearningAgentsNeuralNetwork::SaveNetworkToAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!NeuralNetworkAsset)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is invalid."), *GetName());
		return;
	}

	if (NeuralNetworkAsset == this)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is same as the current network."), *GetName());
		return;
	}

	if (!NeuralNetworkAsset->NeuralNetworkData)
	{
		NeuralNetworkAsset->NeuralNetworkData = NewObject<ULearningAgentsNeuralNetworkData>(NeuralNetworkAsset);
	}

	NeuralNetworkAsset->NeuralNetworkData->CopyFrom(NeuralNetworkData);
	NeuralNetworkAsset->ForceMarkDirty();
}

void ULearningAgentsNeuralNetwork::ForceMarkDirty()
{
	// Manually mark the package as dirty since just using `Modify` prevents 
	// marking packages as dirty during PIE which is most likely when this
	// is being used.
	if (UPackage* Package = GetPackage())
	{
		const bool bIsDirty = Package->IsDirty();

		if (!bIsDirty)
		{
			Package->SetDirtyFlag(true);
		}

		Package->PackageMarkedDirtyEvent.Broadcast(Package, bIsDirty);
	}
}
