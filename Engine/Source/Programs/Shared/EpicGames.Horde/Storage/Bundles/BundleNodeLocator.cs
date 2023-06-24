// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.Serialization;

namespace EpicGames.Horde.Storage.Bundles
{
	/// <summary>
	/// Locates a node in storage
	/// </summary>
	[JsonConverter(typeof(BundleNodeLocatorJsonConverter))]
	[TypeConverter(typeof(BundleNodeLocatorTypeConverter))]
	[CbConverter(typeof(BundleNodeLocatorCbConverter))]
	public struct BundleNodeLocator : IEquatable<BundleNodeLocator>
	{
		/// <summary>
		/// Hash of the referenced node
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Location of the blob containing this node
		/// </summary>
		public BundleLocator Blob { get; }

		/// <summary>
		/// Index of the export within the blob
		/// </summary>
		public int ExportIdx { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleNodeLocator(IoHash hash, BundleLocator blob, int exportIdx)
		{
			Hash = hash;
			Blob = blob;
			ExportIdx = exportIdx;
		}

		/// <summary>
		/// Determines if this locator points to a valid entry
		/// </summary>
		public bool IsValid() => Blob.IsValid();

		/// <summary>
		/// Parse a string as a node locator
		/// </summary>
		/// <param name="text">Text to parse</param>
		/// <returns></returns>
		public static BundleNodeLocator Parse(ReadOnlySpan<char> text)
		{
			int hashLength = IoHash.NumBytes * 2;

			IoHash hash;
			if (text.Length == hashLength && IoHash.TryParse(text, out hash))
			{
				return new BundleNodeLocator(hash, default, 0);
			}

			if (text[hashLength] == '@')
			{
				hash = IoHash.Parse(text.Slice(0, hashLength));
				text = text.Slice(hashLength + 1);
			}
			else
			{
				// For legacy locators that don't have a hash, hash the path instead. This is obviously incorrect, but we never validate hashes and will serve as a stable id during migration.
				hash = IoHash.Compute(Encoding.UTF8.GetBytes(text.ToString()));
			}

			int hashIdx = text.IndexOf('#');
			if (hashIdx == -1)
			{
				throw new ArgumentException("Invalid node locator", nameof(text));
			}

			int exportIdx = Int32.Parse(text.Slice(hashIdx + 1), NumberStyles.None, CultureInfo.InvariantCulture);
			BundleLocator blobLocator = new BundleLocator(new Utf8String(text.Slice(0, hashIdx)));
			return new BundleNodeLocator(hash, blobLocator, exportIdx);
		}

		/// <inheritdoc/>
		public override bool Equals([NotNullWhen(true)] object? obj) => obj is BundleNodeLocator locator && Equals(locator);

		/// <inheritdoc/>
		public bool Equals(BundleNodeLocator other) => Blob == other.Blob && ExportIdx == other.ExportIdx;

		/// <inheritdoc/>
		public override int GetHashCode() => HashCode.Combine(Blob, ExportIdx);

		/// <inheritdoc/>
		public override string ToString() => $"{Hash}@{Blob}#{ExportIdx}";

		/// <inheritdoc/>
		public static bool operator ==(BundleNodeLocator left, BundleNodeLocator right) => left.Equals(right);

		/// <inheritdoc/>
		public static bool operator !=(BundleNodeLocator left, BundleNodeLocator right) => !left.Equals(right);
	}

	/// <summary>
	/// Type converter for <see cref="BundleNodeLocator"/> to and from JSON
	/// </summary>
	sealed class BundleNodeLocatorJsonConverter : JsonConverter<BundleNodeLocator>
	{
		/// <inheritdoc/>
		public override BundleNodeLocator Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => BundleNodeLocator.Parse(reader.GetString() ?? String.Empty);

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, BundleNodeLocator value, JsonSerializerOptions options) => writer.WriteStringValue(value.ToString());
	}

	/// <summary>
	/// Type converter from strings to <see cref="BundleNodeLocator"/> objects
	/// </summary>
	sealed class BundleNodeLocatorTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object? value)
		{
			return BundleNodeLocator.Parse((string)value!);
		}
	}

	/// <summary>
	/// Type converter to compact binary
	/// </summary>
	sealed class BundleNodeLocatorCbConverter : CbConverterBase<BundleNodeLocator>
	{
		/// <inheritdoc/>
		public override BundleNodeLocator Read(CbField field) => BundleNodeLocator.Parse(field.AsString());

		/// <inheritdoc/>
		public override void Write(CbWriter writer, BundleNodeLocator value) => writer.WriteUtf8StringValue(value.ToString());

		/// <inheritdoc/>
		public override void WriteNamed(CbWriter writer, Utf8String name, BundleNodeLocator value) => writer.WriteUtf8String(name, value.ToString());
	}

	/// <summary>
	/// Extension methods for node locators
	/// </summary>
	public static class NodeLocatorExtensions
	{
		/// <summary>
		/// Deserialize a node locator
		/// </summary>
		/// <param name="reader">Reader to deserialize from</param>
		/// <returns>The node id that was read</returns>
		public static BundleNodeLocator ReadNodeLocator(this IMemoryReader reader)
		{
			return BundleNodeLocator.Parse(reader.ReadString());
		}

		/// <summary>
		/// Serialize a node locator
		/// </summary>
		/// <param name="writer">Writer to serialize to</param>
		/// <param name="value">Value to serialize</param>
		public static void WriteNodeLocator(this IMemoryWriter writer, BundleNodeLocator value)
		{
			writer.WriteString(value.ToString());
		}
	}
}
