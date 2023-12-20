// Copyright Epic Games, Inc. All Rights Reserved.
#include "Blob.h"
#include "Device/DeviceBuffer.h"
#include "MixerEngine.h"
#include "Device/Mem/Device_Mem.h"
#include "Transform/BlobTransform.h"
#include "Device/DeviceManager.h"
#include "MixerEngine.h"
#include "Device/Null/Device_Null.h"
#include "Model/Mix/Mix.h"
#include "Blobber.h"

#include "Job/JobBatch.h"

const char* Blob::LODTransformName = "LOD";

#if DEBUG_BLOB_REF_KEEPING == 1
DebugBlobLock::DebugBlobLock()
{
	if (!MixerEngine::Instance() || !MixerEngine::Blobber())
		return;

	MixerEngine::Blobber()->GetDebugBlobMutex()->Lock();
}

DebugBlobLock::~DebugBlobLock()
{
	if (!MixerEngine::Instance() || !MixerEngine::Blobber())
		return;

	MixerEngine::Blobber()->GetDebugBlobMutex()->Unlock();
}
#endif 

//////////////////////////////////////////////////////////////////////////
Blob::Blob() : Buffer(Device_Null::Get()->Create(BufferDescriptor(), nullptr))
{
}

Blob::Blob(DeviceBufferRef InBuffer) 
	: Buffer(InBuffer)
{
}

/// Allocate a NULL device buffer by default
Blob::Blob(const BufferDescriptor& InDesc, CHashPtr InHash) : Buffer(Device_Null::Get()->Create(InDesc, InHash))
{
}

Blob::~Blob()
{
#if DEBUG_BLOB_REF_KEEPING == 1

	//if (!MixerEngine::IsTestMode() && !MixerEngine::IsDestroying())
	//{
	//	DebugBlobLock lock;

	//	if (!_owners.empty())
	//	{
	//		for (size_t i = 0; i < _owners.size(); i++)
	//		{
	//			BlobPtrW tblobW = _owners[i];

	//			if (!tblobW.expired())
	//			{
	//				std::shared_ptr<TiledBlob> tblob = std::static_pointer_cast<TiledBlob>(tblobW.lock());
	//				check(!tblob->HasBlobAsTile(this));
	//			}
	//		}
	//	}
	//}
	//if (MixerEngine::Blobber())
	//{
	//	check(!MixerEngine::Blobber()->IsBlobReferenced(this));
	//}
#endif 
}

#if DEBUG_BLOB_REF_KEEPING == 1
void Blob::AddTiledOwner(BlobPtrW owner)
{
	if (HasOwner(owner))
		return;

	_owners.push_back(owner);
}

void Blob::AddOwner(BlobPtrW owner)
{
	if (HasOwner(owner))
		return;

	_owners.push_back(owner);
}

void Blob::RemoveOwner(BlobPtrW owner)
{
	auto iter = FindOwner(owner);
	if (iter == _owners.end())
		return;

	_owners.erase(iter);
}

void Blob::RemoveOwner(Blob* owner)
{
	auto iter = FindOwner(owner);
	if (iter == _owners.end())
		return;

	_owners.erase(iter);
}

Blob::OwnerList::iterator Blob::FindOwner(BlobPtrW owner) 
{
	return std::find_if(_owners.begin(), _owners.end(), [&owner](const BlobPtrW& rhs) 
	{ 
		return owner.lock() == rhs.lock(); 
	});
}

Blob::OwnerList::iterator Blob::FindOwner(Blob* owner) 
{
	return std::find_if(_owners.begin(), _owners.end(), [&owner](const BlobPtrW& rhs) 
	{ 
		return owner == rhs.lock().get();
	});
}

bool Blob::HasOwner(Blob* owner)
{
	return FindOwner(owner) != _owners.end();
}

bool Blob::HasOwner(BlobPtrW owner) 
{
	return FindOwner(owner) != _owners.end();
}
#endif 

FString Blob::DisplayName() const
{
	if (!Buffer)
		return TEXT("");
	return Buffer->GetName();
}

