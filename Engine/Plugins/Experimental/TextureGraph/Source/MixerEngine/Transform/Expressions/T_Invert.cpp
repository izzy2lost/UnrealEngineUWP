// Copyright Epic Games, Inc. All Rights Reserved.

#include "T_Invert.h"
#include "Job/JobArgs.h"
#include "MixerEngine.h"
#include "Math/Vector.h"
#include "FxMat/MaterialManager.h"
#include "Job/JobBatch.h"
#include "Helper/GraphicsUtil.h"

IMPLEMENT_GLOBAL_SHADER(FSH_Invert, "/Plugin/TextureGraph/Expressions/Expression_Invert.usf", "FSH_Invert", SF_Pixel);

TiledBlobPtr T_Invert::Create(MixUpdateCyclePtr Cycle, BufferDescriptor DesiredOutputDesc, TiledBlobPtr Source, float MaxValue, int32 TargetId)
{
	RenderMaterial_FXPtr RenderMaterial = MixerEngine::GetMaterialManager()->CreateMaterial_FX<VSH_Simple, FSH_Invert>(TEXT("T_Invert"));

	check(RenderMaterial);

	JobUPtr RenderJob = std::make_unique<Job>(Cycle->GetMix(), TargetId, std::static_pointer_cast<BlobTransform>(RenderMaterial));
	RenderJob
		->AddArg(ARG_BLOB(Source, "SourceTexture"))
		->AddArg(ARG_FLOAT(MaxValue, "MaxValue"))
		;

	FString Name = TEXT("Invert"); // FString::Printf(TEXT("GaussianBlur.[%s].[%d].[%llu]"), *gmask->ID(), InContext->TargetId, InContext->Cycle->Batch()->BatchId());

	TiledBlobPtr Result = RenderJob->InitResult(Name, &DesiredOutputDesc);
	Cycle->AddJob(TargetId, std::move(RenderJob));

	return Result;
}
