// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Path to an object within an <see cref="IObjectStore"/>. Object keys are normalized to lowercase, and may consist of the characters [a-z0-9_./].
	/// </summary>
	[JsonSchemaString]
	public readonly struct ObjectKey : IEquatable<ObjectKey>
	{
		readonly Utf8String _path;

		/// <summary>
		/// Accessor for the internal path string
		/// </summary>
		public Utf8String Path => _path;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="path">Path to the blob. The meaning of this string is implementation defined.</param>
		public ObjectKey(Utf8String path)
		{
			_path = path;

			if (path.Length > 0)
			{
				if (path[0] == '/' || path[^1] == '/')
				{
					throw new FormatException($"Object locator '{path}' is invalid; locators may not start or end with a slash");
				}
				for (int idx = 0; idx < path.Length; idx++)
				{
					if (path[idx] == '/' && path[idx - 1] == '/')
					{
						throw new FormatException($"Object locator '{path}' is invalid; locators may not contain double consecutive slashes");
					}
					if (!IsValidChar(path[idx]))
					{
						throw new FormatException($"Object locator '{path}' is invalid; character '{(char)path[idx]}' is not allowed");
					}
				}
			}
		}

		static bool IsValidChar(byte character)
			=> (character >= 'a' && character <= 'z') || (character >=  '0' && character <= '9') || character == '_' || character == '.' || character == '/';

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="path">Path to the blob. The meaning of this string is implementation defined.</param>
		public ObjectKey(string path) => _path = new Utf8String(path);

		/// <summary>
		/// Whether the blob locator is valid
		/// </summary>
		public bool IsValid() => !_path.IsEmpty;

		/// <summary>
		/// Checks whether this blob is within the given folder
		/// </summary>
		/// <param name="folder">Name of the folder</param>
		/// <returns>True if the the blob id is within the given folder</returns>
		public bool WithinFolder(ObjectKey folder)
		{
			Utf8String path = Path;
			Utf8String folderPath = folder.Path;
			return path.Length > folderPath.Length && path.StartsWith(folderPath) && path[folderPath.Length] == '/';
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is BlobLocator blobId && Equals(blobId);

		/// <inheritdoc/>
		public override int GetHashCode() => _path.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(ObjectKey other) => _path == other._path;

		/// <inheritdoc/>
		public override string ToString() => _path.ToString();

		/// <inheritdoc/>
		public static bool operator ==(ObjectKey left, ObjectKey right) => left.Path == right.Path;

		/// <inheritdoc/>
		public static bool operator !=(ObjectKey left, ObjectKey right) => !(left == right);
	}
}
