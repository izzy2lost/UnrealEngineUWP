// Copyright Epic Games, Inc. All Rights Reserved.

using Avalonia.Controls;
using Avalonia.Input;
using FluentAvalonia.UI.Controls;
using Microsoft.Extensions.DependencyInjection;

namespace UnrealToolbox
{
	record class SettingsContext(Window? SettingsWindow, IServiceProvider ServiceProvider)
	{
		public static SettingsContext Default { get; } = new SettingsContext(null!, new ServiceCollection().BuildServiceProvider());
	}

	partial class SettingsWindow : Window
	{
		readonly IServiceProvider? _serviceProvider;
		readonly Dictionary<string, ITrayAppPlugin> _typeToPlugin = new Dictionary<string, ITrayAppPlugin>();

		public SettingsWindow()
			: this(null!)
		{ }

		public SettingsWindow(IServiceProvider serviceProvider)
		{
			InitializeComponent();

			_serviceProvider = serviceProvider;

			foreach (ITrayAppPlugin plugin in _serviceProvider.GetServices<ITrayAppPlugin>())
			{
				if(plugin.HasSettingsPage())
				{
					string typeName = plugin.GetType().FullName!;
					_navView.MenuItems.Add(new NavigationViewItem() { Content = plugin.Name, IconSource = plugin.Icon, Tag = typeName });
					_typeToPlugin.Add(typeName, plugin);
				}
			}

			_navView.SelectedItem = _navView.MenuItems[0];
			_navView.SelectionChanged += NavView_SelectionChanged;

			NavView_UpdateContent();
		}

		protected override void OnGotFocus(GotFocusEventArgs e)
		{
			base.OnGotFocus(e);

			if (_serviceProvider != null)
			{
				foreach (ITrayAppPlugin plugin in _serviceProvider.GetServices<ITrayAppPlugin>())
				{
					plugin.Refresh();
				}
			}
		}

		private void NavView_SelectionChanged(object? sender, NavigationViewSelectionChangedEventArgs e)
		{
			if (sender == _navView)
			{
				NavView_UpdateContent();
			}
		}

		private void NavView_UpdateContent()
		{
			if (_navView.SelectedItem is NavigationViewItem nvi)
			{
				SettingsContext context = new SettingsContext(this, _serviceProvider!);

				string? typeName = (string?)nvi.Tag;
				if (typeName != null && _typeToPlugin.TryGetValue(typeName, out ITrayAppPlugin? plugin))
				{
					_navView.Content = plugin.CreateSettingsPage(context);
				}
				else
				{
					_navView.Content = new GeneralSettingsPage(context);
				}
			}
		}
	}
}
