// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Api;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Commands
{
	[Command("login", "Logs in to a Horde server")]
	class LoginCommand : Command
	{
		class GetAuthConfigResponse
		{
			public string? Method { get; set; }
			public string? ServerUrl { get; set; }
			public string? ClientId { get; set; }
			public string[]? RedirectUrls { get; set; }
		}

		[CommandLine("-Server=")]
		public string? Server { get; set; }

		readonly IHttpClientFactory _httpClientFactory;
		readonly CmdConfig _config;

		public LoginCommand(IHttpClientFactory httpClientFactory, IOptions<CmdConfig> config)
		{
			_httpClientFactory = httpClientFactory;
			_config = config.Value;
		}

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			if (Server != null)
			{
				_config.Server = new Uri(Server);
				await _config.WriteAsync();
			}

			using HordeHttpClient httpClient = _httpClientFactory.CreateHordeClient();

			GetServerInfoResponse serverInfo = await httpClient.GetServerInfoAsync();
			logger.LogInformation("Connected to server version: {Version}", serverInfo.ServerVersion);
			
			return 0;
		}
	}
}
