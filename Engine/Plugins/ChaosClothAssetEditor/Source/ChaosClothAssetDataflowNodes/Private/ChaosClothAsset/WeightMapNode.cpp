// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/WeightMapNode.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/WeightedValue.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowObject.h"
#include "InteractiveToolChange.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WeightMapNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetWeightMapNode"

namespace UE::Chaos::ClothAsset::Private
{
	// These are defined in AddWeightMapNode.cpp

	void TransferWeightMap(
		const TConstArrayView<FVector2f>& InSourcePositions,
		const TConstArrayView<FIntVector3>& SourceIndices,
		const TConstArrayView<int32> SourceWeightsLookup,
		const TConstArrayView<float>& InSourceWeights,
		const TConstArrayView<FVector2f>& InTargetPositions,
		const TConstArrayView<FIntVector3>& TargetIndices,
		const TConstArrayView<int32> TargetWeightsLookup,
		TArray<float>& OutTargetWeights);

	void SetVertexWeights(const TConstArrayView<float> InputMap, const TArray<float>& FinalValues, EChaosClothAssetWeightMapOverrideType OverrideType, TArray<float>& SourceVertexWeights);

	void CalculateFinalVertexWeightValues(const TConstArrayView<float> InputMap, TArrayView<float> FinalOutputMap, EChaosClothAssetWeightMapOverrideType OverrideType, const TArray<float>& SourceVertexWeights);
}


FChaosClothAssetWeightMapNode::FChaosClothAssetWeightMapNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowTerminalNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	if (FDataflowInput* const Input = RegisterInputConnection(&InputName.StringValue, GET_MEMBER_NAME_CHECKED(FChaosClothAssetConnectableIStringValue, StringValue)))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
	if (FDataflowInput* const Input = RegisterInputConnection(&TransferCollection))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&Name);
}

void FChaosClothAssetWeightMapNode::SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const
{
	using namespace UE::Chaos::ClothAsset;

	if (UChaosClothAsset* const ClothAsset = Cast<UChaosClothAsset>(Asset.Get()))
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: Remove after adding the getter function (currently shelved!)  
		if (UDataflow* const DataflowAsset = ClothAsset->DataflowAsset)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		{
			const TSharedPtr<Dataflow::FGraph, ESPMode::ThreadSafe> Dataflow = DataflowAsset->GetDataflow();
			if (const TSharedPtr<FDataflowNode> BaseNode = Dataflow->FindBaseNode(this->GetGuid()))  // This is basically a safe const_cast
			{
				FChaosClothAssetWeightMapNode* const MutableThis = static_cast<FChaosClothAssetWeightMapNode*>(BaseNode.Get());
				check(MutableThis == this);

				// Make the name a valid attribute name, and replace the value in the UI
				FWeightMapTools::MakeWeightMapName(MutableThis->Name);

				// Transfer weight map if the transfer collection input has changed and is valid
				FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
				const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));
				FCollectionClothConstFacade ClothFacade(ClothCollection);
				if (ClothFacade.IsValid())  // Can only act on the collection if it is a valid cloth collection
				{
					FManagedArrayCollection InTransferCollection = GetValue<FManagedArrayCollection>(Context, &TransferCollection);
					const TSharedRef<const FManagedArrayCollection> TransferClothCollection = MakeShared<const FManagedArrayCollection>(MoveTemp(InTransferCollection));
					FCollectionClothConstFacade TransferClothFacade(TransferClothCollection);
					const FName InInputName = GetInputName(Context);

					const uint32 NameTypeHash = HashCombineFast(GetTypeHash(InInputName), (uint32)TransferType);
					const uint32 InTransferCollectionHash = (TransferClothFacade.HasValidSimulationData() && InInputName != NAME_None) ?
						HashCombineFast(TransferClothFacade.CalculateWeightMapTypeHash(), NameTypeHash) : 0;  // TODO: Remove after adding the function (currently shelved!)  

					if (TransferCollectionHash != InTransferCollectionHash)
					{
						MutableThis->TransferCollectionHash = InTransferCollectionHash;

						if (TransferCollectionHash)
						{
							if (TransferClothFacade.HasWeightMap(InInputName))
							{
								// Remap the weights
								TArray<float> RemappedWeights;
								RemappedWeights.SetNumZeroed(ClothFacade.GetNumSimVertices3D());

								switch (TransferType)
								{
								case EChaosClothAssetWeightMapTransferType::Use2DSimMesh:
									Private::TransferWeightMap(
										TransferClothFacade.GetSimPosition2D(),
										TransferClothFacade.GetSimIndices2D(),
										TransferClothFacade.GetSimVertex3DLookup(),
										TransferClothFacade.GetWeightMap(InInputName),
										ClothFacade.GetSimPosition2D(),
										ClothFacade.GetSimIndices2D(),
										ClothFacade.GetSimVertex3DLookup(),
										RemappedWeights);
									break;
								case EChaosClothAssetWeightMapTransferType::Use3DSimMesh:
									FClothGeometryTools::TransferWeightMap(
										TransferClothFacade.GetSimPosition3D(),
										TransferClothFacade.GetSimIndices3D(),
										TransferClothFacade.GetWeightMap(InInputName),
										ClothFacade.GetSimPosition3D(),
										ClothFacade.GetSimNormal(),
										ClothFacade.GetSimIndices3D(),
										TArrayView<float>(RemappedWeights));
									break;
								default: unimplemented();
								}

								// TODO: Allow for transferring render weights from InTransferCollection?

								MutableThis->SetVertexWeights(ClothFacade.GetWeightMap(InInputName), RemappedWeights);
							}
						}
					}
				}
			}
		}
	}
}

void FChaosClothAssetWeightMapNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;

	auto CheckSourceVertexWeights = [this](TArrayView<float>& ClothWeights, const TArray<float>& SourceVertexWeights, bool bIsSim)
	{
		if (SourceVertexWeights.Num() > 0 && SourceVertexWeights.Num() != ClothWeights.Num())
		{
			FClothDataflowTools::LogAndToastWarning(*this,
				LOCTEXT("VertexCountMismatchHeadline", "Vertex count mismatch."),
				FText::Format(LOCTEXT("VertexCountMismatchDetails", "{0} vertex weights in the node: {1}\n{0} vertices in the cloth: {2}"),
					bIsSim ? FText::FromString("Sim") : FText::FromString("Render"),
					SourceVertexWeights.Num(),
					ClothWeights.Num()));
		}
	};

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		// Evaluate InputName
		const FName InInputName = GetInputName(Context);
		InputName.StringValue_Override = GetValue<FString>(Context, &InputName.StringValue, UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden);

		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));
		FCollectionClothFacade ClothFacade(ClothCollection);
		if (ClothFacade.IsValid())  // Can only act on the collection if it is a valid cloth collection
		{
			const FName InName(Name.IsEmpty() ? InInputName : FName(Name));

			// Copy simulation weights into cloth collection

			if (MeshTarget == EChaosClothAssetWeightMapMeshTarget::Simulation)
			{
				ClothFacade.AddWeightMap(InName);		// Does nothing if weight map already exists
				TArrayView<float> ClothSimWeights = ClothFacade.GetWeightMap(InName);

				if (ClothSimWeights.Num() != ClothFacade.GetNumSimVertices3D())
				{
					check(ClothSimWeights.Num() == 0);
					FClothDataflowTools::LogAndToastWarning(*this,
						LOCTEXT("InvalidSimWeightMapNameHeadline", "Invalid weight map name."),
						FText::Format(LOCTEXT("InvalidSimWeightMapNameDetails", "Could not create a sim weight map with name \"{0}\" (reserved name? wrong type?)."),
							FText::FromName(InName)));
				}
				else
				{
					constexpr bool bIsSim = true;
					CheckSourceVertexWeights(ClothSimWeights, GetVertexWeights(), bIsSim);
					CalculateFinalVertexWeightValues(ClothFacade.GetWeightMap(InInputName), ClothSimWeights);
				}
			}
			else 
			{
				check(MeshTarget == EChaosClothAssetWeightMapMeshTarget::Render);

				ClothFacade.AddUserDefinedAttribute<float>(InName, ClothCollectionGroup::RenderVertices);
				TArrayView<float> ClothRenderWeights = ClothFacade.GetUserDefinedAttribute<float>(InName, ClothCollectionGroup::RenderVertices);

				if (ClothRenderWeights.Num() != ClothFacade.GetNumRenderVertices())
				{
					check(ClothRenderWeights.Num() == 0);
					FClothDataflowTools::LogAndToastWarning(*this,
						LOCTEXT("InvalidRenderWeightMapNameHeadline", "Invalid weight map name."),
						FText::Format(LOCTEXT("InvalidRenderWeightMapNameDetails", "Could not create a render weight map with name \"{0}\" (reserved name? wrong type?)."),
							FText::FromName(InName)));
				}
				else
				{
					constexpr bool bIsSim = false;
					CheckSourceVertexWeights(ClothRenderWeights, GetVertexWeights(), bIsSim);
					CalculateFinalVertexWeightValues(ClothFacade.GetUserDefinedAttribute<float>(InInputName, ClothCollectionGroup::RenderVertices), ClothRenderWeights);
				}
			}
		}

		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
	else if (Out->IsA<FString>(&Name))
	{
		FString InputNameString = GetValue<FString>(Context, &InputName.StringValue);
		UE::Chaos::ClothAsset::FWeightMapTools::MakeWeightMapName(InputNameString);
		SetValue(Context, Name.IsEmpty() ? InputNameString : Name, &Name);
	}
}

