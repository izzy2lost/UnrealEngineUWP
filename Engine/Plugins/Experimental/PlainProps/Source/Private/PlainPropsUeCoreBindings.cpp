// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlainPropsUeCoreBindings.h"
#include "PlainPropsBuild.h"
#include "Math/Transform.h"

namespace PlainProps::UE
{

void FTransformBinding::Save(FMemberBuilder& Dst, const FTransform& Src, const FTransform* Default) const
{
	static_assert(std::is_same_v<decltype(FTransform().GetTranslation().X), double>);

	FVector T = Src.GetTranslation();
	FQuat R = Src.GetRotation();
	FVector S = Src.GetScale3D();

	if (Default)
	{
		if (T != Default->GetTranslation())
		{
			Dst.Add(MemberIds.Translate[0], T.X);
			Dst.Add(MemberIds.Translate[1], T.Y);
			Dst.Add(MemberIds.Translate[2], T.Z);
		}

		if (R != Default->GetRotation())
		{
			Dst.Add(MemberIds.Rotate[0], R.X);
			Dst.Add(MemberIds.Rotate[1], R.Y);
			Dst.Add(MemberIds.Rotate[2], R.Z);
			Dst.Add(MemberIds.Rotate[3], R.W);
		}

		if (S != Default->GetScale3D())
		{
			Dst.Add(MemberIds.Scale[0], S.X);
			Dst.Add(MemberIds.Scale[1], S.Y);
			Dst.Add(MemberIds.Scale[2], S.Z);
		}
	}
	else
	{
		Dst.Add(MemberIds.Translate[0],	T.X);
		Dst.Add(MemberIds.Translate[1],	T.Y);
		Dst.Add(MemberIds.Translate[2],	T.Z);
		Dst.Add(MemberIds.Rotate[0],		R.X);
		Dst.Add(MemberIds.Rotate[1],		R.Y);
		Dst.Add(MemberIds.Rotate[2],		R.Z);
		Dst.Add(MemberIds.Rotate[3],		R.W);
		Dst.Add(MemberIds.Scale[0],		S.X);
		Dst.Add(MemberIds.Scale[1],		S.Y);
		Dst.Add(MemberIds.Scale[2],		S.Z);
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

	if (Members.PeekNameUnchecked() == MemberIds.Translate[0])
	{
		FVector Translation;
		Members.GrabLeaves(&Translation.X, 3);
		Dst.SetTranslation(Translation);

		if (!Members.HasMore())
		{
			return;
		}
	}

	if (Members.PeekNameUnchecked() == MemberIds.Rotate[0])
	{
		FQuat Rotation;
		Members.GrabLeaves(&Rotation.X, 4);
		Dst.SetRotation(Rotation);

		if (!Members.HasMore())
		{
			return;
		}
	}

	checkSlow(Members.PeekNameUnchecked() == MemberIds.Scale[0]);
	FVector Scale;
	Members.GrabLeaves(&Scale.X, 3);
	Dst.SetScale3D(Scale);
	checkSlow(!Members.HasMore());
}

bool FTransformBinding::DiffStruct(const void* StructA, const void* StructB) const
{
	return !static_cast<const FTransform*>(StructA)->Equals(*static_cast<const FTransform*>(StructB), 0.0);
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