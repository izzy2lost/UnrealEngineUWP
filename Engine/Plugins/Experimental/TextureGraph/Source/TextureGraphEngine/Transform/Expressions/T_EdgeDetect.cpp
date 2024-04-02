// Copyright Epic Games, Inc. All Rights Reserved.

#include "T_EdgeDetect.h"
#include "FxMat/MaterialManager.h"
#include "Job/JobBatch.h"
#include "Transform/Utility/T_CombineTiledBlob.h"

IMPLEMENT_GLOBAL_SHADER(FSH_EdgeDetect, "/Plugin/TextureGraph/Expressions/Expression_EdgeDetect.usf", "FSH_EdgeDetect", SF_Pixel);

TiledBlobPtr T_EdgeDetect::Create(MixUpdateCyclePtr InCycle, BufferDescriptor DesiredDesc, TiledBlobPtr SourceTexture, float Thickness, int32 InTargetId)
{
	if (!SourceTexture)
		return TextureHelper::GetBlack();

	RenderMaterial_FXPtr RenderMaterial = TextureGraphEngine::GetMaterialManager()->CreateMaterial_FX<VSH_Simple, FSH_EdgeDetect>(TEXT("T_EdgeDetect"));
	check(RenderMaterial);

	// Combine the tiled texture
	TiledBlobPtr CombinedBlob = T_CombineTiledBlob::Create(InCycle, SourceTexture->GetDescriptor(), InTargetId, SourceTexture);
	float StepX = Thickness / (float)SourceTexture->GetWidth();
	float StepY = Thickness / (float)SourceTexture->GetHeight();

	BufferDescriptor Desc = BufferDescriptor::Combine(DesiredDesc, SourceTexture->GetDescriptor());

	FTileInfo TileInfo;
	JobUPtr RenderJob = std::make_unique<Job>(InCycle->GetMix(), InTargetId, std::static_pointer_cast<BlobTransform>(RenderMaterial));
	RenderJob
		->AddArg(ARG_TILEINFO(TileInfo, "TileInfo"))
		->AddArg(ARG_BLOB(CombinedBlob, "SourceTexture"))
		->AddArg(ARG_FLOAT(Thickness, "Thickness"))
		;

	const FString Name = FString::Printf(TEXT("T_EdgeDetect.[%llu]"), InCycle->GetBatch()->GetBatchId());

	TiledBlobPtr Result = RenderJob->InitResult(Name, &Desc);
	InCycle->AddJob(InTargetId, std::move(RenderJob));

	return Result;
}
