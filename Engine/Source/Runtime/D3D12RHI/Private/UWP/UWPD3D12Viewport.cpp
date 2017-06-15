// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Viewport.cpp: D3D viewport RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "RenderCore.h"

#include "Windows/ComPointer.h"
#include "AllowWindowsPlatformTypes.h"
#include "Windows.h"

using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;

extern FD3D12Texture2D* GetSwapChainSurface(FD3D12Device* Parent, EPixelFormat PixelFormat, IDXGISwapChain* SwapChain, const uint32 &backBufferIndex);

FD3D12Viewport::FD3D12Viewport(class FD3D12Adapter* InParent, HWND InWindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat) :
	LastFlipTime(0),
	LastFrameComplete(0),
	LastCompleteTime(0),
	SyncCounter(0),
	bSyncedLastFrame(false),
	WindowHandle(InWindowHandle),
	MaximumFrameLatency(3),
	SizeX(InSizeX),
	SizeY(InSizeY),
	bIsFullscreen(bInIsFullscreen),
	PixelFormat(InPreferredPixelFormat),
	bIsValid(true),
	NumBackBuffers(DefaultNumBackBuffers),
	BackBuffer(nullptr),
	CurrentBackBufferIndex(0),
	Fence(InParent, L"Viewport Fence"),
	LastSignaledValue(0),
	pCommandQueue(nullptr),
#if PLATFORM_SUPPORTS_MGPU
	FramePacerRunnable(nullptr),
#endif //PLATFORM_SUPPORTS_MGPU
	FD3D12AdapterChild(InParent)
{
	check(IsInGameThread());
	GetParentAdapter()->GetViewports().Add(this);
}

void FD3D12Viewport::CalculateSwapChainDepth()
{
	FD3D12Adapter* Adapter = GetParentAdapter();
	NumBackBuffers = (Adapter->AlternateFrameRenderingEnabled()) ? (AFRNumBackBuffersPerNode * Adapter->GetNumGPUNodes()) : DefaultNumBackBuffers;

	BackBuffers.Empty();
	BackBuffers.AddZeroed(NumBackBuffers);
}

//Init for a Viewport that will do the presenting
void FD3D12Viewport::Init(IDXGIFactory* Factory, bool AssociateWindow)
{
	FD3D12Adapter* Adapter = GetParentAdapter();

	Fence.CreateFence();

	CalculateSwapChainDepth();

	DXGI_SWAP_CHAIN_FLAG swapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	const DXGI_MODE_DESC BufferDesc = SetupDXGI_MODE_DESC();

	DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = { 0 };
	{
		// MSAA Sample count
		SwapChainDesc.SampleDesc.Count = 1;
		SwapChainDesc.SampleDesc.Quality = 0;
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
		SwapChainDesc.Width = BufferDesc.Width;
		SwapChainDesc.Height = BufferDesc.Height;
		SwapChainDesc.Format = BufferDesc.Format;
		// 1:single buffering, 2:double buffering, 3:triple buffering
		SwapChainDesc.BufferCount = NumBackBuffers;
		SwapChainDesc.Scaling = DXGI_SCALING_NONE;
		SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		// DXGI_SWAP_EFFECT_DISCARD / DXGI_SWAP_EFFECT_SEQUENTIAL
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		SwapChainDesc.Flags = swapChainFlags;

		pCommandQueue = Adapter->GetDevice()->GetCommandListManager().GetD3DCommandQueue();

		TComPtr<IDXGIFactory4> Factory4;
		Factory4.FromQueryInterface(__uuidof(IDXGIFactory4), Factory);

		VERIFYD3D12RESULT(Factory4->CreateSwapChainForCoreWindow(
			pCommandQueue,
			reinterpret_cast< IUnknown* >(CoreWindow::GetForCurrentThread()),
			&SwapChainDesc,
			NULL,
			SwapChain1.GetInitReference()
		));
	}

	if (AssociateWindow)
	{
		// Set the DXGI message hook to not change the window behind our back.
		Factory->MakeWindowAssociation(WindowHandle, DXGI_MWA_NO_WINDOW_CHANGES);
	}

	// Resize to setup mGPU correctly.
	Resize(BufferDesc.Width, BufferDesc.Height, bIsFullscreen);
}

void FD3D12Viewport::ConditionalResetSwapChain(bool bIgnoreFocus)
{
}

HRESULT FD3D12Viewport::PresentInternal(int32 SyncInterval)
{
	return SwapChain1->Present(SyncInterval, 0);
}

#include "HideWindowsPlatformTypes.h"
