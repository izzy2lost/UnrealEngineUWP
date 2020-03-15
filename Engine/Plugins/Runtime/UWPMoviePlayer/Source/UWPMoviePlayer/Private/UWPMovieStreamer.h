//*********************************************************
// Copyright (c) Microsoft. All rights reserved.
//*********************************************************
#pragma once

#include "CoreMinimal.h"
#include "MoviePlayer.h"
#include "Windows/ComPointer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUWPMoviePlayer, Log, All);

#if WIN10_SDK_VERSION >= 15063

class FUWPMovieStreamer : public IMovieStreamer, public TSharedFromThis<FUWPMovieStreamer>
{
public:
	FUWPMovieStreamer();
	virtual ~FUWPMovieStreamer();

	/** IMovieStreamer interface */
	virtual bool Init(const TArray<FString>& MoviePaths, TEnumAsByte<EMoviePlaybackType> inPlaybackType) override;
	virtual void ForceCompletion() override;
	virtual bool Tick(float DeltaTime) override;
	virtual TSharedPtr<class ISlateViewport> GetViewportInterface() override
	{
		return MovieViewport;
	}
	virtual float GetAspectRatio() const override
	{
		return (float)MovieViewport->GetSize().X / (float)MovieViewport->GetSize().Y;
	}
	virtual void Cleanup() override;

	virtual FString GetMovieName() override;
	virtual bool IsLastMovieInPlaylist() override;

	FOnCurrentMovieClipFinished OnCurrentMovieClipFinishedDelegate;
	virtual FOnCurrentMovieClipFinished& OnCurrentMovieClipFinished() override { return OnCurrentMovieClipFinishedDelegate; }

	virtual FTexture2DRHIRef GetTexture() override { return Texture.IsValid() ? Texture->GetRHIRef() : nullptr; }

private:

	void Init_RenderThread();
	void InitMovieTexture_RenderThread(uint32 Width, uint32 Height);

	void OnNewFrameReady();
	void OnPlaybackNewItem(Windows::Media::Playback::MediaPlaybackItem^ MediaItem);

	bool IsFinished();
	void SetFinished();

	int32 FramesReady;
	int32 FramesRendered;
	int32 FinishedFlag;

	TSharedPtr<FMovieViewport> MovieViewport;
	TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> Texture;

	Windows::Media::Playback::MediaPlayer^ Player;
	Windows::Media::Playback::MediaPlaybackList^ Playlist;
	Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface^ VideoSurface;

	// The MediaPlayer relies on D3D11 to output to texture.  When the engine
	// is running with D3D12 we must use the 11-on-12 layer.
	TComPtr<struct ID3D11On12Device> InteropDevice;
	TComPtr<struct ID3D11DeviceContext> InteropImmediateContext;
	TComPtr<struct ID3D11Resource> InteropTexture;
};

#endif // WIN10_SDK_VERSION >= 15063
