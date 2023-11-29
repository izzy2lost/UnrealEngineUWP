// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace EpicGames.UBA
{
	/// <summary>
	/// Utils
	/// </summary>
	public static class Utils
	{
		/// <summary>
		/// Is UBA available?
		/// </summary>
		public static bool IsAvailable() => s_available.Value;
		static readonly Lazy<bool> s_available = new Lazy<bool>(() => File.Exists(GetLibraryPath()));

		[System.Diagnostics.CodeAnalysis.SuppressMessage("Globalization", "CA1308:Normalize strings to uppercase", Justification = "folder path is lowercase")]
		static string GetLibraryPath()
		{
			string arch = RuntimeInformation.ProcessArchitecture.ToString().ToLowerInvariant();
			string assemblyFolder = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location)!;
			if (OperatingSystem.IsWindows())
			{
				return Path.Combine(assemblyFolder, "runtimes", $"win-{arch}", "native", "UbaHost.dll");
			}
			else if (OperatingSystem.IsLinux())
			{
				return Path.Combine(assemblyFolder, "runtimes", $"linux-{arch}", "native", "libUbaHost.so");
			}
			else if (OperatingSystem.IsMacOS())
			{
				return Path.Combine(assemblyFolder, "runtimes", $"osx", "native", "libUbaHost.dylib");
			}
			throw new PlatformNotSupportedException();
		}
	}
}
