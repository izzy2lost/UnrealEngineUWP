// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Xml.Linq;

namespace PerfReportTool
{

	class OptionalString
	{
		public OptionalString(string valueIn)
		{
			value = valueIn;
			isSet = true;
		}
		public OptionalString()
		{
			isSet = false;
		}
		public OptionalString(XElement element, string Name, bool IsElement = false, XmlVariableMappings vars = null )
		{
			isSet = false;
			if (IsElement)
			{
				XElement child = element.Element(Name);
				if (child != null)
				{
					value = child.GetValue(vars);
					isSet = true;
				}
			}
			else
			{
				XAttribute child = element.Attribute(Name);
				if (child != null)
				{
					value = element.GetRequiredAttribute<string>(vars, Name);
					isSet = true;
				}
			}
		}

		public void InheritFrom(OptionalString baseVersion) { if (!isSet) { isSet = baseVersion.isSet; value = baseVersion.value; } }
		public bool isSet;
		public string value;
	};

	class Optional<T>
	{
		public Optional(T valueIn)
		{
			value = valueIn;
			isSet = true;
		}
		public Optional()
		{
			isSet = false;
		}
		public Optional(XElement element, string AttributeName, XmlVariableMappings vars = null)
		{
			isSet = false;
			try
			{
				if (element.Attribute(AttributeName) != null)
				{
					value = element.GetRequiredAttribute<T>(vars, AttributeName);
					isSet = true;
				}
			}
			catch { }
		}
		public void InheritFrom(Optional<T> baseVersion) { if (!isSet) { isSet = baseVersion.isSet; value = baseVersion.value; } }

		public T value;
		public bool isSet;
	};


	static class OptionalHelper
	{
		public static string GetDoubleSetting(Optional<double> setting, string cmdline)
		{
			return (setting.isSet ? (cmdline + setting.value.ToString()) : "");
		}

		public static string GetStringSetting(OptionalString setting, string cmdline)
		{
			return (setting.isSet ? (cmdline + setting.value) : "");
		}
	};

	class ReportGraph
	{
		public ReportGraph(XElement element, XmlVariableMappings vars)
		{
			title = element.GetRequiredAttribute<string>(vars, "title");
			budget = new Optional<double>(element, "budget", vars);
			inSummary = element.GetSafeAttribute<bool>(vars, "inSummary", false);
			isExternal = element.GetSafeAttribute<bool>(vars, "external", false);

			minFilterStatValue = new Optional<double>(element, "minFilterStatValue", vars);
		}
		public string title;
		public Optional<double> budget;
		public bool inSummary;
		public bool isExternal;
		public Optional<double> minFilterStatValue;
		public GraphSettings settings;
	};

