// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlainPropsUeCoreBindings.h"
#include "PlainPropsBuild.h"
#include "Math/Transform.h"

namespace PlainProps::UE
{

void FTransformBinding::Save(FMemberBuilder& Dst, const FTransform& Src, const FTransform* Default, const FTransformIds& Ids) const
{
	static_assert(std::is_same_v<decltype(FTransform().GetTranslation().X), double>);

	FVector T = Src.GetTranslation();
	FQuat R = Src.GetRotation();
	FVector S = Src.GetScale3D();

	if (Default)
	{
		if (T != Default->GetTranslation())
		{
			Dst.Add(Ids.Translate[0], T.X);
			Dst.Add(Ids.Translate[1], T.Y);
			Dst.Add(Ids.Translate[2], T.Z);
		}

		if (R != Default->GetRotation())
		{
			Dst.Add(Ids.Rotate[0], R.X);
			Dst.Add(Ids.Rotate[1], R.Y);
			Dst.Add(Ids.Rotate[2], R.Z);
			Dst.Add(Ids.Rotate[3], R.W);
		}

		if (S != Default->GetScale3D())
		{
			Dst.Add(Ids.Scale[0], S.X);
			Dst.Add(Ids.Scale[1], S.Y);
			Dst.Add(Ids.Scale[2], S.Z);
		}
	}
	else
	{
		Dst.Add(Ids.Translate[0],	T.X);
		Dst.Add(Ids.Translate[1],	T.Y);
		Dst.Add(Ids.Translate[2],	T.Z);
		Dst.Add(Ids.Rotate[0],		R.X);
		Dst.Add(Ids.Rotate[1],		R.Y);
		Dst.Add(Ids.Rotate[2],		R.Z);
		Dst.Add(Ids.Rotate[3],		R.W);
		Dst.Add(Ids.Scale[0],		S.X);
		Dst.Add(Ids.Scale[1],		S.Y);
		Dst.Add(Ids.Scale[2],		S.Z);
	}
}

void FTransformBinding::Load(FTransform& Dst, FStructView Src, ECustomLoadMethod Method, const FLoadBatch& Batch, const FTransformIds& Ids) const
{
	FMemberReader Members(Src);

	if (Method == ECustomLoadMethod::Construct)
	{
		::new (&Dst) FTransform;
	}
				
	if (!Members.HasMore())
	{
		return;
	}

	FMemberId Name = Members.PeekName().Get();
	double X = Members.GrabLeaf().AsDouble();
	double Y = Members.GrabLeaf().AsDouble();
	double Z = Members.GrabLeaf().AsDouble();

	if (Name == Ids.Translate[0])
	{
		Dst.SetTranslation({X, Y, Z});
			
		if (!Members.HasMore())
		{
			return;
		}
			
		Name = Members.PeekName().Get();
		X = Members.GrabLeaf().AsDouble();
		Y = Members.GrabLeaf().AsDouble();
		Z = Members.GrabLeaf().AsDouble();
	}

	if (Name == Ids.Rotate[0])
	{
		double W = Members.GrabLeaf().AsDouble();
		Dst.SetRotation({X, Y, Z, W});

		if (!Members.HasMore())
		{
			return;
		}

		Name = Members.PeekName().Get();
		X = Members.GrabLeaf().AsDouble();
		Y = Members.GrabLeaf().AsDouble();
		Z = Members.GrabLeaf().AsDouble();
	}

	check(Name == Ids.Scale[0]);
	Dst.SetScale3D({X, Y, Z});
	check(!Members.HasMore());
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