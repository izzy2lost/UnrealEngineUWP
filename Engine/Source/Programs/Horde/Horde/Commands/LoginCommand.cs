// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
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

		public LoginCommand(IOptions<CmdConfig> config)
		{
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

			if (await _config.GetAccessTokenAsync(logger) == null)
			{
				logger.LogError("Unable to log in to server");
				return 1;
			}

			return 0;
		}
	}
}
