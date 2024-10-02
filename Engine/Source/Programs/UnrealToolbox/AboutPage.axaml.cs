// Copyright Epic Games, Inc. All Rights Reserved.

using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Microsoft.Extensions.DependencyInjection;

namespace UnrealToolbox
{
	partial class AboutPage : UserControl
	{
		public AboutPage()
		{
			InitializeComponent();

			string version = Program.Update?.CurrentVersionString ?? "No version information present.";
			_versionText.Text = version;
		}
	}
}