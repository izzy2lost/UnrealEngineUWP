// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace Horde.Server.Configuration
{
	using JsonObject = System.Text.Json.Nodes.JsonObject;

	/// <summary>
	/// Attribute used to mark <see cref="Uri"/> properties that include other config files
	/// </summary>
	[AttributeUsage(AttributeTargets.Property)]
	public sealed class ConfigIncludeAttribute : Attribute
	{
	}

	/// <summary>
	/// Specifies that a class is the root for including other files
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class ConfigIncludeRootAttribute : Attribute
	{
	}

	/// <summary>
	/// Attribute used to mark <see cref="Uri"/> properties that are relative to their containing file
	/// </summary>
	[AttributeUsage(AttributeTargets.Property)]
	public sealed class ConfigRelativePathAttribute : Attribute
	{
	}

	/// <summary>
	/// Attribute used to mark <see cref="Uri"/> properties that are relative to their containing file
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public sealed class ConfigMacroScopeAttribute : Attribute
	{
	}

	/// <summary>
	/// Declares a config macro
	/// </summary>
	public class ConfigMacro
	{
		/// <summary>
		/// Name of the macro property
		/// </summary>
		public string Name { get; set; } = String.Empty;

		/// <summary>
		/// Value for the macro property
		/// </summary>
		public string Value { get; set; } = String.Empty;
	}

	/// <summary>
	/// Exception thrown when reading config files
	/// </summary>
	public sealed class ConfigException : Exception
	{
		readonly ConfigContext _context;

		/// <summary>
		/// Stack of properties
		/// </summary>
		public IEnumerable<string> ScopeStack => _context.ScopeStack;

		/// <summary>
		/// Stack of objects
		/// </summary>
		public IEnumerable<object> IncludeContextStack => _context.IncludeContextStack;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="context">Current parse context for the error</param>
		/// <param name="message">Description of the error</param>
		/// <param name="innerException">Inner exception details</param>
		internal ConfigException(ConfigContext context, string message, Exception? innerException = null)
			: base(message, innerException)
		{
			_context = context;
		}

		/// <summary>
		/// Gets the parser context when this exception was thrown. This is not exposed as a public property to avoid serializing the whole thing to Serilog.
		/// </summary>
		internal ConfigContext GetContext() => _context;
	}

	/// <summary>
	/// Base class for types that can be read from config files
	/// </summary>
	static class ConfigType
	{
		/// <summary>
		/// Preprocess a JSON node
		/// </summary>
		/// <param name="configType"></param>
		/// <param name="node">Node to preprocess</param>
		/// <param name="context">Context for the preprocessor</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Preprocessed node</returns>
		public static async ValueTask<JsonNode?> PreprocessAsync(this ConfigNode configType, JsonNode? node, ConfigContext context, CancellationToken cancellationToken)
		{
			return await configType.PreprocessAsync(node, null, context, cancellationToken);
		}

		/// <summary>
		/// Reads an object from a particular URL
		/// </summary>
		/// <typeparam name="T">Type of object to read</typeparam>
		/// <param name="uri">Location of the file to read</param>
		/// <param name="context">Context for reading</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public static async Task<T> ReadAsync<T>(Uri uri, ConfigContext context, CancellationToken cancellationToken) where T : class, new()
		{
			IConfigFile file = await context.ReadFileAsync(uri, cancellationToken);
			JsonObject obj = await file.ParseFileAsync(context, cancellationToken);

			context.IncludeStack.Push(file);

			ObjectConfigNode type = new ObjectConfigNode(typeof(T));
			obj = (JsonObject)(await type.PreprocessAsync(obj, null, context, cancellationToken))!;

			context.IncludeStack.Pop();

			return JsonSerializer.Deserialize<T>(obj, context.JsonOptions) ?? new T();
		}

		/// <summary>
		/// Combine a relative path with a base URI to produce a new URI
		/// </summary>
		/// <param name="baseUri">Base uri to rebase relative to</param>
		/// <param name="path">Relative path</param>
		/// <returns>Absolute URI</returns>
		public static Uri CombinePaths(Uri baseUri, string path)
		{
			if (path.StartsWith("//", StringComparison.Ordinal))
			{
				if (baseUri.Scheme == PerforceConfigSource.Scheme)
				{
					return new Uri($"{PerforceConfigSource.Scheme}://{baseUri.Host}{path}");
				}
				else
				{
					return new Uri($"{PerforceConfigSource.Scheme}://{PerforceConnectionSettings.Default}{path}");
				}
			}
			return new Uri(baseUri, path);
		}

		/// <summary>
		/// Helper method to expand all macros within a node without performing any other processing on it
		/// </summary>
		internal static JsonNode? ExpandMacros(JsonNode? node, ConfigContext context)
		{
			if (node == null)
			{
				return node;
			}
			else if (node is JsonObject obj)
			{
				JsonObject result = new JsonObject();
				foreach ((string propertyName, JsonNode? propertyNode) in obj)
				{
					result[propertyName] = ExpandMacros(propertyNode, context);
				}
				return result;
			}
			else if (node is JsonArray arr)
			{
				JsonArray result = new JsonArray();
				foreach (JsonNode? elementNode in arr)
				{
					result.Add(ExpandMacros(elementNode, context));
				}
				return result;
			}
			else if (node is JsonValue val && val.GetValueKind() == JsonValueKind.String)
			{
				string strValue = ((string?)val)!;
				string expandedStrValue = context.ExpandMacros(strValue);
				return JsonValue.Create(expandedStrValue);
			}
			else
			{
				return node.DeepClone();
			}
		}
	}

	/// <summary>
	/// Node in the preprocessor parse tree
	/// </summary>
	abstract class ConfigNode
	{
		/// <summary>
		/// Parses macro definitions from this object
		/// </summary>
		/// <param name="node">Node to parse macros from</param>
		/// <param name="context">Context for the preprocessor</param>
		/// <param name="macros">Macros parsed from the boject</param>
		public abstract void ParseMacros(JsonNode? node, ConfigContext context, Dictionary<string, string> macros);

		/// <summary>
		/// Reads a node into a target object (possibly merging with an existing property)
		/// </summary>
		/// <param name="node">Node to preprocess</param>
		/// <param name="existingNode">Optional existing node to merge with. Can be modified.</param>
		/// <param name="context">Context for the preprocessor</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The preprocessed node. May be existingNode if changes are merged with it.</returns>
		public abstract Task<JsonNode?> PreprocessAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken);

		/// <summary>
		/// Traverse the property tree starting with 'node' and process any include directives, merging the results into the given target object.
		/// </summary>
		/// <param name="node"></param>
		/// <param name="includes">Receives the included files</param>
		/// <param name="context"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public abstract Task ParseIncludesAsync(JsonNode? node, List<IConfigFile> includes, ConfigContext context, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Implementation of <see cref="ConfigType"/> for scalar types
	/// </summary>
	class ScalarConfigNode : ConfigNode
	{
		public bool Include { get; set; }
		public bool RelativeToContainingFile { get; set; }

		public ScalarConfigNode(bool include = false, bool relativePath = false)
		{
			Include = include;
			RelativeToContainingFile = relativePath;
		}

		/// <inheritdoc/>
		public override void ParseMacros(JsonNode? jsonNode, ConfigContext context, Dictionary<string, string> macros)
		{ }

		/// <inheritdoc/>
		public override Task<JsonNode?> PreprocessAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
		{
			JsonNode? result;
			if (node == null)
			{
				result = null;
			}
			else if (RelativeToContainingFile && node is JsonValue value && value.GetValueKind() == JsonValueKind.String)
			{
				result = JsonValue.Create(ConfigType.CombinePaths(context.CurrentFile, value.ToString()).AbsoluteUri);
			}
			else
			{
				result = ConfigType.ExpandMacros(node, context);
			}

			return Task.FromResult(result);
		}

		/// <inheritdoc/>
		public override async Task ParseIncludesAsync(JsonNode? node, List<IConfigFile> includes, ConfigContext context, CancellationToken cancellationToken)
		{
			if (Include)
			{
				string? path = (string?)node;

				Uri uri = ConfigType.CombinePaths(context.CurrentFile, context.ExpandMacros(path!));

				IConfigFile file = await context.ReadFileAsync(uri, cancellationToken);
				includes.Add(file);
			}
		}
	}

	/// <summary>
	/// Property containing a binary resource
	/// </summary>
	class ResourceConfigNode : ConfigNode
	{
		/// <inheritdoc/>
		public override Task ParseIncludesAsync(JsonNode? node, List<IConfigFile> includes, ConfigContext context, CancellationToken cancellationToken)
			=> Task.CompletedTask;

		/// <inheritdoc/>
		public override void ParseMacros(JsonNode? node, ConfigContext context, Dictionary<string, string> macros)
		{ }

		/// <inheritdoc/>
		public override async Task<JsonNode?> PreprocessAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
		{
			Uri uri = ConfigType.CombinePaths(context.CurrentFile, JsonSerializer.Deserialize<string>(node, context.JsonOptions) ?? String.Empty);
			IConfigFile file = await context.ReadFileAsync(uri, cancellationToken);

			ConfigResource resource = new ConfigResource();
			resource.Path = uri.AbsoluteUri;
			resource.Data = await file.ReadAsync(cancellationToken);

			return JsonSerializer.SerializeToNode(resource, context.JsonOptions);
		}
	}

	/// <summary>
	/// Array of Json values
	/// </summary>
	class ArrayConfigNode : ConfigNode
	{
		public ConfigNode ElementType { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public ArrayConfigNode(ConfigNode elementType)
		{
			ElementType = elementType;
		}

		/// <inheritdoc/>
		public override void ParseMacros(JsonNode? node, ConfigContext context, Dictionary<string, string> macros)
		{
			if (node is JsonArray arrayNode)
			{
				foreach (JsonNode? elementNode in arrayNode)
				{
					ElementType.ParseMacros(elementNode, context, macros);
				}
			}
		}

		/// <inheritdoc/>
		public override async Task<JsonNode?> PreprocessAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
		{
			JsonArray targetArray = ((JsonArray?)existingNode) ?? new JsonArray();
			foreach (JsonNode? element in (JsonArray)node!)
			{
				context.EnterScope($"[{targetArray.Count}]");

				JsonNode? elementValue = await ElementType.PreprocessAsync(element, context, cancellationToken);
				targetArray.Add(elementValue);

				context.LeaveScope();
			}
			return targetArray;
		}

		/// <inheritdoc/>
		public override async Task ParseIncludesAsync(JsonNode? node, List<IConfigFile> includes, ConfigContext context, CancellationToken cancellationToken)
		{
			if (node is JsonArray arrayNode)
			{
				ObjectConfigNode classElementType = (ObjectConfigNode)ElementType;
				foreach (JsonObject? element in arrayNode)
				{
					await classElementType.ParseIncludesAsync(element, includes, context, cancellationToken);
				}
			}
		}
	}

	/// <summary>
	/// Arbitary mapping of string values to keys
	/// </summary>
	class DictionaryConfigNode : ConfigNode
	{
		public ConfigNode ElementType { get; }

		public DictionaryConfigNode(ConfigNode elementType)
		{
			ElementType = elementType;
		}

		/// <inheritdoc/>
		public override void ParseMacros(JsonNode? jsonNode, ConfigContext context, Dictionary<string, string> macros) 
		{ }

		/// <inheritdoc/>
		public override async Task<JsonNode?> PreprocessAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
		{
			JsonObject? targetObject = ((JsonObject?)existingNode) ?? new JsonObject();
			foreach ((string key, JsonNode? element) in (JsonObject)node!)
			{
				context.EnterScope($"[{key}]");

				JsonNode? elementValue = await ElementType.PreprocessAsync(element, context, cancellationToken);
				targetObject[key] = elementValue;

				context.LeaveScope();
			}
			return targetObject;
		}

		/// <inheritdoc/>
		public override async Task ParseIncludesAsync(JsonNode? node, List<IConfigFile> includes, ConfigContext context, CancellationToken cancellationToken)
		{
			if (node is JsonObject obj && ElementType is ObjectConfigNode classElementType)
			{
				foreach ((_, JsonNode? value) in obj)
				{
					await classElementType.ParseIncludesAsync(value, includes, context, cancellationToken);
				}
			}
		}
	}

	/// <summary>
	/// Handles macro objects
	/// </summary>
	class MacroConfigNode : ConfigNode
	{
		public override Task ParseIncludesAsync(JsonNode? node, List<IConfigFile> includes, ConfigContext context, CancellationToken cancellationToken)
			=> Task.CompletedTask;

		public override void ParseMacros(JsonNode? node, ConfigContext context, Dictionary<string, string> macros)
		{
			ConfigMacro? macro = JsonSerializer.Deserialize<ConfigMacro>(node, context.JsonOptions);
			if (macro != null)
			{
				macros.Add(macro.Name, macro.Value);
			}
		}

		public override Task<JsonNode?> PreprocessAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
			=> Task.FromResult<JsonNode?>(node?.DeepClone());
	}

	/// <summary>
	/// Implementation of <see cref="ConfigType"/> to handle class types
	/// </summary>
	class ObjectConfigNode : ConfigNode
	{
		public bool IncludeRoot { get; set; }
		public bool MacroScope { get; set; }
		public Dictionary<string, ConfigNode> Properties { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public ObjectConfigNode(bool isIncludeRoot, bool isMacroScope, IEnumerable<KeyValuePair<string, ConfigNode>> properties)
		{
			IncludeRoot = isIncludeRoot;
			MacroScope = isMacroScope;
			Properties = new Dictionary<string, ConfigNode>(properties, StringComparer.OrdinalIgnoreCase);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="type">Type to construct from</param>
		public ObjectConfigNode(Type type)
		{
			IncludeRoot = type.GetCustomAttribute<ConfigIncludeRootAttribute>() != null;
			MacroScope = type.GetCustomAttribute<ConfigMacroScopeAttribute>() != null;
			Properties = new Dictionary<string, ConfigNode>(StringComparer.OrdinalIgnoreCase);

			// Find all the direct include properties
			PropertyInfo[] propertyInfos = type.GetProperties(BindingFlags.Instance | BindingFlags.Public | BindingFlags.GetProperty);
			foreach (PropertyInfo propertyInfo in propertyInfos)
			{
				if (propertyInfo.GetCustomAttribute<JsonIgnoreAttribute>() == null)
				{
					ConfigNode? propertyType = CreateTypeForProperty(propertyInfo);
					if (propertyType != null)
					{
						string name = propertyInfo.GetCustomAttribute<JsonPropertyNameAttribute>()?.Name ?? propertyInfo.Name;
						Properties.Add(name, propertyType);
					}
				}
			}
		}

		bool IsDefault()
			=> !IncludeRoot && !MacroScope && Properties.Count == 0;

		static ConfigNode? CreateTypeForProperty(PropertyInfo propertyInfo)
		{
			Type propertyType = propertyInfo.PropertyType;
			if (!propertyType.IsClass || propertyType == typeof(string))
			{
				bool include = propertyInfo.GetCustomAttribute<ConfigIncludeAttribute>() != null;
				bool relativePath = propertyInfo.GetCustomAttribute<ConfigRelativePathAttribute>() != null;
				if (include || relativePath)
				{
					return new ScalarConfigNode(include, relativePath);
				}
				else
				{
					return null;
				}
			}
			return CreateType(propertyType);
		}

		public static ConfigNode? CreateType(Type type)
		{
			ConfigNode? value;
			if (!type.IsClass || type == typeof(string) || type == typeof(JsonNode))
			{
				value = null;
			}
			else if (type == typeof(ConfigMacro))
			{
				value = new MacroConfigNode();
			}
			else if (type.IsAssignableTo(typeof(ConfigResource)))
			{
				value = new ResourceConfigNode();
			}
			else if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(List<>))
			{
				ConfigNode? elementType = CreateType(type.GetGenericArguments()[0]);
				value = (elementType == null) ? null : new ArrayConfigNode(elementType);
			}
			else if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(Dictionary<,>))
			{
				ConfigNode? elementType = CreateType(type.GetGenericArguments()[1]);
				value = (elementType == null) ? null : new DictionaryConfigNode(elementType);
			}
			else
			{
				ObjectConfigNode objValue = new ObjectConfigNode(type);
				value = objValue.IsDefault() ? null : objValue;
			}
			return value;
		}

		public async ValueTask<JsonObject> ReadAsync(JsonObject obj, ConfigContext context, CancellationToken cancellationToken)
		{
			return (JsonObject)(await PreprocessAsync(obj, null, context, cancellationToken))!;
		}

		/// <inheritdoc/>
		public override void ParseMacros(JsonNode? jsonNode, ConfigContext context, Dictionary<string, string> macros)
		{
			if (jsonNode is JsonObject jsonObject)
			{
				foreach ((string name, JsonNode? node) in jsonObject)
				{
					if (node != null && Properties.TryGetValue(name, out ConfigNode? property))
					{
						property.ParseMacros(node, context, macros);
					}
				}
			}
		}

		/// <inheritdoc/>
		public override async Task<JsonNode?> PreprocessAsync(JsonNode? node, JsonNode? existingNode, ConfigContext context, CancellationToken cancellationToken)
		{
			JsonObject? obj = (JsonObject?)node;
			JsonObject? target = (JsonObject?)existingNode;
			if (obj == null)
			{
				return target;
			}

			// Before parsing properties into this object, read all the includes recursively
			if (IncludeRoot)
			{
				// Find all the includes
				List<IConfigFile> includes = new List<IConfigFile>();
				await ParseIncludesInternalAsync(obj, includes, context, cancellationToken);

				// Find all the files, merge them into target
				foreach (IConfigFile include in includes)
				{
					context.IncludeStack.Push(include);

					JsonObject includedJsonObject = await include.ParseFileAsync(context, cancellationToken);
					target = (JsonObject?)await PreprocessAsync(includedJsonObject, target, context, cancellationToken);

					context.IncludeStack.Pop();
				}
			}

			// Ensure that the target object is valid so we can write properties into it
			target ??= new JsonObject();

			// Parse all the macros for this scope
			if (MacroScope)
			{
				Dictionary<string, string> macros = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
				ParseMacros(obj, context, macros);
				context.MacroScopes.Add(macros);
			}

			// Parse all the properties into this object
			foreach ((string name, JsonNode? newNode) in obj)
			{
				if (newNode is JsonValue)
				{
					context.AddProperty(name);
				}

				if (Properties.TryGetValue(name, out ConfigNode? property))
				{
					context.EnterScope(name);
					target[name] = await property.PreprocessAsync(newNode, target[name], context, cancellationToken);
					context.LeaveScope();
				}
				else
				{
					target[name] = Merge(ConfigType.ExpandMacros(newNode, context), target[name]);
				}
			}

			// Parse all the macros for this scope
			if (MacroScope)
			{
				context.MacroScopes.RemoveAt(context.MacroScopes.Count - 1);
			}

			return target;
		}

		static JsonNode? Merge(JsonNode? source, JsonNode? target)
		{
			if (source is JsonObject sourceObj && target is JsonObject targetObj)
			{
				foreach ((string name, JsonNode? node) in sourceObj)
				{
					targetObj[name] = Merge(node, targetObj[name]);
				}
				return target;
			}
			else if (source is JsonArray sourceArr && target is JsonArray targetArr)
			{
				foreach (JsonNode? node in sourceArr)
				{
					targetArr.Add(node?.DeepClone());
				}
				return target;
			}
			return source;
		}

		/// <inheritdoc/>
		public override async Task ParseIncludesAsync(JsonNode? node, List<IConfigFile> includes, ConfigContext context, CancellationToken cancellationToken)
		{
			if (!IncludeRoot && node is JsonObject obj)
			{
				await ParseIncludesInternalAsync(obj, includes, context, cancellationToken);
			}
		}

		async Task ParseIncludesInternalAsync(JsonObject obj, List<IConfigFile> includes, ConfigContext context, CancellationToken cancellationToken)
		{
			foreach ((string name, JsonNode? propertyNode) in obj)
			{
				if (Properties.TryGetValue(name, out ConfigNode? property))
				{
					await property.ParseIncludesAsync(propertyNode, includes, context, cancellationToken);
				}
			}
		}
	}
}
