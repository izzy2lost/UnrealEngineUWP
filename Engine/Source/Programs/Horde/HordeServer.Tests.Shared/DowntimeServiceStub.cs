// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using HordeServer.Server;

namespace HordeServer.Tests
{
	public class DowntimeServiceStub : IDowntimeService
	{
		public DowntimeServiceStub(bool isDowntimeActive = false)
		{
			IsDowntimeActive = isDowntimeActive;
		}

		public bool IsDowntimeActive { get; set; }
	}
}
