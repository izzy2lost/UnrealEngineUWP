// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.Json;
using EpicGames.Core;

namespace Horde
{
	/// <summary>
	/// User configuration for the command line tool
	/// </summary>
	class CmdConfig
	{
		/// <summary>
		/// Gets the path to this config file
		/// </summary>
		public static FileReference Location { get; } = GetLocation();

		Uri _server = new Uri("http://localhost:5000/");

		/// <summary>
		/// Server to connect to
		/// </summary>
		public Uri Server
		{
			get => _server;
			set
			{
				Uri uri = value;
				if (!uri.OriginalString.EndsWith("/", StringComparison.Ordinal))
				{
					uri = new Uri(uri.OriginalString + "/");
				}
				_server = uri;
			}
		}

		/// <summary>
		/// Settings for the storage cache
		/// </summary>
		public CmdCacheConfig Cache { get; set; } = new CmdCacheConfig();

		/// <summary>
		/// Gets the location for the config file
		/// </summary>
		static FileReference GetLocation()
		{
			DirectoryReference? baseDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.UserProfile);
			baseDir ??= DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData);
			baseDir ??= DirectoryReference.GetCurrentDirectory();
			return FileReference.Combine(baseDir, ".horde.json");
		}

		/// <summary>
		/// Read the current configuration from disk
		/// </summary>
		public static CmdConfig Read()
		{
			CmdConfig? config = null;
			if (FileReference.Exists(Location))
			{
				byte[] data = FileReference.ReadAllBytes(Location);
				config = JsonSerializer.Deserialize<CmdConfig>(data, new JsonSerializerOptions { AllowTrailingCommas = true, PropertyNameCaseInsensitive = true, ReadCommentHandling = JsonCommentHandling.Skip });
			}
			return config ?? new CmdConfig();
		}

		/// <summary>
		/// Saves the current configuration to disk
		/// </summary>
		public async Task WriteAsync(CancellationToken cancellationToken = default)
		{
			byte[] data = JsonSerializer.SerializeToUtf8Bytes(this, new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase, WriteIndented = true });
			DirectoryReference.CreateDirectory(Location.Directory);
			await FileReference.WriteAllBytesAsync(Location, data, cancellationToken);
		}
	}

	/// <summary>
	/// Settings for configuring the storage system
	/// </summary>
	public class CmdCacheConfig
	{
		/// <summary>
		/// Directory to use for the persistent backend cache
		/// </summary>
		public string? CacheDir { get; set; }

		/// <summary>
		/// Size of the disk cache, in megabytes
		/// </summary>
		public long CacheSize { get; set; } = 1024;

		/// <summary>
		/// Size of the in-memory header cache, in megabytes 
		/// </summary>
		public long? HeaderCacheSize { get; set; }

		/// <summary>
		/// Size of the in-memory object cache, in megabytes
		/// </summary>
		public long? PacketCacheSize { get; set; }
	}
}
