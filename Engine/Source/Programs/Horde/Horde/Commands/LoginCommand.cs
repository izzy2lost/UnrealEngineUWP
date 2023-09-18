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

		readonly CmdConfig _config;
		readonly IServiceProvider _serviceProvider;

		public LoginCommand(IOptions<CmdConfig> config, IServiceProvider serviceProvider)
		{
			_config = config.Value;
			_serviceProvider = serviceProvider;
		}

		/// <inheritdoc/>
		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			if (Server != null)
			{
				_config.Server = new Uri(Server);
				await _config.WriteAsync();
			}

			HordeHttpClient httpClient = _serviceProvider.GetRequiredService<HordeHttpClient>();
			GetServerInfoResponse serverInfo = await httpClient.GetAsync<GetServerInfoResponse>("api/v1/server/info");
			logger.LogInformation("Connected to server version: {Version}", serverInfo.ServerVersion);
			
			return 0;
		}
	}
}
