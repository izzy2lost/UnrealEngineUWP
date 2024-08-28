// Copyright Epic Games, Inc. All Rights Reserved.

namespace HordeTrayApp
{
	enum TrayAppPluginState
	{
		Undefined,
		Ok,
		Busy,
		Paused,
		Error
	}

	/// <summary>
	/// Reported status of a plugin
	/// </summary>
	record class TrayAppPluginStatus(TrayAppPluginState State, string? Message);

	/// <summary>
	/// Plugin for the tray app
	/// </summary>
	interface ITrayAppPlugin : IAsyncDisposable
	{
		/// <summary>
		/// Get the current status of this plugin
		/// </summary>
		TrayAppPluginStatus GetStatus();

		/// <summary>
		/// Allow the plugin to customize the tray icon context menu
		/// </summary>
		void PopulateMenu(ContextMenuStrip contextMenu);

		/// <summary>
		/// Update the menu state before it's shown
		/// </summary>
		void UpdateMenu();
	}

	/// <summary>
	/// Plugin for the tray app
	/// </summary>
	abstract class TrayAppPluginBase : ITrayAppPlugin
	{
		/// <inheritdoc/>
		public virtual TrayAppPluginStatus GetStatus()
		{
			return new TrayAppPluginStatus(TrayAppPluginState.Undefined, null);
		}

		/// <inheritdoc/>
		public virtual void PopulateMenu(ContextMenuStrip contextMenu)
		{
		}

		/// <inheritdoc/>
		public virtual void UpdateMenu()
		{
		}

		/// <inheritdoc/>
		public virtual ValueTask DisposeAsync()
		{
			return default;
		}
	}
}
