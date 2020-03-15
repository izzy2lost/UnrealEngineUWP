//*********************************************************
// Copyright (c) Microsoft. All rights reserved.
//*********************************************************
#include "UWPMovieStreamer.h"


#include "Slate/SlateTextures.h"
#include "Windows/ComPointer.h"

DEFINE_LOG_CATEGORY(LogUWPMoviePlayer);

#if WIN10_SDK_VERSION >= 15063

#include "UWP/AllowWindowsPlatformTypes.h"
#include <dxgi.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <d3d11on12.h>


FUWPMovieStreamer::FUWPMovieStreamer()
	: FramesReady(0)
	, FramesRendered(0)
	, Player(nullptr)
	, Playlist(nullptr)
	, VideoSurface(nullptr)
	, FinishedFlag(1)
{
	MovieViewport = MakeShared<FMovieViewport>();
	Texture = MakeShared<FSlateTexture2DRHIRef, ESPMode::ThreadSafe>(0, 0, PF_A2B10G10R10, nullptr, TexCreate_Dynamic, true);
}

FUWPMovieStreamer::~FUWPMovieStreamer()
{
	Cleanup();
}

bool FUWPMovieStreamer::Init(const TArray<FString>& MoviePaths, TEnumAsByte<EMoviePlaybackType> InPlaybackType)
{
	using namespace Platform;
	using namespace Windows::Foundation;
	using namespace Windows::Media::Core;
	using namespace Windows::Media::Playback;

	check(IsFinished());

	if (MoviePaths.Num() == 0)
	{
		return false;
	}

	// Ensure that the runtime version of Windows supports use of MediaPlayer in the manner we rely on.
	if (!Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent("Windows.Media.Playback.MediaPlayer", "IsVideoFrameServerEnabled"))
	{
		UE_LOG(LogUWPMoviePlayer, Warning, TEXT("FUWPMovieStreamer is not functional on this version of Windows.  Minimum version is 10.0.15063.0."))
		return false;
	}

	Player = ref new MediaPlayer();

	// Inform player we'll be extracting video frames to render ourselves.
	Player->IsVideoFrameServerEnabled = true;

	// Convert the full set of movie paths to a playlist up-front and then
	// just let the player run free - allows for seamless transitions between items.
	Playlist = ref new MediaPlaybackList();
	Playlist->AutoRepeatEnabled = (InPlaybackType == MT_Looped || InPlaybackType == MT_LoadingLoop);
	for (const FString& MovieName : MoviePaths)
	{
		FString MoviePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() + TEXT("Movies/") + MovieName);
		// Source won't resolve without the extension, assume mp4 if not given.
		if (FPaths::GetExtension(MoviePath).IsEmpty())
		{
			MoviePath += TEXT(".mp4");
		}
		FString MovieUri = FString(TEXT("file://")) + MoviePath;
		MediaSource^ Source = MediaSource::CreateFromUri(ref new Uri(ref new String(*MovieUri)));
		MediaPlaybackItem^ MediaItem = ref new MediaPlaybackItem(Source);
		Playlist->Items->Append(MediaItem);
	}

	Player->Source = Playlist;

	FPlatformAtomics::InterlockedExchange(&FramesReady, 0);
	FramesRendered = 0;

	TSharedPtr<FUWPMovieStreamer> ThisStreamer = SharedThis(this);

	// Schedule additional setup on the render thread (required when primary RHI is D3D12)
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(InitMoviePlayer,
		TSharedPtr<FUWPMovieStreamer>, ThisStreamer, ThisStreamer,
		{
			ThisStreamer->Init_RenderThread();
		});

	// Register relevant event handlers for playback.
	Player->VideoFrameAvailable += ref new TypedEventHandler<MediaPlayer^, Object^>(
		[ThisStreamer](MediaPlayer^, Object^)
	{
		ThisStreamer->OnNewFrameReady();
	});

	Player->MediaEnded += ref new TypedEventHandler<MediaPlayer^, Object^>(
		[ThisStreamer](MediaPlayer^, Object^)
	{
		ThisStreamer->ForceCompletion();
	});

	Playlist->CurrentItemChanged += ref new TypedEventHandler<MediaPlaybackList^, CurrentMediaPlaybackItemChangedEventArgs^>(
		[ThisStreamer, InPlaybackType](MediaPlaybackList^ Playlist, CurrentMediaPlaybackItemChangedEventArgs^ Args)
	{
		// In LoadingLoop mode only the final item should loop.  Remove all others from the playlist as they are completed.
		if (InPlaybackType == MT_LoadingLoop && Args->OldItem != nullptr && Playlist->Items->Size > 1)
		{
			Playlist->Items->RemoveAt(0);
		}
		if (Args->NewItem != nullptr)
		{
			ThisStreamer->OnPlaybackNewItem(Args->NewItem);
		}
	});

	Playlist->ItemFailed += ref new TypedEventHandler<MediaPlaybackList^, MediaPlaybackItemFailedEventArgs^>(
		[ThisStreamer](MediaPlaybackList^ Playlist, MediaPlaybackItemFailedEventArgs^ Args)
	{
		UE_LOG(LogUWPMoviePlayer, Warning, TEXT("Movie %s failed to open."), Args->Item->Source->Uri->DisplayUri->Data());

		for (uint32 i = 0; i < Playlist->Items->Size; ++i)
		{
			if (Playlist->Items->GetAt(i) == Args->Item)
			{
				Playlist->Items->RemoveAt(i);
				break;
			}
		}

		if (Playlist->Items->Size == 0)
		{
			ThisStreamer->ForceCompletion();
		}
	});

	FPlatformAtomics::InterlockedExchange(&FinishedFlag, 0);
	Player->Play();

	return true;
}

