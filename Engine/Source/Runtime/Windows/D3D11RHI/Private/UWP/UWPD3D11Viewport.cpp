// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "D3D11RHIPrivate.h"
#include "RenderCore.h"

#include "AllowWindowsPlatformTypes.h"
	#include <DirectXMath.h>
	#include <dxgi1_2.h>
#include "HideWindowsPlatformTypes.h"

#pragma warning(disable : 4946)	// reinterpret_cast used between related classes: 'Platform::Object' and ...

using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;

extern FD3D11Texture2D* GetSwapChainSurface(FD3D11DynamicRHI* D3DRHI, EPixelFormat PixelFormat, IDXGISwapChain* SwapChain);

DXGI_FORMAT GetSupportedSwapChainBufferFormat(DXGI_FORMAT InPreferredDXGIFormat)
{
	switch (InPreferredDXGIFormat)
	{
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return InPreferredDXGIFormat;

	// More sophisticated fallbacks here?

	default:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	}
}

// FD3D11Viewport::GetBackBufferFormat()
//{
//	return DXGI_FORMAT_B8G8R8A8_UNORM;
//}

extern void D3D11TextureAllocated2D( FD3D11Texture2D& Texture );

FD3D11Viewport::FD3D11Viewport(FD3D11DynamicRHI* InD3DRHI,HWND InWindowHandle,uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat):
	D3DRHI(InD3DRHI),
	LastFlipTime(0),
	LastFrameComplete(0),
	LastCompleteTime(0),
	SyncCounter(0),
	bSyncedLastFrame(false),
	WindowHandle(InWindowHandle),
	SizeX(InSizeX),
	SizeY(InSizeY),
	bIsFullscreen(bInIsFullscreen),
	PixelFormat(InPreferredPixelFormat),
	bIsValid(true),
	FrameSyncEvent(InD3DRHI)
{
	D3DRHI->Viewports.Add(this);

	// Ensure that the D3D device has been created.
	D3DRHI->InitD3DDevice();

	// Create a backbuffer/swapchain for each viewport
	// First, retrieve the underlying DXGI Device from the D3D Device
	TRefCountPtr<IDXGIDevice1> DXGIDevice;
	VERIFYD3D11RESULT(D3DRHI->GetDevice()->QueryInterface(__uuidof(DXGIDevice), (void**)DXGIDevice.GetInitReference() ));

	IDXGIAdapter* pdxgiAdapter;
	VERIFYD3D11RESULT(DXGIDevice->GetAdapter(&pdxgiAdapter));

	IDXGIFactory2* pdxgiFactory;
	VERIFYD3D11RESULT(pdxgiAdapter->GetParent(__uuidof(pdxgiFactory), reinterpret_cast< void** >(&pdxgiFactory)));

	// Create the swapchain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};
	swapChainDesc.Width = SizeX;
	swapChainDesc.Height = SizeY;
	swapChainDesc.Format = GetSupportedSwapChainBufferFormat(GetRenderTargetFormat(PixelFormat));
	swapChainDesc.Stereo = false; 
	swapChainDesc.SampleDesc.Count = 1;                          // don't use multi-sampling
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	swapChainDesc.BufferCount = 2;                               // use two buffers to enable flip effect
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // we recommend using this swap effect for all applications
	swapChainDesc.Flags = 0;

	IDXGISwapChain1* CreatedSwapChain = NULL;

	VERIFYD3D11RESULT(pdxgiFactory->CreateSwapChainForCoreWindow(
		D3DRHI->GetDevice(), 
		reinterpret_cast< IUnknown* >(CoreWindow::GetForCurrentThread()),
		&swapChainDesc,
		NULL,
		(IDXGISwapChain1**)&CreatedSwapChain
		)
		);

	*(SwapChain.GetInitReference()) = CreatedSwapChain;

	// Set the DXGI message hook to not change the window behind our back.
	//D3DRHI->GetFactory()->MakeWindowAssociation(WindowHandle,DXGI_MWA_NO_WINDOW_CHANGES);

	// Create a RHI surface to represent the viewport's back buffer.
	BackBuffer = GetSwapChainSurface(D3DRHI, PixelFormat, SwapChain);

	GRHISupportsHDROutput =
		(swapChainDesc.Format == DXGI_FORMAT_R10G10B10A2_TYPELESS) ||
		(swapChainDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT);

	BeginInitResource(&FrameSyncEvent);
}

void FD3D11Viewport::ConditionalResetSwapChain(bool bIgnoreFocus)
{
	if(!bIsValid)
	{
		const bool bIsFocused = true;
		const bool bIsIconic = false;
		if(bIgnoreFocus || (bIsFocused && !bIsIconic) )
		{
			FlushRenderingCommands();

			HRESULT Result = SwapChain->SetFullscreenState(bIsFullscreen,NULL);
			if(SUCCEEDED(Result))
			{
				bIsValid = true;
			}
			else
			{
				// Even though the docs say SetFullscreenState always returns S_OK, that doesn't always seem to be the case.
				UE_LOG(LogD3D11RHI, Log, TEXT("IDXGISwapChain::SetFullscreenState returned %08x; waiting for the next frame to try again."),Result);
			}
		}
	}
}