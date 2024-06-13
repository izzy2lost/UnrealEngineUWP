// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text.Json;

namespace EpicGames.Core
{
	/// <summary>
	/// Utility functions for serializing using the JsonSerializer, to replace BinaryFormatterUtils.
	/// </summary>
	public static class JsonSerializerUtils
	{
		static JsonSerializerOptions Options => new JsonSerializerOptions() { IncludeFields = true };

		/// <summary>
		/// Load an object from a file on disk, using the binary formatter
		/// </summary>
		/// <param name="location">File to read from</param>
		/// <returns>Instance of the object that was read from disk</returns>
		public static T Load<T>(FileReference location)
		{
			using FileStream stream = new FileStream(location.FullName, FileMode.Open, FileAccess.Read);
			JsonSerializerOptions options = new JsonSerializerOptions();
			return JsonSerializer.Deserialize<T>(stream, Options)
				?? throw new TypeLoadException($"Type {typeof(T)} cannot be loaded from {location}");
		}

		/// <summary>
		/// Saves a file to disk, using the binary formatter
		/// </summary>
		/// <param name="location">File to write to</param>
		/// <param name="obj">Object to serialize</param>
		public static void Save<T>(FileReference location, T obj)
		{
			DirectoryReference.CreateDirectory(location.Directory);
			using FileStream stream = new FileStream(location.FullName, FileMode.Create, FileAccess.Write);
			JsonSerializer.Serialize<T>(stream, obj, Options);
		}

		/// <summary>
		/// Saves a file to disk using the binary formatter, without updating the timestamp if it hasn't changed
		/// </summary>
		/// <param name="location">File to write to</param>
		/// <param name="obj">Object to serialize</param>
		public static void SaveIfDifferent<T>(FileReference location, T obj)
		{
			byte[] contents;
			using (MemoryStream stream = new MemoryStream())
			{
				JsonSerializer.Serialize(stream, obj, Options);
				contents = stream.ToArray();
			}

			DirectoryReference.CreateDirectory(location.Directory);
			FileReference.WriteAllBytesIfDifferent(location, contents);
		}
	}
}
