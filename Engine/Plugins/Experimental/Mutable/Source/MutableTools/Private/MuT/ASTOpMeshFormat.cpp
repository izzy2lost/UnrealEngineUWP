// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshFormat.h"

#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Mesh.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableTrace.h"
#include "MuR/RefCounted.h"
#include "MuR/System.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpMeshClipMorphPlane.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpMeshApplyPose.h"
#include "MuT/ASTOpSwitch.h"


namespace mu
{


//-------------------------------------------------------------------------------------------------
ASTOpMeshFormat::ASTOpMeshFormat()
    : Source(this)
    , Format(this)
{
}


ASTOpMeshFormat::~ASTOpMeshFormat()
{
    // Explicit call needed to avoid recursive destruction
    ASTOp::RemoveChildren();
}


bool ASTOpMeshFormat::IsEqual(const ASTOp& otherUntyped) const
{
	if (otherUntyped.GetOpType() == GetOpType())
	{
		const ASTOpMeshFormat* other = static_cast<const ASTOpMeshFormat*>(&otherUntyped);
        return Source==other->Source && Format==other->Format && Flags ==other->Flags;
    }
    return false;
}


uint64 ASTOpMeshFormat::Hash() const
{
	uint64 res = std::hash<void*>()(Source.child().get() );
    hash_combine( res, Format.child().get() );
    return res;
}


mu::Ptr<ASTOp> ASTOpMeshFormat::Clone(MapChildFuncRef mapChild) const
{
	mu::Ptr<ASTOpMeshFormat> n = new ASTOpMeshFormat();
    n->Source = mapChild(Source.child());
    n->Format = mapChild(Format.child());
	n->Flags = Flags;
	n->bOptimizeBuffers = bOptimizeBuffers;
    return n;
}


void ASTOpMeshFormat::ForEachChild(const TFunctionRef<void(ASTChild&)> f )
{
    f( Source );
    f( Format );
}


void ASTOpMeshFormat::Link( FProgram& program, FLinkerOptions* )
{
    // Already linked?
    if (!linkedAddress)
    {
        OP::MeshFormatArgs args;
        FMemory::Memzero( &args, sizeof(args) );

		args.Flags = Flags;
		if (bOptimizeBuffers)
		{
			args.Flags = args.Flags | OP::MeshFormatArgs::OptimizeBuffers;
		}

		if (Source) args.source = Source->linkedAddress;
		if (Format) args.format = Format->linkedAddress;

        linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
        program.m_opAddress.Add((uint32)program.m_byteCode.Num());
        AppendCode(program.m_byteCode,OP_TYPE::ME_FORMAT);
        AppendCode(program.m_byteCode,args);
    }

}


mu::Ptr<ASTOp> ASTOpMeshFormat::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
{
	mu::Ptr<ASTOp> at = context.MeshFormatSinker.Apply(this);
	return at;
}


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
mu::Ptr<ASTOp> Sink_MeshFormatAST::Apply(const ASTOpMeshFormat* root)
{
	m_root = root;

	OldToNew.Reset();

	m_initialSource = m_root->Source.child();
	mu::Ptr<ASTOp> newSource = Visit(m_initialSource, m_root);

	// If there is any change, it is the new root.
	if (newSource != m_initialSource)
	{
		return newSource;
	}

	return nullptr;
}


namespace
{
	mu::Ptr<const Mesh> FindBaseMeshConstant(mu::Ptr<ASTOp> at)
	{
		mu::Ptr<const Mesh> res;

		switch (at->GetOpType())
		{
		case OP_TYPE::ME_CONSTANT:
		{
			const ASTOpConstantResource* typed = static_cast<const ASTOpConstantResource*>(at.get());
			res = static_cast<const Mesh*>(typed->GetValue().get());
			break;
		}

		default:
			check(false);
		}

		check(res);

		return res;
	}