void FUWPMovieStreamer::ForceCompletion()
{
	FPlatformAtomics::InterlockedExchange(&FinishedFlag, 1);
}

bool FUWPMovieStreamer::Tick(float DeltaTime)
{
	if (Player == nullptr)
	{
		return true;
	}

	// Check whether the player has produced a new frame.
	int32 OriginalFramesReady = FPlatformAtomics::InterlockedCompareExchange(&FramesReady, FramesRendered, FramesRendered);
	if (OriginalFramesReady != FramesRendered && VideoSurface != nullptr)
	{
		// The copy operation uses D3D11.  If primary RHI is D3D12 then we must
		// bracket it with 11on12 calls to properly manage resource state.
		if (InteropDevice.IsValid())
		{
			check(InteropTexture.IsValid());
			InteropDevice->AcquireWrappedResources(&InteropTexture, 1);
		}

		Player->CopyFrameToVideoSurface(VideoSurface);

		if (InteropDevice.IsValid())
		{
			check(InteropTexture.IsValid());
			InteropDevice->ReleaseWrappedResources(&InteropTexture, 1);

			check(InteropImmediateContext.IsValid());
			InteropImmediateContext->Flush();
		}

		FramesRendered = OriginalFramesReady;

		// We don't set the viewport texture initially to avoid displaying garbage prior to the first frame.
		// Harmless to just set it unconditionally here.
		MovieViewport->SetTexture(Texture);
	}

	return IsFinished();
}

void FUWPMovieStreamer::Cleanup()
{
	check(IsFinished());

	Player = nullptr;
	Playlist = nullptr;

	InteropTexture.Reset();
	InteropImmediateContext.Reset();
	InteropDevice.Reset();
	VideoSurface = nullptr;
	BeginReleaseResource(Texture.Get());

	FPlatformAtomics::InterlockedExchange(&FramesReady, 0);
	FramesRendered = 0;
}

FString FUWPMovieStreamer::GetMovieName()
{
	return (Playlist != nullptr && Playlist->CurrentItem)
		? Playlist->CurrentItem->GetDisplayProperties()->VideoProperties->Title->Data()
		: TEXT("");
}

bool FUWPMovieStreamer::IsLastMovieInPlaylist()
{
	return Playlist != nullptr && Playlist->CurrentItemIndex == Playlist->Items->Size - 1;
}

void FUWPMovieStreamer::OnNewFrameReady()
{
	FPlatformAtomics::InterlockedIncrement(&FramesReady);
}

