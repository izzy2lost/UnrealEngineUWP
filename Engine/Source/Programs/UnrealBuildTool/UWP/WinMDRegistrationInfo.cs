using System;
using System.Collections.Generic;
using System.Linq;
using Windows.Foundation.Metadata;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Reflection;


namespace UnrealBuildTool
{
	// @ATG_CHANGE : BEGIN winmd type registration support
	public class WinMDRegistrationInfo
	{
		public class ActivatableType
		{
			public ActivatableType(string InTypeName, object InThreadingModel)
			{
				TypeName = InTypeName;
				ThreadingModelName = ((ThreadingModel)InThreadingModel).ToString().ToLowerInvariant();
			}

			public string TypeName { get; private set; }
			public string ThreadingModelName { get; private set; }
		}

		public WinMDRegistrationInfo(FileReference InWindMDSourcePath, string InPackageRelativeDllPath)
		{
			PackageRelativeDllPath = InPackageRelativeDllPath;
            ResolveSearchPaths.Add(InWindMDSourcePath.Directory.FullName);

            ActivatableTypesList = new List<ActivatableType>();
			var DependsOn = Assembly.ReflectionOnlyLoadFrom(InWindMDSourcePath.FullName);
			foreach (var WinMDType in DependsOn.GetExportedTypes())
			{
				bool IsActivatable = false;
				object ThreadingModel = Windows.Foundation.Metadata.ThreadingModel.Both;
				foreach (var Attr in WinMDType.CustomAttributes)
				{
					if (Attr.AttributeType.AssemblyQualifiedName == typeof(ActivatableAttribute).AssemblyQualifiedName ||
                        Attr.AttributeType.AssemblyQualifiedName == typeof(StaticAttribute).AssemblyQualifiedName)
					{
						IsActivatable = true;
					}
					else if (Attr.AttributeType.AssemblyQualifiedName == typeof(ThreadingAttribute).AssemblyQualifiedName)
					{
						ThreadingModel = Attr.ConstructorArguments[0].Value;
					}
				}
				if (IsActivatable)
				{
					ActivatableTypesList.Add(new ActivatableType(WinMDType.FullName, ThreadingModel));
				}
			}
		}

		public string PackageRelativeDllPath { get; private set; }

		public IEnumerable<ActivatableType> ActivatableTypes
		{
			get
			{
				return ActivatableTypesList;
			}
		}

		static WinMDRegistrationInfo()
		{
			AppDomain.CurrentDomain.ReflectionOnlyAssemblyResolve += (Sender, EventArgs) => Assembly.ReflectionOnlyLoad(EventArgs.Name);
			WindowsRuntimeMetadata.ReflectionOnlyNamespaceResolve += (Sender, EventArgs) =>
			{
				string Path = WindowsRuntimeMetadata.ResolveNamespace(EventArgs.NamespaceName, ResolveSearchPaths).FirstOrDefault();
				if (Path == null)
				{
					return;
				}
				EventArgs.ResolvedAssemblies.Add(Assembly.ReflectionOnlyLoadFrom(Path));
			};
		}

        private static List<string> ResolveSearchPaths = new List<string>();
		private List<ActivatableType> ActivatableTypesList;
	}
	// @ATG_CHANGE : END
}
