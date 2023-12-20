// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FStochasticShadowsViewState
{
public:
	TRefCountPtr<IPooledRenderTarget> ShadowMaskPageTableHistory;
	TRefCountPtr<IPooledRenderTarget> ShadowMaskAtlasHistory;
	TRefCountPtr<IPooledRenderTarget> ShadowMaskSceneDepthHistory;

	FVector4f HistoryScreenPositionScaleBias = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	FVector4f HistoryUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);

	void SafeRelease()
	{
		ShadowMaskPageTableHistory.SafeRelease();
		ShadowMaskAtlasHistory.SafeRelease();
		ShadowMaskSceneDepthHistory.SafeRelease();
	}
};