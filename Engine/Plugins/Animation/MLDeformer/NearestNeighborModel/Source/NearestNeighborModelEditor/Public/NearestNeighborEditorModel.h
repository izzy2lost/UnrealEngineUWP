// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MLDeformerMorphModelEditorModel.h"
#include "MLDeformerEditorActor.h"

class UMLDeformerAsset;
class USkeletalMesh;
class UGeometryCache;
class UMLDeformerComponent;
class UMLDeformerOptimizedNetwork;
class UNearestNeighborModel;
class UNearestNeighborModelInstance;
class UNearestNeighborTrainingModel;
class UNearestNeighborModelVizSettings;

namespace UE::NearestNeighborModel
{
	using namespace UE::MLDeformer;

	class FNearestNeighborEditorModelActor;
	class FNearestNeighborModelDetails;
	class FNearestNeighborModelSampler;
	class FNearestNeighborModelVizSettingsDetails;

	class NEARESTNEIGHBORMODELEDITOR_API FNearestNeighborEditorModel
		: public UE::MLDeformer::FMLDeformerMorphModelEditorModel
	{
	public:
		// We need to implement this static MakeInstance method.
		static FMLDeformerEditorModel* MakeInstance();

		// FGCObject overrides.
		virtual FString GetReferencerName() const override { return TEXT("FNearestNeighborEditorModel"); }
		// ~END FGCObject overrides.
	
		// FMLDeformerGeomCacheEditorModel overrides.
		virtual void Init(const InitSettings& Settings) override;
		virtual TSharedPtr<FMLDeformerSampler> CreateSamplerObject() const override;
		virtual void CreateActors(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene) override;
		virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
		virtual FMLDeformerEditorActor* CreateEditorActor(const FMLDeformerEditorActor::FConstructSettings& Settings) const override;

		virtual void InitInputInfo(UMLDeformerInputInfo* InputInfo) override;
		virtual ETrainingResult Train() override;
		virtual bool IsTrained() const override;
		virtual bool LoadTrainedNetwork() const override;
		virtual void OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent) override;
		virtual void OnPostTraining(ETrainingResult TrainingResult, bool bUsePartiallyTrainedWhenAborted) override;
		
		UE_DEPRECATED(5.4, "Onnx file no longer used for training network.")
		virtual FString GetTrainedNetworkOnnxFile() const override;
		virtual int32 GetNumTrainingFrames() const override;
		// ~END FMLDeformerGeomCacheEditorModel overrides.

		// UMLDeformerMorphModelEditorModel overrides.
		virtual bool IsMorphWeightClampingSupported() const override	{ return false; }	// We already do input clamping, so output clamping really isn't needed.
		// ~END UMLDeformerMorphModelEditorModel overrides.


		friend class FNearestNeighborModelSampler;
		friend class FNearestNeighborModelDetails;
		friend class ::UNearestNeighborTrainingModel;
		friend class FNearestNeighborModelVizSettingsDetails;

	private:
		// Some helpers that cast to this model's variants of some classes.
		UNearestNeighborModel* GetNearestNeighborModel() const { return static_cast<UNearestNeighborModel*>(Model); }
		UNearestNeighborModelVizSettings* GetNearestNeighborModelVizSettings() const;

		// Recomputes nearest neighbor coeffcients and nearest neighbor vertex offsets. 
		virtual uint8 UpdateNearestNeighborData();

		// Temporarily set sampler to use part data in nearest neighbor dataset. This is useful when updating nearest neighbor data.
		uint8 SetSamplerPartData(const int32 PartI);

		// Reset smapler to use the original dataset.
		void ResetSamplerData();

		void UpdateNearestNeighborActors();
		void KMeansClusterPoses();
		TPair<UAnimSequence*, uint8> CreateAnimOfClusterCenters(const FString& PackageName, const TArray<int32>& KmeansResults);
		uint8 GetKMeansClusterResult() const { return KMeansClusterResult; }

		uint8 InitMorphTargets();
		void RefreshMorphTargets();
		void AddFloatArrayToDeltaArray(const TArray<float>& FloatArr, const TArray<uint32>& VertexMap, TArray<FVector3f>& DeltaArr, int32 DeltaArrayOffset = INDEX_NONE, TOptional<TArray<int32>> OptionalIncludedFrames = TOptional<TArray<int32>>());

		int32 GetNumParts();

		void OnMorphTargetUpdate();
		uint8 GetMorphTargetUpdateResult() { return MorphTargetUpdateResult; }
		UMLDeformerComponent* GetTestMLDeformerComponent() const;
		UMLDeformerModelInstance* GetTestMLDeformerModelInstance() const;
		void InitTestMLDeformerPreviousWeights();
		uint8 WarnIfNetworkInvalid();

		virtual void CreateNearestNeighborActors(UWorld* World, int32 StartIndex = 0);

		template<class TrainingModelClass>
		TrainingModelClass* InitTrainingModel(FMLDeformerEditorModel* EditorModel)
		{
			TArray<UClass*> TrainingModels;
			GetDerivedClasses(TrainingModelClass::StaticClass(), TrainingModels);
			if (TrainingModels.IsEmpty())
			{
				return nullptr;
			}

			TrainingModelClass* TrainingModel = Cast<TrainingModelClass>(TrainingModels.Last()->GetDefaultObject());
			TrainingModel->Init(EditorModel);
			return TrainingModel;
		}

		void GetNeighborStats();

		// Actors showing the nearest neighbors of the current frame
		TArray<FNearestNeighborEditorModelActor*> NearestNeighborActors;

		UWorld* EditorWorld = nullptr;
		uint8 MorphTargetUpdateResult = 0;
		uint8 KMeansClusterResult = 0;
		int32 NumTrainingFramesOverride = -1;
	};
}	// namespace UE::NearestNeighborModel
