// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Serialization;

namespace Horde.Server.Ddc
{
	/// <summary>
	/// Identifier for a blob in the store
	/// </summary>
	[TypeConverter(typeof(BlobIdTypeConverter))]
	[JsonConverter(typeof(BlobIdJsonConverter))]
	[CbConverter(typeof(BlobIdCbConverter))]
	public readonly struct BlobId : IEquatable<BlobId>
	{
		/// <summary>
		/// Hash of the blob
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BlobId(IoHash hash) => Hash = hash;

		/// <summary>
		/// Parses a BlobId from a string
		/// </summary>
		public static BlobId Parse(string text) => new BlobId(IoHash.Parse(text));

		/// <summary>
		/// Creates a BlobId from a block of data
		/// </summary>
		public static BlobId FromBlob(ReadOnlySpan<byte> span) => new BlobId(IoHash.Compute(span));

		/// <inheritdoc/>
		public override readonly int GetHashCode() => Hash.GetHashCode();

		/// <inheritdoc/>
		public readonly bool Equals(BlobId other) => Hash.Equals(other.Hash);

		/// <inheritdoc/>
		public override readonly bool Equals(object? obj) => obj is BlobId blobId && Equals(blobId);

		/// <inheritdoc/>
		public override readonly string ToString() => Hash.ToString();

		/// <inheritdoc cref="IoHash.op_Equality"/>
		public static bool operator ==(BlobId left, BlobId right) => left.Hash == right.Hash;

		/// <inheritdoc cref="IoHash.op_Inequality"/>
		public static bool operator !=(BlobId left, BlobId right) => !(left == right);

		/// <summary>
		/// Constructs a BlobId from a hash
		/// </summary>
		public static BlobId FromIoHash(IoHash hash) => new BlobId(hash);

		/// <summary>
		/// Creates a BlobId from a data stream
		/// </summary>
		public static async Task<BlobId> FromStreamAsync(Stream stream, CancellationToken cancellationToken) => FromIoHash(await IoHash.ComputeAsync(stream, cancellationToken));

		/// <summary>
		/// Converts a BlobId to an IoHash
		/// </summary>
		public IoHash AsIoHash() => Hash;
	}

	/// <summary>
	/// Converts from <see cref="BlobId"/> instances to other types
	/// </summary>
	public class BlobIdTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			if (sourceType == typeof(string))
			{
				return true;
			}
			return base.CanConvertFrom(context, sourceType);
		}

		/// <inheritdoc/>
		public override object? ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object value)
		{
			if (value is string s)
			{
				return BlobId.Parse(s);
			}

			return base.ConvertFrom(context, culture, value);
		}

		/// <inheritdoc/>
		public override bool CanConvertTo(ITypeDescriptorContext? context, Type? destinationType)
		{
			if (destinationType == typeof(string))
			{
				return true;
			}
			return base.CanConvertTo(context, destinationType);
		}

		/// <inheritdoc/>
		public override object? ConvertTo(ITypeDescriptorContext? context, CultureInfo? culture, object? value, Type destinationType)
		{
			if (destinationType == typeof(string))
			{
				return value?.ToString();
			}

			return base.ConvertTo(context, culture, value, destinationType);
		}
	}

	/// <summary>
	/// Converts from <see cref="BlobId"/> instances to other types
	/// </summary>
	public class BlobIdJsonConverter : JsonConverter<BlobId>
	{
		/// <inheritdoc/>
		public override BlobId Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
		{
			string? str = reader.GetString();
			if (str == null)
			{
				throw new InvalidDataException("Unable to parse blob identifier");
			}

			return BlobId.Parse(str);
		}

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, BlobId value, JsonSerializerOptions options)
		{
			writer.WriteStringValue(value.ToString());
		}
	}

	/// <summary>
	/// Serializes <see cref="BlobId"/> instances to compact binary
	/// </summary>
	public class BlobIdCbConverter : CbConverter<BlobId>
	{
		/// <inheritdoc/>
		public override BlobId Read(CbField field) => new BlobId(field.AsHash());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, BlobId value) => writer.WriteBinaryAttachmentValue(value.Hash);

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, CbFieldName name, BlobId value) => writer.WriteBinaryAttachment(name, value.Hash);
	}
}