void FUWPMovieStreamer::Init_RenderThread()
{
	// Copying the video frame from the player to a UE texture uses D3D11.
	// So validate that the native device is D3D11.  If not, we assume it's 12
	// and create an 11on12 representation for it.
	IUnknown* NativeDevice = static_cast<IUnknown*>(RHIGetNativeDevice());
	TComPtr<ID3D11Device> D3D11Device;
	NativeDevice->QueryInterface(IID_PPV_ARGS(&D3D11Device));
	if (!D3D11Device.IsValid())
	{
		UE_LOG(LogUWPMoviePlayer, Log, TEXT("Native device is not D3D11.  Using D3D11On12."));
		HRESULT Hr = D3D11On12CreateDevice(NativeDevice, 0, nullptr, 0, nullptr, 0, 0, &D3D11Device, &InteropImmediateContext, nullptr);
		if (SUCCEEDED(Hr))
		{
			InteropDevice.FromQueryInterface(IID_ID3D11On12Device, D3D11Device);
			check(InteropDevice.IsValid());
		}
		else
		{
			UE_LOG(LogUWPMoviePlayer, Warning, TEXT("D3D11On12CreateDevice failed with error %08X.  Movies will not play."), Hr);
			ForceCompletion();
		}
	}
}

void FUWPMovieStreamer::InitMovieTexture_RenderThread(uint32 Width, uint32 Height)
{
	// When we change the D3D resource that backs the Slate texture
	// we must create a new IDirect3DSurface for it.
	bool bNeedsNewVideoSurface = (VideoSurface == nullptr);
	if (!Texture->IsInitialized())
	{
		Texture->InitResource();
		bNeedsNewVideoSurface = true;
	}

	if (Texture->GetWidth() != Width || Texture->GetHeight() != Height)
	{
		UE_LOG(LogUWPMoviePlayer, Log, TEXT("Resizing target texture to %dx%d."), Width, Height);
		Texture->Resize(Width, Height);
		bNeedsNewVideoSurface = true;
	}

	if (bNeedsNewVideoSurface)
	{
		VideoSurface = nullptr;

		TComPtr<IDXGISurface> DxgiSurface;
		IUnknown* UnkTex = static_cast<IUnknown*>(Texture->GetRHIRef()->GetNativeResource());

		// Create a D3D11 representation of the target texture if necessary
		if (InteropDevice.IsValid())
		{
			D3D11_RESOURCE_FLAGS OverrideFlags = { 0 };
			HRESULT Hr = InteropDevice->CreateWrappedResource(UnkTex,
				&OverrideFlags,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				IID_PPV_ARGS(&InteropTexture));

			if (SUCCEEDED(Hr))
			{
				UnkTex = static_cast<IUnknown*>(InteropTexture);
			}
			else
			{
				UnkTex = nullptr;
			}
		}

		if (UnkTex != nullptr)
		{
			if (SUCCEEDED(UnkTex->QueryInterface(IID_PPV_ARGS(&DxgiSurface))))
			{
				try
				{
					VideoSurface = Windows::Graphics::DirectX::Direct3D11::CreateDirect3DSurface(DxgiSurface);
				}
				catch (Platform::Exception^ Ex)
				{
					UE_LOG(LogUWPMoviePlayer, Warning, TEXT("Windows::Graphics::DirectX::Direct3D11::CreateDirect3DSurface failed with error %08X (%s)."), Ex->HResult, Ex->Message->Data());
				}
			}
		}
	}

	if (VideoSurface == nullptr)
	{
		UE_LOG(LogUWPMoviePlayer, Warning, TEXT("Failed to create IDirect3DSurface representation.  Abandoning playback."));
		ForceCompletion();
	}
}

void FUWPMovieStreamer::OnPlaybackNewItem(Windows::Media::Playback::MediaPlaybackItem^ MediaItem)
{
	// We can now access the video frame size and adjust our texture to match.
	if (MediaItem->VideoTracks->Size > 0)
	{
		UE_LOG(LogUWPMoviePlayer, Log, TEXT("Starting playback of %s"), MediaItem->Source->Uri->DisplayUri->Data());

		Windows::Media::MediaProperties::VideoEncodingProperties^ VideoProperties = MediaItem->VideoTracks->GetAt(0)->GetEncodingProperties();

		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(InitMovieTexture,
			TSharedPtr<FUWPMovieStreamer>, ThisStreamer, SharedThis(this),
			uint32, Width, VideoProperties->Width,
			uint32, Height, VideoProperties->Height,
			{
				ThisStreamer->InitMovieTexture_RenderThread(Width, Height);
			});
	}
}

bool FUWPMovieStreamer::IsFinished()
{
	return FPlatformAtomics::InterlockedCompareExchange(&FinishedFlag, 1, 1) == 1;
}

#include "UWP/HideWindowsPlatformTypes.h"

#endif