void Blob::Touch(uint64 BatchId)
{
	// Just update the access info for the time being
	if (Buffer)
	{
		Buffer->UpdateAccessInfo(BatchId);

		// Don't touch the buffer here. This will be done in the blobber idle loop 
		// Buffer->Touch(BatchId);
		MixerEngine::GetBlobber()->Touch(this);
	}
}

void Blob::UpdateAccessInfo(uint64 batchId)
{
	if (Buffer)
		Buffer->UpdateAccessInfo(batchId);
}

DeviceBufferRef Blob::GetBufferRef() const
{ 
	return Buffer; 
}

void Blob::SetHash(CHashPtr Hash)
{
	if (Buffer)
		Buffer->SetHash(Hash);
}

AsyncDeviceBufferRef Blob::TransferTo(Device* TargetDevice)
{
	/// Must have a valid buffer here
	check(Buffer && Buffer->IsValid());

	DeviceBufferRef ExistingBuffer = Buffer;
	return TargetDevice->Transfer(Buffer).then([this, TargetDevice, ExistingBuffer](DeviceBufferRef NewBuffer) mutable 
	{
		check(NewBuffer);
		check(NewBuffer != ExistingBuffer);

		/// At this point, the old buffer can deallocate
		Buffer = NewBuffer;

		return Buffer;
	});
}

AsyncBufferResultPtr Blob::Bind(const BlobTransform* Transform, const ResourceBindInfo& BindInfo)
{
	/// Ok, we must have a raw buffer over here to transfer over to the device buffer
	//check(_buffer);

	Device* Dev = BindInfo.Dev;
	if (!Dev)
		Dev = Buffer->GetOwnerDevice();

	check(Dev);

	/// Touch this buffer if we're not tiled ... TiledBlob must touch its own
	/// buffers because it involves touching all the tiles as well ... which may 
	/// be un-necessary in a lot of cases
	if (!IsTiled())
		Touch(BindInfo.BatchId);

	/// If the buffer isn't compatible then we transfer it over to the other new device
	if (!Buffer->IsCompatible(Dev))
	{
		return Dev->Transfer(Buffer).then([this, Transform, BindInfo](DeviceBufferRef result)
		{
			Buffer = result;
			return Buffer->Bind(Transform, BindInfo);
		});
	}

	/// Ok now we can actually bind this
	return Buffer->Bind(Transform, BindInfo);
}

AsyncBufferResultPtr Blob::Unbind(const BlobTransform* Transform, const ResourceBindInfo& BindInfo)
{
	//check(_buffer);
	return Buffer->Unbind(Transform, BindInfo);
}

bool Blob::IsValid() const
{
	return Buffer->IsValid();
}

AsyncRawBufferPtr Blob::Raw()
{
	/// Could possibly be going to another thread
	/// Save the current temp hash
	CHashPtr prevHash = Buffer->Hash(false);
	check(!prevHash || prevHash->IsTemp());

	return Buffer->Raw().then([this, prevHash](RawBufferPtr raw)
	{
		const BufferDescriptor& desc = Buffer->Descriptor();
		CHashPtr hash = Buffer->Hash(false);

		if (!desc.bIsTransient && hash->IsFinal() && (!prevHash || !prevHash->IsFinal()))
			Buffer = Buffer->GetOwnerDevice()->AddInternal(Buffer);
		else
		{
			HashType prevHashValue = prevHash ? prevHash->Value() : DataUtil::GNullHash;
			UE_LOG(LogDevice, VeryVerbose, TEXT("DeviceBuffer has a new hash without owning reference. Unless this is manually cached by the device, this buffer will be deleted which is undesirable. Name: %s, Hash: %llu [Prev Hash: %llu, Size: %dx%d]"),
				*desc.Name, prevHashValue, hash->Value(), desc.Width, desc.Height);
		}

		/// TODO: do we really need this?
		if (prevHash != nullptr)
			MixerEngine::GetBlobber()->UpdateHash(prevHash->Value(), hash);

		return Buffer->Raw_Now();
	});
}

