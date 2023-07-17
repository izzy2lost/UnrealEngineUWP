// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;

#pragma warning disable CS1591

namespace Horde.Server.Ddc
{
	public interface IRefService
    {
        Task<(RefRecord, BlobContents?)> GetAsync(NamespaceId ns, BucketId bucket, RefId key, string[] fields, bool doLastAccessTracking = true, CancellationToken cancellationToken = default);
        Task<(ContentId[], BlobId[])> PutAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash, CbObject payload, CancellationToken cancellationToken = default);
        Task<(ContentId[], BlobId[])> FinalizeAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash, CancellationToken cancellationToken = default);

        Task<bool> DeleteAsync(NamespaceId ns, BucketId bucket, RefId key, CancellationToken cancellationToken = default);

        Task<bool> ExistsAsync(NamespaceId ns, BucketId bucket, RefId key, CancellationToken cancellationToken = default);
        Task<List<BlobId>> GetReferencedBlobsAsync(NamespaceId ns, BucketId bucket, RefId key, CancellationToken cancellationToken = default);
    }

	public class RefRecord
	{
		public RefRecord(NamespaceId ns, BucketId bucket, RefId name, DateTime lastAccess, byte[]? inlinePayload, BlobId blobIdentifier, bool isFinalized)
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
		public RefId Name { get; }
		public DateTime LastAccess { get; }
		public byte[]? InlinePayload { get; set; }
		public BlobId BlobIdentifier { get; set; }
		public bool IsFinalized { get; }
	}

	public class RefNotFoundException : Exception
	{
		public RefNotFoundException(NamespaceId ns, BucketId bucket, RefId key) : base($"Object not found {key} in bucket {bucket} namespace {ns}")
		{
			Namespace = ns;
			Bucket = bucket;
			Key = key;
		}

		public NamespaceId Namespace { get; }
		public BucketId Bucket { get; }
		public RefId Key { get; }
	}

	public class ObjectHashMismatchException : Exception
    {
        public ObjectHashMismatchException(NamespaceId ns, BucketId bucket, IoHash name, BlobId suppliedHash, BlobId actualHash) : base($"Object {name} in bucket {bucket} and namespace {ns} did not reference hash {suppliedHash} was referencing {actualHash}")
        {
        }
    }
}
