// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FSampledDirectLightingViewState
{
public:
	TRefCountPtr<IPooledRenderTarget> DiffuseLightingHistory;
	TRefCountPtr<IPooledRenderTarget> SpecularLightingHistory;
	TRefCountPtr<IPooledRenderTarget> LuminanceMomentsHistory;
	TRefCountPtr<IPooledRenderTarget> SceneDepthHistory;
	TRefCountPtr<IPooledRenderTarget> NumFramesAccumulatedHistory;

	FVector4f HistoryScreenPositionScaleBias = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	FVector4f HistoryUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);

	void SafeRelease()
	{
		DiffuseLightingHistory.SafeRelease();
		SpecularLightingHistory.SafeRelease();
		LuminanceMomentsHistory.SafeRelease();
		SceneDepthHistory.SafeRelease();
		NumFramesAccumulatedHistory.SafeRelease();
	}
};