AscynCHashPtr Blob::CalcHash()
{
	/// If we already have a have a hash for the buffer then don't bother
	if (Buffer)
	{
		CHashPtr bufferHash = Buffer->Hash(false);
		if (bufferHash && bufferHash->IsFinal())
			return cti::make_ready_continuable(bufferHash);
	}

	return Raw().then([this] 
	{ 
		return Buffer->Hash(false);
	});
}

bool Blob::IsNull() const
{
	return Buffer->IsNull();
}

CHashPtr Blob::Hash() const
{
	/// Use own address as hash. This will still ensure that we detect object re-use
	/// even if they're late bound or haven't been calculated yet
	return Buffer->Hash(false);
}

void Blob::OnFinaliseInternal(BlobReadyCallback Callback) const
{
	Callback(this);
}

AsyncBlobResultPtr Blob::OnFinalise() const
{
	/// If already finalised then just return a fulfilled promise
	if (IsFinalised())
		return cti::make_ready_continuable(this);

	return cti::make_continuable<const Blob*>([this](auto&& Promise) 
	{
		TSharedPtr<cti::promise<const Blob*>> PromisePtr = MakeShared<cti::promise<const Blob*>>(std::move(Promise));
		OnFinaliseInternal([this, PromisePtr](const Blob* BlobObj) mutable
		{
			PromisePtr->set_value(BlobObj);
		});
	});
}

AsyncBufferResultPtr Blob::Flush(const ResourceBindInfo& BindInfo)
{
	/// We've got nothing to 
	check(Buffer);
	return Buffer->Flush(BindInfo);
}

void Blob::ResetBuffer()
{
	if (Buffer)
	{
		/// Release the native buffer information
		Buffer->ReleaseNative();

		Buffer = Device_Null::Get()->Create(Buffer->Descriptor(), nullptr);
		UE_LOG(LogData, Log, TEXT("Resetting buffer: %s"), *DisplayName());
	}
	else
		Buffer = Device_Null::Get()->Create(BufferDescriptor(), nullptr);
}

AsyncPrepareResult Blob::PrepareForWrite(const ResourceBindInfo& BindInfo)
{
	check(!bIsFinalised || BindInfo.bIsCombined);
	check(Buffer);

	if(BindInfo.bIsCombined)
		Buffer->Desc = GetDescriptor();

	return Buffer->PrepareForWrite(BindInfo);
}

bool Blob::HasMinMax() const
{
	return (MinValue && MaxValue) || (MinMax != nullptr);
}

float Blob::GetMinValue() const
{
	check(MinValue);
	return *MinValue;
}

float Blob::GetMaxValue() const
{
	check(MaxValue);
	return *MaxValue;
}

BlobPtr Blob::GetMinMaxBlob()
{
	check(MinMax);
	return MinMax;
}

void Blob::SetMinMax(BlobPtr InMinMax)
{
	MinMax = InMinMax;

	InMinMax->OnFinalise()
		.then([this](const Blob* FinalisedBlob)
		{
			check(FinalisedBlob);
			return MinMax->Raw();
		})
		.then([this](RawBufferPtr Raw)
		{
			check(Raw);
			const float* data = reinterpret_cast<const float*>(Raw->GetData());

			check(data);
			check(Raw->GetDescriptor().Width == 1 && Raw->GetDescriptor().Height == 1);

			MinValue = std::make_shared<float>(data[0]);
			MaxValue = std::make_shared<float>(data[1]);

			/// We don't need to keep this anymore
			MinMax = nullptr;
		});
}

int32 Blob::NumLODLevels() const
{
	return (int32)LODLevels.size();
}

bool Blob::HasLODLevels() const
{
	return LODLevels.size() > 0;
}

bool Blob::HasLODLevel(int32 Index) const
{
	return Index >= 0 && Index <= (int32)LODLevels.size() && !LODLevels[Index - 1].expired();
}

BlobPtrW Blob::GetLODLevel(int32 Level)
{
	check(Level > 0 && Level <= (int32)LODLevels.size());
	return LODLevels[Level - 1];
}

