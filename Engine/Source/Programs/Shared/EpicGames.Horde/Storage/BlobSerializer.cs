// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Serialization;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Handles serialization of blobs using <see cref="BlobConverter"/> instances.
	/// </summary>
	public static class BlobSerializer
	{
		/// <summary>
		/// Deserialize an object
		/// </summary>
		/// <typeparam name="T">Return type for deserialization</typeparam>
		/// <param name="blobData">Data to deserialize from</param>
		/// <param name="options">Options to control serialization</param>
		public static T Deserialize<T>(BlobData blobData, BlobSerializerOptions? options = null)
		{
			options ??= BlobSerializerOptions.Default;
			BlobReader reader = new BlobReader(blobData);
			return options.GetConverter<T>().Read(reader, options);
		}
		/*
		/// <summary>
		/// Deserialize an object
		/// </summary>
		/// <typeparam name="T">Return type for the deserialized object</typeparam>
		/// <param name="handle">Handle to the blob to deserialize</param>
		/// <param name="options">Options to control serialization</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async ValueTask<T> DeserializeAsync<T>(IBlobHandle handle, BlobSerializerOptions? options = null, CancellationToken cancellationToken = default)
		{
			BlobData data = await handle.ReadAsync(cancellationToken);
			return Deserialize<T>(data, options);
		}

		/// <summary>
		/// Deserialize an object
		/// </summary>
		/// <typeparam name="T">Return type for the deserialized object</typeparam>
		/// <param name="handle">Handle to the blob to deserialize</param>
		/// <param name="options">Options to control serialization</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static ValueTask<T> DeserializeAsync<T>(IBlobHandle<T> handle, BlobSerializerOptions? options = null, CancellationToken cancellationToken = default)
		{
			return DeserializeAsync<T>((IBlobHandle)handle, options, cancellationToken);
		}*/

		/// <summary>
		/// Serialize an object into a blob
		/// </summary>
		/// <typeparam name="T">Type of object to serialize</typeparam>
		/// <param name="writer">Writer for the blob data</param>
		/// <param name="value">Object to serialize</param>
		/// <param name="options">Options to control serialization</param>
		/// <returns>Type of the serialized blob</returns>
		public static BlobType Serialize<T>(IBlobWriter writer, T value, BlobSerializerOptions? options = null)
		{
			options ??= BlobSerializerOptions.Default;
			return options.GetConverter<T>().Write(writer, value, options);
		}
		/*
		/// <summary>
		/// Deserialize an object
		/// </summary>
		/// <param name="writer">Writer for serialized data</param>
		/// <param name="value">The object to serialize</param>
		/// <param name="options">Options to control serialization</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the serialized blob</returns>
		public static async ValueTask<IBlobHandle<T>> SerializeAsync<T>(IStorageWriter writer, T value, BlobSerializerOptions? options = null, CancellationToken cancellationToken = default)
		{
			BlobWriter blobWriter = new BlobWriter(writer);
			BlobType blobType = Serialize<T>(blobWriter, value, options);
			return await writer.WriteBlobAsync<T>(blobType, blobWriter.Length, blobWriter.References, cancellationToken);
		}*/
	}

	/// <summary>
	/// Options for serializing blobs
	/// </summary>
	public class BlobSerializerOptions
	{
		/// <summary>
		/// Default options instance
		/// </summary>
		public static BlobSerializerOptions Default { get; } = new BlobSerializerOptions();

		/// <summary>
		/// Known converter types
		/// </summary>
		public IList<BlobConverter> Converters { get; } = new List<BlobConverter>();

		readonly ConcurrentDictionary<Type, BlobConverter> _cachedConverters = new ConcurrentDictionary<Type, BlobConverter>();
		readonly static ConcurrentDictionary<Type, BlobConverter> s_cachedDefaultConverters = new ConcurrentDictionary<Type, BlobConverter>();
		readonly static Func<Type, BlobConverter> s_createDefaultConverter = CreateDefaultConverter;

		/// <summary>
		/// Gets a converter for the given type
		/// </summary>
		public BlobConverter<T> GetConverter<T>()
		{
			return (BlobConverter<T>)_cachedConverters.GetOrAdd(typeof(T), CreateConverter);
		}

		BlobConverter CreateConverter(Type type)
		{
			foreach (BlobConverter converter in Converters)
			{
				if (converter.CanConvert(type))
				{
					return converter;
				}
			}
			return s_cachedDefaultConverters.GetOrAdd(type, s_createDefaultConverter);
		}

		static BlobConverter CreateDefaultConverter(Type type)
		{
			BlobConverterAttribute? attribute = type.GetCustomAttribute<BlobConverterAttribute>();
			if (attribute != null)
			{
				Type converterType = attribute.ConverterType;
				if (converterType.IsGenericTypeDefinition)
				{
					converterType = converterType.MakeGenericType(type.GetGenericArguments());
				}
				return (BlobConverter)Activator.CreateInstance(converterType)!;
			}
			throw new NotSupportedException($"No converter is available to handle type {type.Name}");
		}
	}

	/// <summary>
	/// Extension methods for serializing blob types
	/// </summary>
	public static class BlobSerializerExtensions
	{
		/// <summary>
		/// Deserialize an object
		/// </summary>
		/// <typeparam name="T">Return type for the deserialized object</typeparam>
		/// <param name="handle">Handle to the blob to deserialize</param>
		/// <param name="options">Options to control serialization</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async ValueTask<T> ReadBlobAsync<T>(this IBlobHandle handle, BlobSerializerOptions? options = null, CancellationToken cancellationToken = default)
		{
			BlobData data = await handle.ReadBlobDataAsync(cancellationToken);
			return BlobSerializer.Deserialize<T>(data, options);
		}

		/// <summary>
		/// Deserialize an object
		/// </summary>
		/// <typeparam name="T">Return type for the deserialized object</typeparam>
		/// <param name="handle">Handle to the blob to deserialize</param>
		/// <param name="options">Options to control serialization</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async ValueTask<T> ReadBlobAsync<T>(this IBlobHandle<T> handle, BlobSerializerOptions? options = null, CancellationToken cancellationToken = default)
		{
			BlobData data = await handle.ReadBlobDataAsync(cancellationToken);
			return BlobSerializer.Deserialize<T>(data, options);
		}

		/// <summary>
		/// Serialize an object to storage
		/// </summary>
		/// <param name="writer">Writer for serialized data</param>
		/// <param name="value">The object to serialize</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the serialized blob</returns>
		public static ValueTask<IBlobHandle<T>> WriteBlobAsync<T>(this IStorageWriter writer, T value, CancellationToken cancellationToken)
		{
			return WriteBlobAsync<T>(writer, value, null, cancellationToken);
		}

		/// <summary>
		/// Serialize an object to storage
		/// </summary>
		/// <param name="writer">Writer for serialized data</param>
		/// <param name="value">The object to serialize</param>
		/// <param name="options">Options to control serialization</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the serialized blob</returns>
		public static ValueTask<IBlobHandle<T>> WriteBlobAsync<T>(this IStorageWriter writer, T value, BlobSerializerOptions? options = null, CancellationToken cancellationToken = default)
		{
			BlobWriter blobWriter = new BlobWriter(writer);
			BlobType blobType = BlobSerializer.Serialize<T>(blobWriter, value, options);
			return writer.WriteBlobAsync<T>(blobType, blobWriter.Length, blobWriter.References, cancellationToken);
		}

		/// <summary>
		/// Reads data for a ref from the store, along with the node's contents.
		/// </summary>
		/// <param name="store">Store instance to write to</param>
		/// <param name="name">The ref name</param>
		/// <param name="cacheTime">Minimum coherency for any cached value to be returned</param>
		/// <param name="options">Options to control serialization</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Node for the given ref, or null if it does not exist</returns>
		public static async Task<TNode?> TryReadRefAsync<TNode>(this IStorageClient store, RefName name, DateTime cacheTime = default, BlobSerializerOptions? options = null, CancellationToken cancellationToken = default) where TNode : class
		{
			IBlobHandle? refTarget = await store.TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			if (refTarget == null)
			{
				return null;
			}

			using BlobData blobData = await refTarget.ReadBlobDataAsync(cancellationToken);
			return BlobSerializer.Deserialize<TNode>(blobData, options);
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">Store instance to write to</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="cacheTime">Minimum coherency of any cached result</param>
		/// <param name="options">Options to control serialization</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The blob instance</returns>
		public static async Task<TNode> ReadRefAsync<TNode>(this IStorageClient store, RefName name, DateTime cacheTime = default, BlobSerializerOptions? options = null, CancellationToken cancellationToken = default) where TNode : class
		{
			TNode? refValue = await store.TryReadRefAsync<TNode>(name, cacheTime, options, cancellationToken);
			if (refValue == null)
			{
				throw new RefNameNotFoundException(name);
			}
			return refValue;
		}
	}
}
