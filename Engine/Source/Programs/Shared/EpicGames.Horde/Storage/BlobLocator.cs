// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Identifier for a blob within a particular namespace
	/// </summary>
	[JsonSchemaString]
	[TypeConverter(typeof(BlobLocatorTypeConverter))]
	[JsonConverter(typeof(BlobLocatorJsonConverter))]
	public readonly struct BlobLocator : IEquatable<BlobLocator>
	{
		readonly string _path;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="path">Path to the blob. The meaning of this string is implementation defined.</param>
		public BlobLocator(string path) => _path = path;

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is BlobLocator blobId && Equals(blobId);

		/// <inheritdoc/>
		public override int GetHashCode() => (_path ?? String.Empty).GetHashCode(StringComparison.Ordinal);

		/// <inheritdoc/>
		public bool Equals(BlobLocator other) => String.Equals(_path, other._path, StringComparison.Ordinal);

		/// <inheritdoc/>
		public override string ToString() => _path ?? String.Empty;

		/// <inheritdoc/>
		public static bool operator ==(BlobLocator left, BlobLocator right) => String.Equals(left._path, right._path, StringComparison.Ordinal);

		/// <inheritdoc/>
		public static bool operator !=(BlobLocator left, BlobLocator right) => !(left == right);
	}

	/// <summary>
	/// Type converter from strings to PropertyFilter objects
	/// </summary>
	sealed class BlobLocatorTypeConverter : TypeConverter
	{
		/// <inheritdoc/>
		public override bool CanConvertFrom(ITypeDescriptorContext? context, Type sourceType)
		{
			return sourceType == typeof(string);
		}

		/// <inheritdoc/>
		public override object ConvertFrom(ITypeDescriptorContext? context, CultureInfo? culture, object value)
		{
			return new BlobLocator((string)value);
		}

		/// <inheritdoc/>
		public override bool CanConvertTo(ITypeDescriptorContext? context, Type? destinationType)
		{
			return destinationType == typeof(string);
		}

		/// <inheritdoc/>
		public override object? ConvertTo(ITypeDescriptorContext? context, CultureInfo? culture, object? value, Type destinationType)
		{
			if (destinationType == typeof(string))
			{
				return value?.ToString();
			}
			else
			{
				return null;
			}
		}
	}

	/// <summary>
	/// Class which serializes AgentId objects to JSON
	/// </summary>
	public sealed class BlobLocatorJsonConverter : JsonConverter<BlobLocator>
	{
		/// <inheritdoc/>
		public override BlobLocator Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options) => new BlobLocator(reader.GetString() ?? String.Empty);

		/// <inheritdoc/>
		public override void Write(Utf8JsonWriter writer, BlobLocator value, JsonSerializerOptions options) => writer.WriteStringValue(value.ToString());
	}
}