FName FChaosClothAssetWeightMapNode::GetInputName(Dataflow::FContext& Context) const
{
	FString InputNameString = GetValue<FString>(Context, &InputName.StringValue);
	UE::Chaos::ClothAsset::FWeightMapTools::MakeWeightMapName(InputNameString);
	const FName InInputName(*InputNameString);
	return InInputName != NAME_None ? InInputName : FName(Name);
}

void FChaosClothAssetWeightMapNode::SetVertexWeights(const TConstArrayView<float> InputMap, const TArray<float>& FinalValues)
{
	UE::Chaos::ClothAsset::Private::SetVertexWeights(InputMap, FinalValues, MapOverrideType, GetVertexWeights());
}

void FChaosClothAssetWeightMapNode::CalculateFinalVertexWeightValues(const TConstArrayView<float> InputMap, TArrayView<float> FinalOutputMap) const
{
	UE::Chaos::ClothAsset::Private::CalculateFinalVertexWeightValues(InputMap, FinalOutputMap, MapOverrideType, GetVertexWeights());
}

// Object encapsulating a change to the WeightMap node's values. Used for Undo/Redo.
class FChaosClothAssetWeightMapNode::FWeightMapNodeChange final : public FToolCommandChange
{

public:

	FWeightMapNodeChange(const FChaosClothAssetWeightMapNode& Node) :
		NodeGuid(Node.GetGuid()),
		SavedWeights(Node.GetVertexWeights()),
		SavedMapOverrideType(Node.MapOverrideType),
		SavedWeightMapName(Node.Name)
	{}

private:

	FGuid NodeGuid;
	TArray<float> SavedWeights;

	EChaosClothAssetWeightMapOverrideType SavedMapOverrideType;
	FString SavedWeightMapName;

	virtual FString ToString() const final
	{
		return TEXT("FChaosClothAssetWeightMapNode::FWeightMapNodeChange");
	}

	virtual void Apply(UObject* Object) final
	{
		SwapApplyRevert(Object);
	}

	virtual void Revert(UObject* Object) final
	{
		SwapApplyRevert(Object);
	}

	void SwapApplyRevert(UObject* Object)
	{
		if (UDataflow* const Dataflow = Cast<UDataflow>(Object))
		{
			if (const TSharedPtr<FDataflowNode> BaseNode = Dataflow->GetDataflow()->FindBaseNode(NodeGuid))
			{
				if (FChaosClothAssetWeightMapNode* const Node = BaseNode->AsType<FChaosClothAssetWeightMapNode>())
				{
					Swap(Node->GetVertexWeights(), SavedWeights);
					Swap(Node->MapOverrideType, SavedMapOverrideType);
					Swap(Node->Name, SavedWeightMapName);

					Node->Invalidate();
				}
			}
		}
	}
};

TUniquePtr<FToolCommandChange> FChaosClothAssetWeightMapNode::MakeWeightMapNodeChange(const FChaosClothAssetWeightMapNode& Node)
{
	return MakeUnique<FChaosClothAssetWeightMapNode::FWeightMapNodeChange>(Node);
}

#undef LOCTEXT_NAMESPACE
