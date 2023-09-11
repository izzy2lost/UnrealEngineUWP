// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text.Json;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.OIDC;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;

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

	/// <summary>
	/// Extension methods for <see cref="CmdConfig"/>
	/// </summary>
	static class CmdConfigExtensions
	{
		class GetAuthConfigResponse
		{
			public string? Method { get; set; }
			public string? ServerUrl { get; set; }
			public string? ClientId { get; set; }
			public string[]? LocalRedirectUrls { get; set; }
		}

		/// <summary>
		/// Get an access token for the server specified in a config instance
		/// </summary>
		public static async Task<string?> GetAccessTokenAsync(this CmdConfig config, ILogger logger, CancellationToken cancellationToken = default)
		{
			if (config.Server == null)
			{
				throw new InvalidOperationException("No Horde server is configured. Run 'horde login -server=...' to configure.");
			}

			logger.LogInformation("Getting access token for {Server}", config.Server);

			GetAuthConfigResponse authConfig;
			using (HttpClient httpClient = new HttpClient())
			{
				Uri uri = new Uri(config.Server, "api/v1/server/auth");
				authConfig = await httpClient.GetAsync<GetAuthConfigResponse>(uri, cancellationToken);
			}

			if (String.Equals(authConfig.Method, "Anonymous", StringComparison.OrdinalIgnoreCase))
			{
				return null;
			}

			const string OidcProvider = "Horde";

			Dictionary<string, string?> values = new Dictionary<string, string?>();
			values[$"Providers:{OidcProvider}:DisplayName"] = "Horde";
			values[$"Providers:{OidcProvider}:ServerUri"] = authConfig.ServerUrl;
			values[$"Providers:{OidcProvider}:ClientId"] = authConfig.ClientId;
			values[$"Providers:{OidcProvider}:RedirectUri"] = authConfig.LocalRedirectUrls?.FirstOrDefault();

			ConfigurationBuilder builder = new ConfigurationBuilder();
			builder.AddInMemoryCollection(values);

			IConfiguration configuration = builder.Build();

			using ITokenStore tokenStore = TokenStoreFactory.CreateTokenStore();
			OidcTokenManager oidcTokenManager = OidcTokenManager.CreateTokenManager(configuration, tokenStore, new List<string>() { OidcProvider });

			OidcTokenInfo? result = null;
			try
			{
				result = await oidcTokenManager.TryGetAccessToken(OidcProvider, cancellationToken);
			}
			catch (Exception ex)
			{
				logger.LogTrace(ex, "Unable to get access token; attempting login: {Message}", ex.Message);
			}
			if (result == null)
			{
				result = await oidcTokenManager.Login(OidcProvider, cancellationToken);
			}

			if (result.AccessToken == null)
			{
				throw new Exception($"Unable to get access token for {config.Server}");
			}

			logger.LogInformation("Received bearer token for {Server}", config.Server);
			return result.AccessToken;
		}

		/// <summary>
		/// Creates an HTTP client for connecting to the configured Horde server
		/// </summary>
		public static async Task<HordeHttpClient> GetHttpClientAsync(this CmdConfig config, ILogger logger, CancellationToken cancellationToken = default)
		{
			Uri? server = config.Server;
			if (server == null)
			{
				throw new Exception("No server is configured. Run 'horde login -server=...' to set up.");
			}

			string? token = await config.GetAccessTokenAsync(logger, cancellationToken);
			if (token == null)
			{
				throw new Exception("Unable to log in to server.");
			}

			return new HordeHttpClient(server, token, logger);
		}
	}
}
