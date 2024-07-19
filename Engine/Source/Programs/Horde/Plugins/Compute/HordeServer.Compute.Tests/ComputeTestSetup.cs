// Copyright Epic Games, Inc. All Rights Reserved.

using HordeServer.Acls;
using HordeServer.Compute;
using HordeServer.Logs;
using HordeServer.Storage;
using Microsoft.Extensions.DependencyInjection;

namespace HordeServer.Tests
{
	public class ComputeTestSetup : ServerTestSetup
	{
		public ComputeService ComputeService => ServiceProvider.GetRequiredService<ComputeService>();
		public StorageService StorageService => ServiceProvider.GetRequiredService<StorageService>();
		public ILogCollection LogCollection => ServiceProvider.GetRequiredService<ILogCollection>();

		public ComputeTestSetup()
		{
			AddPlugin<StoragePlugin>();
			AddPlugin<ComputePlugin>();
		}

		protected override void ConfigureServices(IServiceCollection services)
		{
			base.ConfigureServices(services);

			services.AddSingleton<IDefaultAclModifier, ComputeAclModifier>();
		}
	}
}
