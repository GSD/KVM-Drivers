// AutomationFramework.cs - C# Wrapper for KVM-Drivers Automation Framework
// Provides .NET-friendly API over the C++ automation framework

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace KVMDrivers.Automation
{
    /// <summary>
    /// Main entry point for the KVM-Drivers Automation Framework
    /// </summary>
    public class AutomationFramework : IDisposable
    {
        private IntPtr _handle;
        private bool _disposed;
        private bool _initialized;

        // Events
        public event EventHandler<LogEventArgs> LogReceived;
        public event EventHandler<ProgressEventArgs> ProgressChanged;
        public event EventHandler<TestCompletedEventArgs> TestCompleted;

        /// <summary>
        /// Creates a new instance of the automation framework
        /// </summary>
        public AutomationFramework()
        {
            _handle = NativeMethods.KvmAutomation_Create();
            if (_handle == IntPtr.Zero)
                throw new InvalidOperationException("Failed to create automation framework");
        }

        /// <summary>
        /// Initializes the framework and connects to drivers
        /// </summary>
        public void Initialize()
        {
            ThrowIfDisposed();
            
            int result = NativeMethods.KvmAutomation_Initialize(_handle);
            if (result != 0)
                throw new InvalidOperationException($"Failed to initialize framework (error {result})");
            
            _initialized = true;
        }

        /// <summary>
        /// Loads a test suite from a YAML file
        /// </summary>
        public TestSuite LoadTestSuite(string filePath)
        {
            ThrowIfDisposed();
            ThrowIfNotInitialized();

            IntPtr suiteHandle = NativeMethods.KvmAutomation_LoadTestSuite(_handle, filePath);
            if (suiteHandle == IntPtr.Zero)
                throw new InvalidOperationException($"Failed to load test suite: {filePath}");

            return new TestSuite(suiteHandle);
        }

        /// <summary>
        /// Runs a test suite and returns the result
        /// </summary>
        public TestResult RunTest(TestSuite suite)
        {
            ThrowIfDisposed();
            ThrowIfNotInitialized();

            IntPtr resultHandle = NativeMethods.KvmAutomation_RunTest(_handle, suite.Handle);
            if (resultHandle == IntPtr.Zero)
                throw new InvalidOperationException("Test execution failed");

            var result = new TestResult(resultHandle);
            TestCompleted?.Invoke(this, new TestCompletedEventArgs(result));
            return result;
        }

        /// <summary>
        /// Runs the built-in smoke test
        /// </summary>
        public bool RunSmokeTest()
        {
            ThrowIfDisposed();
            ThrowIfNotInitialized();

            return NativeMethods.KvmAutomation_RunSmokeTest(_handle) == 0;
        }

        /// <summary>
        /// Gets the framework version
        /// </summary>
        public static string Version => NativeMethods.KvmAutomation_GetVersion();

        public void Dispose()
        {
            if (!_disposed)
            {
                if (_initialized)
                {
                    NativeMethods.KvmAutomation_Shutdown(_handle);
                }
                NativeMethods.KvmAutomation_Destroy(_handle);
                _disposed = true;
            }
        }

        private void ThrowIfDisposed()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(AutomationFramework));
        }

        private void ThrowIfNotInitialized()
        {
            if (!_initialized)
                throw new InvalidOperationException("Framework not initialized. Call Initialize() first.");
        }
    }

    /// <summary>
    /// Represents a loaded test suite
    /// </summary>
    public class TestSuite : IDisposable
    {
        internal IntPtr Handle { get; }
        private bool _disposed;

        internal TestSuite(IntPtr handle)
        {
            Handle = handle;
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                NativeMethods.KvmTestSuite_Destroy(Handle);
                _disposed = true;
            }
        }
    }

    /// <summary>
    /// Contains the results of a test run
    /// </summary>
    public class TestResult : IDisposable
    {
        internal IntPtr Handle { get; }
        private bool _disposed;

        internal TestResult(IntPtr handle)
        {
            Handle = handle;
        }

        /// <summary>
        /// Whether the test overall succeeded
        /// </summary>
        public bool Success => NativeMethods.KvmResult_GetSuccess(Handle) != 0;

        /// <summary>
        /// Total number of test steps
        /// </summary>
        public int TotalSteps => NativeMethods.KvmResult_GetTotalSteps(Handle);

        /// <summary>
        /// Number of passed steps
        /// </summary>
        public int PassedSteps => NativeMethods.KvmResult_GetPassedSteps(Handle);

        /// <summary>
        /// Number of failed steps
        /// </summary>
        public int FailedSteps => NativeMethods.KvmResult_GetFailedSteps(Handle);

        /// <summary>
        /// Saves the result to a file
        /// </summary>
        public void SaveToFile(string filePath, ResultFormat format = ResultFormat.Json)
        {
            string formatStr = format switch
            {
                ResultFormat.Json => "json",
                ResultFormat.Xml => "xml",
                ResultFormat.Markdown => "md",
                _ => "json"
            };

            NativeMethods.KvmResult_SaveToFile(Handle, filePath, formatStr);
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                NativeMethods.KvmResult_Destroy(Handle);
                _disposed = true;
            }
        }
    }

    /// <summary>
    /// Result format options
    /// </summary>
    public enum ResultFormat
    {
        Json,
        Xml,
        Markdown
    }

    // Event args classes
    public class LogEventArgs : EventArgs
    {
        public string Message { get; set; }
        public LogLevel Level { get; set; }
    }

    public class ProgressEventArgs : EventArgs
    {
        public int CurrentStep { get; set; }
        public int TotalSteps { get; set; }
        public string StepName { get; set; }
        public double PercentComplete => TotalSteps > 0 ? (CurrentStep * 100.0) / TotalSteps : 0;
    }

    public class TestCompletedEventArgs : EventArgs
    {
        public TestResult Result { get; }
        public TestCompletedEventArgs(TestResult result) => Result = result;
    }

    public enum LogLevel
    {
        Debug,
        Info,
        Warning,
        Error
    }

    // Fluent API for building tests
    public class TestBuilder
    {
        private readonly List<TestStep> _steps = new();
        private string _name = "Unnamed Test";
        private string _description = "";

        public TestBuilder WithName(string name)
        {
            _name = name;
            return this;
        }

        public TestBuilder WithDescription(string description)
        {
            _description = description;
            return this;
        }

        public TestBuilder AddStep(TestStep step)
        {
            _steps.Add(step);
            return this;
        }

        public TestBuilder Click(int x, int y, MouseButton button = MouseButton.Left)
        {
            return AddStep(new TestStep
            {
                Action = "input.mouse.click",
                Parameters = new Dictionary<string, string>
                {
                    ["x"] = x.ToString(),
                    ["y"] = y.ToString(),
                    ["button"] = button.ToString().ToLower()
                }
            });
        }

        public TestBuilder Move(int x, int y)
        {
            return AddStep(new TestStep
            {
                Action = "input.mouse.move",
                Parameters = new Dictionary<string, string>
                {
                    ["x"] = x.ToString(),
                    ["y"] = y.ToString()
                }
            });
        }

        public TestBuilder Type(string text, int intervalMs = 10)
        {
            return AddStep(new TestStep
            {
                Action = "input.keyboard.type",
                Parameters = new Dictionary<string, string>
                {
                    ["text"] = text,
                    ["interval_ms"] = intervalMs.ToString()
                }
            });
        }

        public TestBuilder KeyPress(string key)
        {
            return AddStep(new TestStep
            {
                Action = "input.keyboard.keypress",
                Parameters = new Dictionary<string, string>
                {
                    ["key"] = key
                }
            });
        }

        public TestBuilder KeyCombo(params string[] keys)
        {
            return AddStep(new TestStep
            {
                Action = "input.keyboard.combo",
                Parameters = new Dictionary<string, string>
                {
                    ["keys"] = string.Join("+", keys)
                }
            });
        }

        public TestBuilder Wait(int milliseconds)
        {
            return AddStep(new TestStep
            {
                Action = "system.wait",
                Parameters = new Dictionary<string, string>
                {
                    ["duration_ms"] = milliseconds.ToString()
                }
            });
        }

        public TestBuilder Screenshot(string name = null)
        {
            return AddStep(new TestStep
            {
                Action = "display.capture",
                Parameters = new Dictionary<string, string>
                {
                    ["name"] = name ?? $"screenshot_{DateTime.Now:yyyyMMdd_HHmmss}"
                }
            });
        }

        // Save to YAML file
        public void SaveToFile(string filePath)
        {
            var yaml = new System.Text.StringBuilder();
            yaml.AppendLine($"name: {_name}");
            yaml.AppendLine($"description: {_description}");
            yaml.AppendLine("version: \"1.0\"");
            yaml.AppendLine();
            yaml.AppendLine("steps:");

            int id = 1;
            foreach (var step in _steps)
            {
                yaml.AppendLine($"  - id: {id++}");
                yaml.AppendLine($"    action: {step.Action}");
                if (step.Parameters.Count > 0)
                {
                    yaml.AppendLine("    params:");
                    foreach (var param in step.Parameters)
                    {
                        yaml.AppendLine($"      {param.Key}: \"{param.Value}\"");
                    }
                }
                yaml.AppendLine();
            }

            System.IO.File.WriteAllText(filePath, yaml.ToString());
        }
    }

    public class TestStep
    {
        public string Action { get; set; }
        public Dictionary<string, string> Parameters { get; set; } = new();
        public string Description { get; set; }
    }

    public enum MouseButton
    {
        Left,
        Right,
        Middle
    }

    // Native method interop
    internal static class NativeMethods
    {
        private const string DllName = "kvm_automation.dll";

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr KvmAutomation_Create();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void KvmAutomation_Destroy(IntPtr handle);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int KvmAutomation_Initialize(IntPtr handle);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void KvmAutomation_Shutdown(IntPtr handle);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr KvmAutomation_LoadTestSuite(IntPtr handle, string filepath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr KvmAutomation_RunTest(IntPtr handle, IntPtr suite);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int KvmAutomation_RunSmokeTest(IntPtr handle);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int KvmResult_GetSuccess(IntPtr result);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int KvmResult_GetTotalSteps(IntPtr result);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int KvmResult_GetPassedSteps(IntPtr result);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int KvmResult_GetFailedSteps(IntPtr result);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void KvmResult_SaveToFile(IntPtr result, string filepath, string format);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void KvmResult_Destroy(IntPtr result);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void KvmTestSuite_Destroy(IntPtr suite);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr KvmAutomation_GetVersion();
    }
}
