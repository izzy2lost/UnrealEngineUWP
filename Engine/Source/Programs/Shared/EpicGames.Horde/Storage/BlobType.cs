// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Binary;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Identifies the type of a blob
	/// </summary>
	/// <param name="Guid">Nominal identifier for the type</param>
	/// <param name="Version">Version number for the serializer</param>
	public record struct BlobType(Guid Guid, int Version)
	{
		/// <summary>
		/// Number of bytes in a serialized blob type instance
		/// </summary>
		public const int NumBytes = 20;

		/// <summary>
		/// Blob type used to indicate a leaf node. Data is an opaque blob and has no references.
		/// </summary>
		public static BlobType Leaf { get; } = new BlobType(Guid.Parse("{1080B643-8015-4A4D-ADBB-C2FB586C8D81}"), 1);

		/// <summary>
		/// Deserialize a type from a byte span
		/// </summary>
		public static BlobType Read(ReadOnlySpan<byte> span)
		{
			Guid guid = new Guid(span.Slice(0, 16));
			int version = BinaryPrimitives.ReadInt32LittleEndian(span.Slice(16));

			return new BlobType(guid, version);
		}

		/// <summary>
		/// Serialize to a byte span
		/// </summary>
		public readonly void Write(Span<byte> data)
		{
			Guid.TryWriteBytes(data.Slice(0, 16));
			BinaryPrimitives.WriteInt32LittleEndian(data.Slice(16), Version);
		}

		/// <inheritdoc/>
		public override readonly string ToString() => $"{Guid},{Version}";
	}
}
