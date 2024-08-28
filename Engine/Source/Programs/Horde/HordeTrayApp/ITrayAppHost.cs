// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HordeTrayApp
{
	/// <summary>
	/// Interface for the tray app host application
	/// </summary>
	interface ITrayAppHost
	{
		/// <summary>
		/// Notifies the host that a status change has ocurred
		/// </summary>
		void UpdateStatus();
	}
}
