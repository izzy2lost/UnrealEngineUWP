// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Model/Mix/MixInterface.h"
#include "TG_OutputSettings.h"
#include "TextureGraph.generated.h"

class UTG_Graph;
using ErrorReportMap = TMap<int32, TArray<FTextureGraphErrorReport>>;

UCLASS(BlueprintType)
class TEXTUREGRAPH_API UTextureGraph : public UMixInterface
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category="TG_GraphParams")
	TObjectPtr<class UTG_Graph> TextureGraph;

	UPROPERTY(EditAnywhere, Category = NoCategory)
	TObjectPtr<UTG_OutputSettingsSet> OutputSettingsSet;

	bool CheckCyclicDependency(const UTextureGraph* InTextureGraph) const;
	void GatherAllDependentGraphs(TArray<UTextureGraph*>& DependentGraphs) const;
	
public:

	bool IsDependent(const UTextureGraph* TextureGraph) const;
	
	// Construct the script giving it its name Initialize to a default one output script
	virtual void Construct(FString Name);

	// Override Serialize method of UObject
	virtual void Serialize(FArchive& Ar) override;

	// Override the PostLoad method of UObject to allocate settings
	virtual void PostLoad() override;

	// Override PreSave method of UObject
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

	virtual const UTG_Graph*			Graph() const { return TextureGraph; }
	virtual UTG_Graph*					Graph() { return TextureGraph; }
	UTG_OutputSettingsSet*				GetOutputSettingsSet() { return OutputSettingsSet; }

	void								InvalidateAll() override;
	void								Update(MixUpdateCyclePtr InCycle) override;

	void								PostMeshLoad() override;

	void								TriggerUpdate(bool Tweaking);

	void								Log() const;
};
