// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;

namespace Jupiter.Implementation
{
	public interface IReferencesStore
	{
		Task<ObjectRecord> Get(NamespaceId ns, BucketId bucket, RefId key, FieldFlags flags);

		[Flags]
		public enum FieldFlags
		{
			None = 0,
			IncludePayload = 1,
			All = IncludePayload
		}

		Task Put(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash, byte[] blob, bool isFinalized);
		Task Finalize(NamespaceId ns, BucketId bucket, RefId key, BlobId blobIdentifier);

		Task UpdateLastAccessTime(NamespaceId ns, BucketId bucket, RefId key, DateTime newLastAccessTime);
		IAsyncEnumerable<(NamespaceId, BucketId, RefId, DateTime)> GetRecords();

		IAsyncEnumerable<NamespaceId> GetNamespaces();
		Task<bool> Delete(NamespaceId ns, BucketId bucket, RefId key);
		Task<long> DropNamespace(NamespaceId ns);
		Task<long> DeleteBucket(NamespaceId ns, BucketId bucket);
	}

	public class ObjectRecord
	{
		public ObjectRecord(NamespaceId ns, BucketId bucket, RefId name, DateTime lastAccess, byte[]? inlinePayload, BlobId blobIdentifier, bool isFinalized)
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
		public bool IsFinalized {get;}
	}

	public class ObjectNotFoundException : Exception
	{
		public ObjectNotFoundException(NamespaceId ns, BucketId bucket, RefId key) : base($"Object not found {key} in bucket {bucket} namespace {ns}")
		{
			Namespace = ns;
			Bucket = bucket;
			Key = key;
		}

		public NamespaceId Namespace { get; }
		public BucketId Bucket { get; }
		public RefId Key { get; }
	}
}