	class GraphSettings
	{
		public GraphSettings(XElement element)
		{
			smooth = new Optional<bool>(element, "smooth");
			thickness = new Optional<double>(element, "thickness");
			miny = new Optional<double>(element, "miny");
			maxy = new Optional<double>(element, "maxy");
			maxAutoMaxY = new Optional<double>(element, "maxAutoMaxY");
			threshold = new Optional<double>(element, "threshold");
			averageThreshold = new Optional<double>(element, "averageThreshold");
			minFilterStatValue = new Optional<double>(element, "minFilterStatValue");
			minFilterStatName = new OptionalString(element, "minFilterStatName");
			smoothKernelPercent = new Optional<double>(element, "smoothKernelPercent");
			smoothKernelSize = new Optional<double>(element, "smoothKernelSize");
			compression = new Optional<double>(element, "compression");
			width = new Optional<int>(element, "width");
			height = new Optional<int>(element, "height");
			stacked = new Optional<bool>(element, "stacked");
			showAverages = new Optional<bool>(element, "showAverages");
			filterOutZeros = new Optional<bool>(element, "filterOutZeros");
			maxHierarchyDepth = new Optional<int>(element, "maxHierarchyDepth");
			hideStatPrefix = new OptionalString(element, "hideStatPrefix");
			mainStat = new OptionalString(element, "mainStat");
			showEvents = new OptionalString(element, "showEvents");
			requiresDetailedStats = new Optional<bool>(element, "requiresDetailedStats");
			ignoreStats = new OptionalString(element, "ignoreStats");

			statString = new OptionalString(element, "statString", true);
			//additionalArgs = new OptionalString(element, "additionalArgs", true);
			statMultiplier = new	(element, "statMultiplier");
			legendAverageThreshold = new Optional<double>(element, "legendAverageThreshold");
			snapToPeaks = new Optional<bool>(element, "snapToPeaks");
			lineDecimalPlaces = new Optional<int>(element, "lineDecimalPlaces");
		}
		public void InheritFrom(GraphSettings baseSettings)
		{
			smooth.InheritFrom(baseSettings.smooth);
			statString.InheritFrom(baseSettings.statString);
			thickness.InheritFrom(baseSettings.thickness);
			miny.InheritFrom(baseSettings.miny);
			maxy.InheritFrom(baseSettings.maxy);
			maxAutoMaxY.InheritFrom(baseSettings.maxAutoMaxY);
			threshold.InheritFrom(baseSettings.threshold);
			averageThreshold.InheritFrom(baseSettings.averageThreshold);
			minFilterStatValue.InheritFrom(baseSettings.minFilterStatValue);
			minFilterStatName.InheritFrom(baseSettings.minFilterStatName);
			smoothKernelSize.InheritFrom(baseSettings.smoothKernelSize);
			smoothKernelPercent.InheritFrom(baseSettings.smoothKernelPercent);
			compression.InheritFrom(baseSettings.compression);
			width.InheritFrom(baseSettings.width);
			height.InheritFrom(baseSettings.height);
			//additionalArgs.InheritFrom(baseSettings.additionalArgs);
			stacked.InheritFrom(baseSettings.stacked);
			showAverages.InheritFrom(baseSettings.showAverages);
			filterOutZeros.InheritFrom(baseSettings.filterOutZeros);
			maxHierarchyDepth.InheritFrom(baseSettings.maxHierarchyDepth);
			hideStatPrefix.InheritFrom(baseSettings.hideStatPrefix);
			mainStat.InheritFrom(baseSettings.mainStat);
			showEvents.InheritFrom(baseSettings.showEvents);
			requiresDetailedStats.InheritFrom(baseSettings.requiresDetailedStats);
			statMultiplier.InheritFrom(baseSettings.statMultiplier);
			ignoreStats.InheritFrom(baseSettings.ignoreStats);
			legendAverageThreshold.InheritFrom(baseSettings.legendAverageThreshold);
			snapToPeaks.InheritFrom(baseSettings.snapToPeaks);
			lineDecimalPlaces.InheritFrom(baseSettings.lineDecimalPlaces);

		}
		public Optional<bool> smooth;
		public OptionalString statString;
		public Optional<double> thickness;
		public Optional<double> miny;
		public Optional<double> maxy;
		public Optional<double> maxAutoMaxY;
		public Optional<double> threshold;
		public Optional<double> averageThreshold;
		public Optional<double> minFilterStatValue;
		public OptionalString minFilterStatName;
		public Optional<double> smoothKernelSize;
		public Optional<double> smoothKernelPercent;
		public Optional<double> compression;
		public Optional<int> width;
		public Optional<int> height;
		//public OptionalString additionalArgs;
		public Optional<bool> stacked;
		public Optional<bool> showAverages;
		public Optional<bool> filterOutZeros;
		public Optional<int> maxHierarchyDepth;
		public OptionalString hideStatPrefix;
		public OptionalString mainStat;
		public OptionalString showEvents;
		public OptionalString ignoreStats;
		public Optional<double> statMultiplier;
		public Optional<double> legendAverageThreshold;

		public Optional<bool> requiresDetailedStats;
		public Optional<bool> snapToPeaks;
		public Optional<int> lineDecimalPlaces;

	};

}