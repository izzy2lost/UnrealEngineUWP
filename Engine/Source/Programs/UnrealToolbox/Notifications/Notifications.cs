// Copyright Epic Games, Inc. All Rights Reserved.

using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Platform;
using DesktopNotifications;
using DesktopNotifications.FreeDesktop;
using DesktopNotifications.Windows;
using Microsoft.Extensions.Logging;
using System;
using System.Reflection;
using System.Runtime.InteropServices;
using UnrealToolbox;

namespace UnrealToolbox
{
	/// <summary>
	/// Toolbox notification manager interface
	/// </summary>
	public interface IToolboxNotificationManager
	{
		/// <summary>
		/// Show a notification
		/// </summary>
		/// <param name="title"></param>
		/// <param name="body"></param>
		/// <param name="force"></param>
		void ShowNotification(string title, string body, bool force = false);
	}

	/// <summary>
	/// Toolbox notification manager implementation
	/// </summary>
	class ToolboxNotificationManager : IToolboxNotificationManager, IDisposable
	{

		private INotificationManager? _platformManager;
		readonly ILogger _logger;

		// spam prevention
		DateTime? _lastNotificationTime;
		string? _lastTitle;
		string? _lastBody;

		[DllImport("shell32.dll", SetLastError = true)]
		private static extern void SetCurrentProcessExplicitAppUserModelID([MarshalAs(UnmanagedType.LPWStr)] string appId);

		public ToolboxNotificationManager(ILogger<ToolboxNotificationManager> logger)
		{
			_logger = logger;
		}

		public void Start()
		{
			if (Environment.OSVersion.Platform == PlatformID.Win32NT)
			{
				// WindowsApplicationContext.FromCurrentProcess() has side effects of creating start menu items, and changing the app user model id to the executing assembly, which can be dotnet.exe
				// WindowsApplicationContext context = WindowsApplicationContext.FromCurrentProcess();
				WindowsApplicationContext? context = Activator.CreateInstance(type: typeof(WindowsApplicationContext), bindingAttr: BindingFlags.Instance | BindingFlags.NonPublic, binder: null, args: new object[] { "Unreal Toolbox", "Unreal Toolbox" },culture: null) as WindowsApplicationContext;
				SetCurrentProcessExplicitAppUserModelID("Unreal Toolbox");

				_platformManager = new WindowsNotificationManager(context);
			}
			else
			{
				throw new NotImplementedException();
			}

			// initialize and ensure with result
			_platformManager.Initialize().GetAwaiter().GetResult();

		}

		public void Dispose()
		{
			_platformManager?.Dispose();
		}

		/// <summary>
		/// Show a notification, 
		/// </summary>
		/// <param name="title"></param>
		/// <param name="body"></param>
		/// <param name="force"></param>
		public void ShowNotification(string title, string body, bool force = false)
		{
			// spawn 
			if (!force && _lastNotificationTime != null && !String.IsNullOrEmpty(_lastBody) && !String.IsNullOrEmpty(_lastTitle))
			{
				TimeSpan deltaTime = DateTime.Now - _lastNotificationTime.Value;

				// don't show a new notification if already displayed one in last minute
				if (deltaTime.TotalSeconds < 60)
				{
					return;
				}

				// if the title and body are the same, wait 2 minutes
				if ((deltaTime.TotalSeconds < 120) && title == _lastTitle && body == _lastBody)
				{
					return;
				}
			}

			_lastTitle = title;
			_lastBody = body;
			_lastNotificationTime = DateTime.Now;

			Notification notification = new Notification
			{
				Title = title,
				Body = body
			};

			_platformManager?.ShowNotification(notification, DateTimeOffset.Now + TimeSpan.FromSeconds(30));
		}
	}
}