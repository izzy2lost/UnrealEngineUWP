// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlainPropsUeCoreBindings.h"
#include "PlainPropsBuild.h"
#include "Math/Transform.h"

namespace PlainProps::UE
{

void FTransformBinding::Save(FMemberBuilder& Dst, const FTransform& Src, const FTransform* Default, const FSaveContext& Context) const
{
	static_assert(std::is_same_v<decltype(FTransform().GetTranslation().X), double>);

	FVector T = Src.GetTranslation();
	FQuat R = Src.GetRotation();
	FVector S = Src.GetScale3D();

	if (Default)
	{
		if (T != Default->GetTranslation())
		{
			Dst.Add(MemberIds[(uint8)EMember::TranslateX], T.X);
			Dst.Add(MemberIds[(uint8)EMember::TranslateY], T.Y);
			Dst.Add(MemberIds[(uint8)EMember::TranslateZ], T.Z);
		}

		if (R != Default->GetRotation())
		{
			Dst.Add(MemberIds[(uint8)EMember::RotateX], R.X);
			Dst.Add(MemberIds[(uint8)EMember::RotateY], R.Y);
			Dst.Add(MemberIds[(uint8)EMember::RotateZ], R.Z);
			Dst.Add(MemberIds[(uint8)EMember::RotateW], R.W);
		}

		if (S != Default->GetScale3D())
		{
			Dst.Add(MemberIds[(uint8)EMember::ScaleX], S.X);
			Dst.Add(MemberIds[(uint8)EMember::ScaleY], S.Y);
			Dst.Add(MemberIds[(uint8)EMember::ScaleZ], S.Z);
		}
	}
	else
	{
		Dst.Add(MemberIds[(uint8)EMember::TranslateX],	T.X);
		Dst.Add(MemberIds[(uint8)EMember::TranslateY],	T.Y);
		Dst.Add(MemberIds[(uint8)EMember::TranslateZ],	T.Z);
		Dst.Add(MemberIds[(uint8)EMember::RotateX], R.X);
		Dst.Add(MemberIds[(uint8)EMember::RotateY], R.Y);
		Dst.Add(MemberIds[(uint8)EMember::RotateZ], R.Z);
		Dst.Add(MemberIds[(uint8)EMember::RotateW], R.W);
		Dst.Add(MemberIds[(uint8)EMember::ScaleX],	S.X);
		Dst.Add(MemberIds[(uint8)EMember::ScaleY],	S.Y);
		Dst.Add(MemberIds[(uint8)EMember::ScaleZ],	S.Z);
	}
}

void FTransformBinding::Load(FTransform& Dst, FStructView Src, ECustomLoadMethod Method, const FLoadBatch& Batch) const
{
	static_assert(std::is_same_v<decltype(FTransform().GetTranslation().X), double>);

	FMemberReader Members(Src);

	if (Method == ECustomLoadMethod::Construct)
	{
		::new (&Dst) FTransform;
	}
				
	if (!Members.HasMore())
	{
		return;
	}

	if (Members.PeekNameUnchecked() == MemberIds[(uint8)EMember::TranslateX])
	{
		FVector Translation;
		Members.GrabLeaves(&Translation.X, 3);
		Dst.SetTranslation(Translation);

		if (!Members.HasMore())
		{
			return;
		}
	}

	if (Members.PeekNameUnchecked() == MemberIds[(uint8)EMember::RotateX])
	{
		FQuat Rotation;
		Members.GrabLeaves(&Rotation.X, 4);
		Dst.SetRotation(Rotation);

		if (!Members.HasMore())
		{
			return;
		}
	}

	checkSlow(Members.PeekNameUnchecked() == MemberIds[(uint8)EMember::ScaleX]);
	FVector Scale;
	Members.GrabLeaves(&Scale.X, 3);
	Dst.SetScale3D(Scale);
	checkSlow(!Members.HasMore());
}

} // namespace PlainProps::UE

//////////////////////////////////////////////////////////////////////////
namespace PlainProps
{
	
template<>
void AppendString(FString& Out, const FName& Name)
{
	Name.AppendString(Out);
}

}