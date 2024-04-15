// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialValues/DMMaterialValueRenderTarget.h"
#include "Components/DMRenderTargetRenderer.h"
#include "DMComponentPath.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/CoreDelegates.h"

const FString UDMMaterialValueRenderTarget::RendererPathToken = "Renderer";

UDMMaterialValueRenderTarget::UDMMaterialValueRenderTarget()
	: TextureSize(FIntPoint(512, 512))
	, TextureFormat(ETextureRenderTargetFormat::RTF_RGBA16f)
	, ClearColor(FLinearColor::Black)
	, Renderer(nullptr)
{
#if WITH_EDITOR
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialValueRenderTarget, TextureSize));
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialValueRenderTarget, TextureFormat));
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialValueRenderTarget, ClearColor));
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialValueRenderTarget, Renderer));
#endif
}

UDMMaterialValueRenderTarget::~UDMMaterialValueRenderTarget()
{
	FCoreDelegates::OnEndFrame.Remove(EndOfFrameDelegateHandle);
	EndOfFrameDelegateHandle.Reset();
}

UTextureRenderTarget2D* UDMMaterialValueRenderTarget::GetRenderTarget() const
{
	return Cast<UTextureRenderTarget2D>(GetValue());
}

const FIntPoint& UDMMaterialValueRenderTarget::GetTextureSize() const
{
	return TextureSize;
}

void UDMMaterialValueRenderTarget::SetTextureSize(const FIntPoint& InTextureSize)
{
	if (InTextureSize.X <= 0 || InTextureSize.Y <= 0 || InTextureSize == TextureSize)
	{
		return;
	}

	TextureSize = InTextureSize;

	AsyncCreateRenderTarget();
}

ETextureRenderTargetFormat UDMMaterialValueRenderTarget::GetTextureFormat() const
{
	return TextureFormat;
}

void UDMMaterialValueRenderTarget::SetTextureFormat(ETextureRenderTargetFormat InTextureFormat)
{
	if (InTextureFormat == TextureFormat)
	{
		return;
	}

	TextureFormat = InTextureFormat;

	AsyncCreateRenderTarget();
}

const FLinearColor& UDMMaterialValueRenderTarget::GetClearColor() const
{
	return ClearColor;
}

void UDMMaterialValueRenderTarget::SetClearColor(const FLinearColor& InClearColor)
{
	if (InClearColor == ClearColor)
	{
		return;
	}

	ClearColor = InClearColor;

	AsyncCreateRenderTarget();
}

UDMRenderTargetRenderer* UDMMaterialValueRenderTarget::GetRenderer() const
{
	return Renderer;
}

void UDMMaterialValueRenderTarget::SetRenderer(UDMRenderTargetRenderer* InRenderer)
{
#if WITH_EDITOR
	if (Renderer == InRenderer)
	{
		return;
	}

	if (Renderer)
	{
		Renderer->SetComponentState(EDMComponentLifetimeState::Removed);
	}

	Renderer = InRenderer;

	if (IsComponentAdded())
	{
		Renderer->SetComponentState(EDMComponentLifetimeState::Added);
	}
#endif
}

void UDMMaterialValueRenderTarget::EnsureRenderTarget(bool bInAsync)
{
	if (IsValid(GetRenderTarget()))
	{
		return;
	}

	if (bInAsync)
	{
		AsyncCreateRenderTarget();
	}
	else
	{
		CreateRenderTarget();
	}
}

void UDMMaterialValueRenderTarget::FlushCreateRenderTarget()
{
	if (EndOfFrameDelegateHandle.IsValid() || !IsValid(GetRenderTarget()))
	{
		CreateRenderTarget();
	}
}

void UDMMaterialValueRenderTarget::Update(EDMUpdateType InUpdateType)
{
	Super::Update(InUpdateType);

	if (!IsValid(GetRenderTarget()))
	{
		AsyncCreateRenderTarget();
	}
	else
	{
		UpdateRenderTarget();
	}
}

#if WITH_EDITOR
void UDMMaterialValueRenderTarget::PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent)
{
	Super::PostEditorDuplicate(InMaterialModel, InParent);

	if (IsValid(Renderer))
	{
		Renderer->PostEditorDuplicate(InMaterialModel, this);
	}

	// Make sure we have a unique render target
	AsyncCreateRenderTarget();
}

void UDMMaterialValueRenderTarget::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	static const FName TextureSizeName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueRenderTarget, TextureSize);
	static const FName ClearColorName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueRenderTarget, ClearColor);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	if (MemberName == TextureSizeName || MemberName == ClearColorName)
	{
		AsyncCreateRenderTarget();
	}
}
#endif

void UDMMaterialValueRenderTarget::PostLoad()
{
	Super::PostLoad();

	if (!IsValid(GetRenderTarget()))
	{
		CreateRenderTarget();
	}
}

void UDMMaterialValueRenderTarget::AsyncCreateRenderTarget()
{
	if (!EndOfFrameDelegateHandle.IsValid())
	{
		EndOfFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddUObject(this, &UDMMaterialValueRenderTarget::CreateRenderTarget);
	}
}

void UDMMaterialValueRenderTarget::CreateRenderTarget()
{
	if (EndOfFrameDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(EndOfFrameDelegateHandle);
		EndOfFrameDelegateHandle.Reset();
	}

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(this, NAME_None,
		EObjectFlags::RF_Transactional | EObjectFlags::RF_DuplicateTransient | EObjectFlags::RF_TextExportTransient
	);

	check(RenderTarget);
	RenderTarget->RenderTargetFormat = TextureFormat;
	RenderTarget->ClearColor = ClearColor;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->bCanCreateUAV = false;
	RenderTarget->InitAutoFormat(TextureSize.X, TextureSize.Y);
	RenderTarget->UpdateResourceImmediate(true);

	SetValue(RenderTarget);
}

void UDMMaterialValueRenderTarget::UpdateRenderTarget()
{
	if (Renderer)
	{
		if (UTextureRenderTarget2D* RenderTarget = GetRenderTarget())
		{
			Renderer->UpdateRenderTarget();
		}
		else
		{
			AsyncCreateRenderTarget();
		}
	}
}

UDMMaterialComponent* UDMMaterialValueRenderTarget::GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const
{
	if (InPathSegment.GetToken() == RendererPathToken)
	{
		return Renderer;
	}

	return Super::GetSubComponentByPath(InPath, InPathSegment);
}

#if WITH_EDITOR
void UDMMaterialValueRenderTarget::OnComponentAdded()
{
	Super::OnComponentAdded();

	if (Renderer)
	{
		Renderer->SetComponentState(EDMComponentLifetimeState::Added);
	}

	AsyncCreateRenderTarget();
}

void UDMMaterialValueRenderTarget::OnComponentRemoved()
{
	Super::OnComponentRemoved();

	if (Renderer)
	{
		Renderer->SetComponentState(EDMComponentLifetimeState::Removed);
	}
}
#endif
