// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Extensions.Hosting;

namespace HordeServer.Tests
{
	public class AppLifetimeStub : IHostApplicationLifetime
	{
		public CancellationToken ApplicationStarted { get; }
		public CancellationToken ApplicationStopping { get; }
		public CancellationToken ApplicationStopped { get; }

		public AppLifetimeStub()
		{
			ApplicationStarted = new CancellationToken();
			ApplicationStopping = new CancellationToken();
			ApplicationStopped = new CancellationToken();
		}

		public void StopApplication()
		{
			throw new NotImplementedException();
		}
	}
}