void Blob::SetLODLevel(int32 Level, BlobPtr LODBlob, BlobPtrW LODParentBlob, BlobPtrW LODSourceBlob, bool bAddToBlobber)
{
	check(LODBlob);
	check(!LODParentBlob.expired());
	check(!LODSourceBlob.expired());

	LODParent = LODParentBlob;
	LODSource = LODSourceBlob;

	LODBlob->bIsLODLevel = true;

	if (LODLevels.empty())
	{
		/// -1 because we don't wanna keep a weak pointer of Mip Level 0. 'this' is supposed to 
		/// the the mip Level 0, so there's no point keeping it in the mip chain
		int32 numLevels = TextureHelper::CalcNumMipLevels(std::max(GetWidth(), GetHeight())) - 1;

		if (numLevels > 0)
			LODLevels.resize(numLevels);
	}

	check(Level > 0 && Level <= (int32)LODLevels.size());
	check(LODBlob->GetWidth() < GetWidth() && LODBlob->GetHeight() < GetHeight());

	/// Make sure we're not replacing an existing lod Level
	//check(_lodLevels[Level - 1].expired() || (!_lodLevels[Level - 1].expired() && _lodLevels[Level - 1].lock() == blob));

	LODLevels[Level - 1] = LODBlob;

	BlobPtr ThisBlob = shared_from_this();

	/// Add hashes to the blobber
	if (bAddToBlobber)
	{
#if 0 /// TODO
		CHashPtr lodHash = Blob::CalculateMipHash(Hash(), Level);
		MixerEngine::Blobber()->AddResult(lodHash, blob);

		CHashPtr lodHash_0 = Blob::CalculateMipHash(Hash(), 0);
		MixerEngine::Blobber()->AddResult(lodHash_0, thisBlob);
#endif /// 

		//CHashPtrVec thisLodHashes = Blob::CalculateMipHashes(lodSource.lock()->Hash(), lodParent.lock()->Hash(), 0);
		//for (CHashPtr& lodHash : thisLodHashes)
		//{
		//	MixerEngine::Blobber()->AddResult(lodHash, thisBlob);
		//}
	}

	for (int32 li = 0; li < Level - 1; li++)
	{
		BlobPtr lodLevel = LODLevels[li].lock();

		if (lodLevel)
		{
			lodLevel->SetLODLevel(Level - li - 1, LODBlob, ThisBlob, LODSourceBlob, bAddToBlobber);
		}
	}
}

void Blob::Finalise_Now(bool bNoCalcHash, CHashPtr FixedHash)
{
	/// If already finalised then nothing to do over here
	if (bIsFinalised)
		return;

	bIsFinalised = true;
	FinaliseTS = FDateTime::Now();
}

AsyncBufferResultPtr Blob::Finalise(bool bNoCalcHash, CHashPtr FixedHash)
{
	Finalise_Now(bNoCalcHash, FixedHash);
	return cti::make_ready_continuable(std::make_shared<BufferResult>());
}

CHashPtr Blob::CalculateMipHash(CHashPtr MainHash, int32 Level)
{
	CHashPtrVec Sources =
	{
		MainHash,
		std::make_shared<CHash>(DataUtil::Hash_Int32(Level), true),
		std::make_shared<CHash>(DataUtil::Hash_GenericString_Name(FString(LODTransformName)), true)
	};

	return CHash::ConstructFromSources(Sources);
}

CHashPtrVec Blob::CalculateMipHashes(CHashPtr MainHash, CHashPtr ParentHash, int32 Level)
{
	CHashPtr MainHashAtLOD = CalculateMipHash(MainHash, Level);
	CHashPtr ParentHashAtLOD = CalculateMipHash(ParentHash, Level);

	/// Just return one unique hash
	if (MainHashAtLOD->Value() != ParentHashAtLOD->Value())
		return { MainHashAtLOD, ParentHashAtLOD };

	return { MainHashAtLOD };
}

//////////////////////////////////////////////////////////////////////////