	// Make a mesh format suitable to morph a particular other format.
	Ptr<Mesh> MakeMorphTargetFormat(Ptr<const Mesh> pTargetFormat)
	{
		MUTABLE_CPUPROFILER_SCOPE(MakeMorphTargetFormat);

		// Make a morph format by adding all the vertex channels from the base into a single
		// vertex buffer

		int32 offset = 0;
		int32 numChannels = 0;
		TArray<EMeshBufferSemantic> semantics;
		TArray<int32> semanticIndices;
		TArray<EMeshBufferFormat> formats;
		TArray<int32> components;
		TArray<int32> offsets;

		// Add the vertex channels from the new format
		for (int32 vb = 0; vb < pTargetFormat->GetVertexBuffers().GetBufferCount(); ++vb)
		{
			for (int32 c = 0; c < pTargetFormat->GetVertexBuffers().GetBufferChannelCount(vb); ++c)
			{
				// Channel info
				EMeshBufferSemantic semantic;
				int semanticIndex;
				EMeshBufferFormat format;
				int component;
				pTargetFormat->GetVertexBuffers().GetChannel(vb, c, &semantic, &semanticIndex, &format, &component, nullptr);

				// TODO: Filter useless semantics for morphing.
				// Maybe some formats like the ones with a packed tangent sign need to be tweaked here, to make sense of the whole buffer.
				semantics.Add(semantic);
				semanticIndices.Add(semanticIndex);
				formats.Add(format);
				components.Add(component);
				offsets.Add(offset);
				offset += components[numChannels] * GetMeshFormatData(formats[numChannels]).SizeInBytes;
				numChannels++;
			}
		}


		MeshPtr pTargetMorphFormat = new Mesh;
		pTargetMorphFormat->GetVertexBuffers().SetBufferCount(1);

		pTargetMorphFormat->GetVertexBuffers().SetBuffer(0, offset,
			numChannels,
			semantics.GetData(),
			semanticIndices.GetData(),
			formats.GetData(),
			components.GetData(),
			offsets.GetData());

		return pTargetMorphFormat;
	}

}


mu::Ptr<ASTOp> Sink_MeshFormatAST::Visit(const mu::Ptr<ASTOp>& at, const ASTOpMeshFormat* currentFormatOp)
{
	if (!at) return nullptr;

	// Already visited?
	const Ptr<ASTOp>* Cached = OldToNew.Find({ at,currentFormatOp });
	if (Cached)
	{
		return *Cached;
	}

	mu::Ptr<ASTOp> newAt = at;
	switch (at->GetOpType())
	{

	case OP_TYPE::ME_APPLYLAYOUT:
	{
		auto newOp = mu::Clone<ASTOpFixed>(at);
		newOp->SetChild(newOp->op.args.MeshApplyLayout.mesh, Visit(newOp->children[newOp->op.args.MeshApplyLayout.mesh].child(), currentFormatOp));
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_SETSKELETON:
	{
		auto newOp = mu::Clone<ASTOpFixed>(at);
		newOp->SetChild(newOp->op.args.MeshSetSkeleton.source, Visit(newOp->children[newOp->op.args.MeshSetSkeleton.source].child(), currentFormatOp));
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_ADDTAGS:
	{
		Ptr<ASTOpMeshAddTags> newOp = mu::Clone<ASTOpMeshAddTags>(at);
		newOp->Source = Visit(newOp->Source.child(), currentFormatOp);
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_CLIPMORPHPLANE:
	{
		auto newOp = mu::Clone<ASTOpMeshClipMorphPlane>(at);
		newOp->source = Visit(newOp->source.child(), currentFormatOp);
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_MORPH:
	{
		// Move the format down the base of the morph
		Ptr<ASTOpMeshMorph> NewOp = mu::Clone<ASTOpMeshMorph>(at);
		NewOp->Base = Visit(NewOp->Base.child(), currentFormatOp);

		// Reformat the morph targets to match the new format.
		Ptr<const Mesh> pTargetFormat = FindBaseMeshConstant(currentFormatOp->Format.child());
		Ptr<const Mesh> pTargetMorphFormat = MakeMorphTargetFormat(pTargetFormat);

		mu::Ptr<ASTOpConstantResource> NewFormatConstant = new ASTOpConstantResource();
		NewFormatConstant->Type = OP_TYPE::ME_CONSTANT;
		NewFormatConstant->SetValue(pTargetMorphFormat, nullptr);

		if (NewOp->Target)
		{
			mu::Ptr<ASTOpMeshFormat> newFormat = mu::Clone<ASTOpMeshFormat>(currentFormatOp);
			newFormat->Flags = OP::MeshFormatArgs::Vertex | OP::MeshFormatArgs::IgnoreMissing;
			newFormat->Format = NewFormatConstant;

			NewOp->Target = Visit(NewOp->Target.child(), newFormat.get());
		}

		newAt = NewOp;
		break;
	}

	case OP_TYPE::ME_MERGE:
	{
		Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(at);
		newOp->SetChild(newOp->op.args.MeshMerge.base, Visit(newOp->children[newOp->op.args.MeshMerge.base].child(), currentFormatOp));
		newOp->SetChild(newOp->op.args.MeshMerge.added, Visit(newOp->children[newOp->op.args.MeshMerge.added].child(), currentFormatOp));
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_APPLYPOSE:
	{
		Ptr<ASTOpMeshApplyPose> NewOp = mu::Clone<ASTOpMeshApplyPose>(at);
		NewOp->base = Visit(NewOp->base.child(), currentFormatOp);
		newAt = NewOp;
		break;
	}

	case OP_TYPE::ME_INTERPOLATE:
	{
		// Move the format down the base of the morph
		Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(at);
		newOp->SetChild(newOp->op.args.MeshInterpolate.base, Visit(newOp->children[newOp->op.args.MeshInterpolate.base].child(), currentFormatOp));

		// Reformat the morph targets to match the new format.
		Ptr<const Mesh> pTargetFormat = FindBaseMeshConstant(currentFormatOp->Format.child());
		Ptr<const Mesh> pTargetMorphFormat = MakeMorphTargetFormat(pTargetFormat);

		mu::Ptr<ASTOpConstantResource> TargetMorphFormatOp = new ASTOpConstantResource();
		TargetMorphFormatOp->Type = OP_TYPE::ME_CONSTANT;
		TargetMorphFormatOp->SetValue(pTargetMorphFormat, nullptr );

		for (int32 t = 0; t < MUTABLE_OP_MAX_INTERPOLATE_COUNT - 1; ++t)
		{
			if (newOp->children[newOp->op.args.MeshInterpolate.targets[t]])
			{
				mu::Ptr<ASTOpMeshFormat> newFormat = mu::Clone<ASTOpMeshFormat>(currentFormatOp);
				newFormat->Flags = OP::MeshFormatArgs::Vertex | OP::MeshFormatArgs::IgnoreMissing;
				newFormat->Format = TargetMorphFormatOp;

				newOp->SetChild(newOp->op.args.MeshInterpolate.targets[t], Visit(newOp->children[newOp->op.args.MeshInterpolate.targets[t]].child(), newFormat.get()));
			}
		}

		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_REMOVEMASK:
	{
		auto newOp = mu::Clone<ASTOpMeshRemoveMask>(at);
		newOp->source = Visit(newOp->source.child(), currentFormatOp);
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_CONDITIONAL:
	{
		auto newOp = mu::Clone<ASTOpConditional>(at);
		newOp->yes = Visit(newOp->yes.child(), currentFormatOp);
		newOp->no = Visit(newOp->no.child(), currentFormatOp);
		newAt = newOp;
		break;
	}

	case OP_TYPE::ME_SWITCH:
	{
		auto newOp = mu::Clone<ASTOpSwitch>(at);
		newOp->def = Visit(newOp->def.child(), currentFormatOp);
		for (auto& c : newOp->cases)
		{
			c.branch = Visit(c.branch.child(), currentFormatOp);
		}
		newAt = newOp;
		break;
	}

	// This cannot be sunk since the result is different. Since the clipping is now correctly
	// generated at the end of the chain when really necessary, this wrong optimisation is no 
	// longer needed.
	//case OP_TYPE::ME_CLIPMORPHPLANE:
//         {
//             // We move the mask creation down the source
//             auto typedAt = dynamic_cast<const ASTOpMeshClipMorphPlane*>(at.get());
//             newAt = Visit(typedAt->source.child());
//             break;
//         }

	case OP_TYPE::ME_FORMAT:
		// TODO: The child format can be removed. 
		// Unless channels are removed and re-added, which would change their content?
		break;

	case OP_TYPE::ME_DIFFERENCE:

	default:
		if (at != m_initialSource)
		{
			mu::Ptr<ASTOpMeshFormat> newOp = mu::Clone<ASTOpMeshFormat>(currentFormatOp);
			newOp->Source = at;
			newAt = newOp;
		}
		break;

	}

	OldToNew.Add({ at,currentFormatOp }, newAt);

	return newAt;
}

}
