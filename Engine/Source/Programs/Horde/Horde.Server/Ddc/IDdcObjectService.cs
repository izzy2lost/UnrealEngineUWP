// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;

#pragma warning disable CS1591

namespace Horde.Server.Ddc
{
	public interface IDdcObjectService
    {
        Task<(ObjectRecord, BlobContents?)> Get(NamespaceId ns, BucketId bucket, IoHash key, string[] fields, bool doLastAccessTracking = true);
        Task<(ContentId[], BlobIdentifier[])> Put(NamespaceId ns, BucketId bucket, IoHash key, BlobIdentifier blobHash, CbObject payload);
        Task<(ContentId[], BlobIdentifier[])> Finalize(NamespaceId ns, BucketId bucket, IoHash key, BlobIdentifier blobHash);

        IAsyncEnumerable<NamespaceId> GetNamespaces();

        Task<bool> Delete(NamespaceId ns, BucketId bucket, IoHash key);
        Task<long> DropNamespace(NamespaceId ns);
        Task<long> DeleteBucket(NamespaceId ns, BucketId bucket);

        Task<bool> Exists(NamespaceId ns, BucketId bucket, IoHash key);
        Task<List<BlobIdentifier>> GetReferencedBlobs(NamespaceId ns, BucketId bucket, IoHash key);
    }

	public class ObjectRecord
	{
		public ObjectRecord(NamespaceId ns, BucketId bucket, IoHash name, DateTime lastAccess, byte[]? inlinePayload, BlobIdentifier blobIdentifier, bool isFinalized)
		{
			Namespace = ns;
			Bucket = bucket;
			Name = name;
			LastAccess = lastAccess;
			InlinePayload = inlinePayload;
			BlobIdentifier = blobIdentifier;
			IsFinalized = isFinalized;
		}

		public NamespaceId Namespace { get; }
		public BucketId Bucket { get; }
		public IoHash Name { get; }
		public DateTime LastAccess { get; }
		public byte[]? InlinePayload { get; set; }
		public BlobIdentifier BlobIdentifier { get; set; }
		public bool IsFinalized { get; }
	}

	public class ObjectNotFoundException : Exception
	{
		public ObjectNotFoundException(NamespaceId ns, BucketId bucket, IoHash key) : base($"Object not found {key} in bucket {bucket} namespace {ns}")
		{
			Namespace = ns;
			Bucket = bucket;
			Key = key;
		}

		public NamespaceId Namespace { get; }
		public BucketId Bucket { get; }
		public IoHash Key { get; }
	}

	public class ObjectHashMismatchException : Exception
    {
        public ObjectHashMismatchException(NamespaceId ns, BucketId bucket, IoHash name, BlobIdentifier suppliedHash, BlobIdentifier actualHash) : base($"Object {name} in bucket {bucket} and namespace {ns} did not reference hash {suppliedHash} was referencing {actualHash}")
        {
        }
    }
}
