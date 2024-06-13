// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Core
{
	/// <summary>
	/// Utility functions for serializing using BinaryFormatter, should not be used.
	/// </summary>
	[Obsolete("BinaryFormatter serialization and deserialization are obsolete. See https://aka.ms/binaryformatter for more information.")]
	public static class BinaryFormatterUtils
	{
		/// <summary>
		/// Load an object from a file on disk, using the json serializer
		/// </summary>
		/// <param name="location">File to read from</param>
		/// <returns>Instance of the object that was read from disk</returns>
		public static T Load<T>(FileReference location) => JsonSerializerUtils.Load<T>(location);

		/// <summary>
		/// Saves a file to disk, using the json serializer
		/// </summary>
		/// <param name="location">File to write to</param>
		/// <param name="obj">Object to serialize</param>
		public static void Save(FileReference location, object obj) => JsonSerializerUtils.Save(location, obj);

		/// <summary>
		/// Saves a file to disk using the binary serializer, without updating the timestamp if it hasn't changed
		/// </summary>
		/// <param name="location">File to write to</param>
		/// <param name="obj">Object to serialize</param>
		public static void SaveIfDifferent(FileReference location, object obj) => JsonSerializerUtils.SaveIfDifferent(location, obj);
	}
}
