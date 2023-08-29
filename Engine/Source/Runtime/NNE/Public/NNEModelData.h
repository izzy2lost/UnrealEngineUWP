// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Memory/SharedBuffer.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"

#include "NNEModelData.generated.h"

namespace UE::NNE
{
	/**
	 * This class present a ref counted view on an immutable memory buffer. 
	 * Allowing runtime to reference result of GetModelData() even if they outlive UNNEModelData.
	 */
	class FSharedModelData
	{
	private:
		FSharedBuffer Data;

	public:
		FSharedModelData(FSharedBuffer InData) : Data(InData) {}
		FSharedModelData() {}

		/**
		 * Get a const array view on the shared data which is guaranteed to remain valid as long as this objects exists.
		 *
		 * @return A const array view of the shared data.
		 */
		TConstArrayView<uint8> GetView() const
		{
			return MakeArrayView(static_cast<const uint8*>(Data.GetData()), Data.GetSize());
		}
	};
}

/**
 * This class represents assets that store neural network model data.
 *
 * Neural network models typically consist of a graph of operations and corresponding parameters as e.g. weights.
 * UNNEModelData assets store such model data as imported e.g. by the UNNEModelDataFactory class.
 * An INNERuntime object retrieved by UE::NNE::GetRuntime<T>(const FString& Name) can be used to create an inferable neural network model.
 */
UCLASS(BlueprintType, Category = "NNE")
class NNE_API UNNEModelData : public UObject
{
	GENERATED_BODY()

public:

	// UObject interface
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	/**
	 * Initialize the model data with a copy of the data inside Buffer.
	 *
	 * This function is called by the UNNEModelDataFactory class when importing a neural network model file.
	 *
	 * @param Type A string identifying the type of data inside this asset. Corresponds to the extension of the imported file.
	 * @param Buffer The raw binary file data of the imported model to be copied into this asset.
	 */
	void Init(const FString& Type, TConstArrayView<uint8> Buffer);

	/**
	 * Get the target runtimes this model data will be cooked for. An empty list means all runtimes.
	 *
	 * @return The target runtimes names.
	 */
	TArrayView<const FString> GetTargetRuntimes() const;

	/**
	 * Set the target runtimes this model data will be cooked for. An empty list means all runtimes.
	 *
	 * @param RuntimeNames The target runtimes names.
	 */
	void SetTargetRuntimes(TArrayView<const FString> RuntimeNames);

	/**
	 * Get the type of data inside FileData.
	 *
	 * The FileType identifies the type of data inside FileData and typically is the extension of the file used to create the asset.
	 *
	 * @return The FileType.
	 */
	FString GetFileType();

	/**
	 * Get read only access to FileData.
	 *
	 * The FileData contains the binary data of the file which has been used to create the asset.
	 *
	 * @return The FileData.
	 */
	TConstArrayView<uint8> GetFileData();

	/**
	 * Clears the FileData and the FileType.
	 *
	 * Caution, if the FileData is cleared, no more models can be created on runtimes that do not already have ModelData inside this asset.
	 */
	void ClearFileDataAndFileType();

	/**
	 * Get the FGuid identifying the FileData.
	 *
	 * The FileId is created on import of an asset. It can be used to identify the FileData, e.g. when putting corresponding data into the DDC or caching data locally.
	 *
	 * @return The FileId.
	 */
	FGuid GetFileId();

	/**
	 * Get the cached (editor) or cooked (game) optimized model data for a given runtime.
	 *
	 * This function is used by runtimes when creating a model. In editor, the function will create the optimized model data with the passed runtime in case it has not been cached in the DCC yet. In game, the cooked data is accessed. The returned model data is aligned in memory as requested by the runtime.
	 *
	 * @param RuntimeName The name of the runtime for which the data should be returned.
	 * @return The optimized and runtime specific model data or an invalid TSharedPtr in case of failure.
	 */
	TSharedPtr<UE::NNE::FSharedModelData> GetModelData(const FString& RuntimeName);

	/**
	 * Clears the ModelData.
	 *
	 * Caution, if the ModelData is cleared, only runtimes that support cooking on the current platform can create new models from this asset.
	 */
	void ClearModelData();

private:
	/**
	 * A list of string of the supported runtime, empty to support them all.
	 */
	TArray<FString> TargetRuntimes;

	/**
	 * A string identifying the type of data inside this asset. Corresponds to the extension of the imported file.
	 */
	FString FileType;

	/**
	 * The raw binary file data of the imported model.
	 */
	TArray<uint8> FileData;

	/**
	 * A Guid that uniquely identifies this model. This is used to cache optimized models in the editor.
	 */
	FGuid FileId;

	/**
	 * The processed / optimized model data for the different runtimes.
	 */
	TMap<FString, TTuple<FSharedBuffer, uint32>> ModelData;
};
