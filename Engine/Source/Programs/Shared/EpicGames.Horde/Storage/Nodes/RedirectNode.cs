// Copyright Epic Games, Inc. All Rights Reserved.

namespace EpicGames.Horde.Storage.Nodes
{
	/// <summary>
	/// A node containing arbitrary compact binary data
	/// </summary>
	[BlobConverter(typeof(RedirectNodeConverter<>))]
	public class RedirectNode<T>
	{
		/// <summary>
		/// The target handle
		/// </summary>
		public IBlobHandle<T> Target { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="handle">Target node for the redirect</param>
		public RedirectNode(IBlobHandle<T> handle) => Target = handle;
	}

	class RedirectNodeConverter<T> : BlobConverter<RedirectNode<T>>
	{
		BlobType s_blobType = new BlobType("{BE09E54F-47CA-7A6B-2A97-AFBC183B1538}", 1);

		/// <inheritdoc/>
		public override RedirectNode<T> Read(IBlobReader reader, BlobSerializerOptions options)
		{
			IBlobHandle<T> handle = reader.ReadBlobHandle<T>();
			return new RedirectNode<T>(handle);
		}

		/// <inheritdoc/>
		public override BlobType Write(IBlobWriter writer, RedirectNode<T> value, BlobSerializerOptions options)
		{
			writer.WriteBlobHandle<T>(value.Target);
			return s_blobType;
		}
	}
}
