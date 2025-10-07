/*
 * Universal Multi-Segmented Bi-Buffer Bus - C# Direct Binding
 * No API wrapper - Direct FFI connection with auto-scaling and GPU support
 */

using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Concurrent;

namespace UniversalMultiSegmentedBiBufferBus
{
    /// <summary>
    /// Language types supported by the Universal Bus
    /// </summary>
    public enum LanguageType
    {
        C = 0,
        Cpp = 1,
        Python = 2,
        JavaScript = 3,
        Rust = 4,
        Go = 5,
        Java = 6,
        CSharp = 7,
        Kotlin = 8,
        Swift = 9
    }

    /// <summary>
    /// Universal data structure for cross-language compatibility
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct UniversalData
    {
        public IntPtr Data;
        public UIntPtr Size;
        public uint TypeId;
        public LanguageType SourceLang;
    }

    /// <summary>
    /// Auto-scaling configuration
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct ScalingConfig
    {
        public uint MinProducers;
        public uint MaxProducers;
        public uint MinConsumers;
        public uint MaxConsumers;
        public uint ScaleThresholdPercent;
        public uint ScaleCooldownMs;
        [MarshalAs(UnmanagedType.I1)]
        public bool GpuPreferred;
        [MarshalAs(UnmanagedType.I1)]
        public bool AutoBalanceLoad;
    }

    /// <summary>
    /// GPU capabilities information
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct GpuCapabilities
    {
        [MarshalAs(UnmanagedType.I1)]
        public bool HasCuda;
        [MarshalAs(UnmanagedType.I1)]
        public bool HasOpenCL;
        [MarshalAs(UnmanagedType.I1)]
        public bool HasCompute;
        [MarshalAs(UnmanagedType.I1)]
        public bool HasMemoryPool;
        public UIntPtr MemorySize;
        public int ComputeCapability;
        public UIntPtr MaxThreads;
    }

    /// <summary>
    /// GPU information wrapper for .NET
    /// </summary>
    public class GpuInfo
    {
        public bool Available { get; set; }
        public bool HasCuda { get; set; }
        public bool HasOpenCL { get; set; }
        public bool HasCompute { get; set; }
        public ulong MemorySize { get; set; }
        public int ComputeCapability { get; set; }
        public ulong MaxThreads { get; set; }
    }

    /// <summary>
    /// Scaling status information
    /// </summary>
    public class ScalingStatus
    {
        public uint OptimalProducers { get; set; }
        public uint OptimalConsumers { get; set; }
        public GpuInfo GpuInfo { get; set; }
    }

    /// <summary>
    /// Native library interop
    /// </summary>
    internal static class NativeMethods
    {
        private const string LibraryName = "universal_multi_segmented_bi_buffer_bus";

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr umsbb_create_direct(UIntPtr bufferSize, uint segmentCount, LanguageType lang);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool umsbb_submit_direct(IntPtr handle, ref UniversalData data);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr umsbb_drain_direct(IntPtr handle, LanguageType targetLang);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void umsbb_destroy_direct(IntPtr handle);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool initialize_gpu();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool gpu_available();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern GpuCapabilities get_gpu_capabilities();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool configure_auto_scaling(ref ScalingConfig config);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint get_optimal_producer_count();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint get_optimal_consumer_count();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void trigger_scale_evaluation();

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr create_universal_data(IntPtr data, UIntPtr size, uint typeId, LanguageType lang);

        [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void free_universal_data(IntPtr data);
    }

    /// <summary>
    /// Direct Universal Bus for .NET
    /// Provides direct access to the Universal Multi-Segmented Bi-Buffer Bus
    /// with auto-scaling and GPU acceleration support
    /// </summary>
    public class DirectUniversalBus : IDisposable
    {
        private IntPtr _handle;
        private readonly ulong _bufferSize;
        private readonly uint _segmentCount;
        private readonly bool _gpuEnabled;
        private bool _disposed = false;
        private readonly object _lock = new object();

        /// <summary>
        /// Creates a new Direct Universal Bus
        /// </summary>
        /// <param name="bufferSize">Size of each buffer segment (default: 1MB)</param>
        /// <param name="segmentCount">Number of segments (0 = auto-determine)</param>
        /// <param name="gpuPreferred">Prefer GPU processing for large operations</param>
        /// <param name="autoScale">Enable automatic scaling</param>
        /// <exception cref="InvalidOperationException">Thrown when bus creation fails</exception>
        /// <example>
        /// <code>
        /// using var bus = new DirectUniversalBus(1024 * 1024, 0, true, true);
        /// </code>
        /// </example>
        public DirectUniversalBus(ulong bufferSize = 1024 * 1024, uint segmentCount = 0, bool gpuPreferred = true, bool autoScale = true)
        {
            _bufferSize = bufferSize;
            _segmentCount = segmentCount;

            if (autoScale)
            {
                ConfigureAutoScaling(gpuPreferred);
            }

            _handle = NativeMethods.umsbb_create_direct(new UIntPtr(bufferSize), segmentCount, LanguageType.CSharp);
            if (_handle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Failed to create Universal Bus");
            }

            _gpuEnabled = gpuPreferred && NativeMethods.initialize_gpu();

            Console.WriteLine($"[C# Direct] Bus created with {bufferSize} byte segments, GPU: {_gpuEnabled}");
        }

        /// <summary>
        /// Configures automatic scaling parameters
        /// </summary>
        private static void ConfigureAutoScaling(bool gpuPreferred)
        {
            var config = new ScalingConfig
            {
                MinProducers = 1,
                MaxProducers = 16,
                MinConsumers = 1,
                MaxConsumers = 8,
                ScaleThresholdPercent = 75,
                ScaleCooldownMs = 1000,
                GpuPreferred = gpuPreferred,
                AutoBalanceLoad = true
            };

            if (!NativeMethods.configure_auto_scaling(ref config))
            {
                throw new InvalidOperationException("Failed to configure auto-scaling");
            }
        }

        /// <summary>
        /// Sends data to the bus
        /// </summary>
        /// <param name="data">Data to send</param>
        /// <param name="typeId">Type identifier for routing</param>
        /// <returns>True if successful, false otherwise</returns>
        /// <exception cref="ObjectDisposedException">Thrown when bus is disposed</exception>
        /// <example>
        /// <code>
        /// bus.Send("Hello from C#!", 1);
        /// bus.Send(Encoding.UTF8.GetBytes("Binary data"), 2);
        /// </code>
        /// </example>
        public bool Send(byte[] data, uint typeId = 0)
        {
            if (_disposed) throw new ObjectDisposedException(nameof(DirectUniversalBus));
            if (data == null || data.Length == 0) return false;

            lock (_lock)
            {
                if (_handle == IntPtr.Zero) return false;

                // Pin the data in memory
                var dataHandle = GCHandle.Alloc(data, GCHandleType.Pinned);
                try
                {
                    var dataPtr = dataHandle.AddrOfPinnedObject();
                    var udataPtr = NativeMethods.create_universal_data(dataPtr, new UIntPtr((uint)data.Length), typeId, LanguageType.CSharp);
                    
                    if (udataPtr == IntPtr.Zero) return false;

                    try
                    {
                        var udata = Marshal.PtrToStructure<UniversalData>(udataPtr);
                        return NativeMethods.umsbb_submit_direct(_handle, ref udata);
                    }
                    finally
                    {
                        NativeMethods.free_universal_data(udataPtr);
                    }
                }
                finally
                {
                    dataHandle.Free();
                }
            }
        }

        /// <summary>
        /// Sends string data to the bus
        /// </summary>
        /// <param name="message">String message to send</param>
        /// <param name="typeId">Type identifier for routing</param>
        /// <returns>True if successful, false otherwise</returns>
        public bool Send(string message, uint typeId = 0)
        {
            if (string.IsNullOrEmpty(message)) return false;
            return Send(Encoding.UTF8.GetBytes(message), typeId);
        }

        /// <summary>
        /// Receives data from the bus
        /// </summary>
        /// <param name="cancellationToken">Cancellation token for async operation</param>
        /// <returns>Received data as byte array, or null if nothing available</returns>
        /// <exception cref="ObjectDisposedException">Thrown when bus is disposed</exception>
        /// <example>
        /// <code>
        /// var data = bus.Receive();
        /// if (data != null)
        /// {
        ///     Console.WriteLine($"Received: {Encoding.UTF8.GetString(data)}");
        /// }
        /// </code>
        /// </example>
        public byte[] Receive(CancellationToken cancellationToken = default)
        {
            if (_disposed) throw new ObjectDisposedException(nameof(DirectUniversalBus));

            lock (_lock)
            {
                if (_handle == IntPtr.Zero) return null;

                var udataPtr = NativeMethods.umsbb_drain_direct(_handle, LanguageType.CSharp);
                if (udataPtr == IntPtr.Zero) return null;

                try
                {
                    var udata = Marshal.PtrToStructure<UniversalData>(udataPtr);
                    if (udata.Data == IntPtr.Zero || udata.Size.ToUInt64() == 0) return null;

                    var result = new byte[udata.Size.ToUInt64()];
                    Marshal.Copy(udata.Data, result, 0, result.Length);
                    return result;
                }
                finally
                {
                    NativeMethods.free_universal_data(udataPtr);
                }
            }
        }

        /// <summary>
        /// Receives string data from the bus
        /// </summary>
        /// <param name="cancellationToken">Cancellation token for async operation</param>
        /// <returns>Received string, or null if nothing available</returns>
        public string ReceiveString(CancellationToken cancellationToken = default)
        {
            var data = Receive(cancellationToken);
            return data != null ? Encoding.UTF8.GetString(data) : null;
        }

        /// <summary>
        /// Sends data and waits for a response
        /// </summary>
        /// <param name="data">Data to send</param>
        /// <param name="typeId">Type identifier</param>
        /// <param name="timeoutMs">Timeout in milliseconds</param>
        /// <param name="cancellationToken">Cancellation token</param>
        /// <returns>Response data, or null if no response within timeout</returns>
        public async Task<byte[]> SendAndReceiveAsync(byte[] data, uint typeId = 0, int timeoutMs = 5000, CancellationToken cancellationToken = default)
        {
            if (!Send(data, typeId)) return null;

            using var cts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            cts.CancelAfter(timeoutMs);

            while (!cts.Token.IsCancellationRequested)
            {
                var response = Receive(cts.Token);
                if (response != null) return response;
                
                await Task.Delay(1, cts.Token).ConfigureAwait(false);
            }

            return null;
        }

        /// <summary>
        /// Gets GPU capabilities information
        /// </summary>
        /// <returns>GPU information</returns>
        public GpuInfo GetGpuInfo()
        {
            var caps = NativeMethods.get_gpu_capabilities();
            var available = NativeMethods.gpu_available();

            return new GpuInfo
            {
                Available = available,
                HasCuda = caps.HasCuda,
                HasOpenCL = caps.HasOpenCL,
                HasCompute = caps.HasCompute,
                MemorySize = caps.MemorySize.ToUInt64(),
                ComputeCapability = caps.ComputeCapability,
                MaxThreads = caps.MaxThreads.ToUInt64()
            };
        }

        /// <summary>
        /// Gets current auto-scaling status
        /// </summary>
        /// <returns>Scaling status information</returns>
        public ScalingStatus GetScalingStatus()
        {
            return new ScalingStatus
            {
                OptimalProducers = NativeMethods.get_optimal_producer_count(),
                OptimalConsumers = NativeMethods.get_optimal_consumer_count(),
                GpuInfo = GetGpuInfo()
            };
        }

        /// <summary>
        /// Triggers manual scale evaluation
        /// </summary>
        public void TriggerScaleEvaluation()
        {
            NativeMethods.trigger_scale_evaluation();
        }

        /// <summary>
        /// Disposes the bus and cleans up resources
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Protected dispose method
        /// </summary>
        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                lock (_lock)
                {
                    if (_handle != IntPtr.Zero)
                    {
                        NativeMethods.umsbb_destroy_direct(_handle);
                        _handle = IntPtr.Zero;
                        Console.WriteLine("[C# Direct] Bus disposed");
                    }
                }
                _disposed = true;
            }
        }

        /// <summary>
        /// Finalizer
        /// </summary>
        ~DirectUniversalBus()
        {
            Dispose(false);
        }
    }

    /// <summary>
    /// Auto-scaling producer-consumer system for .NET
    /// </summary>
    public class AutoScalingBus : IDisposable
    {
        private readonly DirectUniversalBus _bus;
        private readonly List<CancellationTokenSource> _producers = new List<CancellationTokenSource>();
        private readonly List<CancellationTokenSource> _consumers = new List<CancellationTokenSource>();
        private readonly ConcurrentQueue<Task> _runningTasks = new ConcurrentQueue<Task>();
        private bool _disposed = false;

        /// <summary>
        /// Creates a new auto-scaling bus
        /// </summary>
        /// <param name="bufferSize">Buffer size</param>
        /// <param name="segmentCount">Segment count</param>
        /// <param name="gpuPreferred">GPU preferred</param>
        public AutoScalingBus(ulong bufferSize = 1024 * 1024, uint segmentCount = 0, bool gpuPreferred = true)
        {
            _bus = new DirectUniversalBus(bufferSize, segmentCount, gpuPreferred, true);
        }

        /// <summary>
        /// Starts auto-scaling producers
        /// </summary>
        /// <param name="producerFunc">Function that generates data</param>
        /// <param name="count">Number of producers (null = auto-determine)</param>
        /// <example>
        /// <code>
        /// bus.StartAutoProducers(workerId => Encoding.UTF8.GetBytes($"Message from producer {workerId}"), null);
        /// </code>
        /// </example>
        public void StartAutoProducers(Func<uint, byte[]> producerFunc, uint? count = null)
        {
            var producerCount = count ?? _bus.GetScalingStatus().OptimalProducers;

            for (uint i = 0; i < producerCount; i++)
            {
                var cts = new CancellationTokenSource();
                _producers.Add(cts);

                var workerId = i;
                var task = Task.Run(async () =>
                {
                    while (!cts.Token.IsCancellationRequested)
                    {
                        try
                        {
                            var data = producerFunc(workerId);
                            if (data != null)
                            {
                                _bus.Send(data, workerId);
                            }
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"Producer {workerId} error: {ex.Message}");
                        }

                        await Task.Delay(1, cts.Token).ConfigureAwait(false);
                    }
                }, cts.Token);

                _runningTasks.Enqueue(task);
            }

            Console.WriteLine($"Started {producerCount} auto-scaling producers");
        }

        /// <summary>
        /// Starts auto-scaling consumers
        /// </summary>
        /// <param name="consumerFunc">Function that processes data</param>
        /// <param name="count">Number of consumers (null = auto-determine)</param>
        /// <example>
        /// <code>
        /// bus.StartAutoConsumers((data, workerId) => 
        ///     Console.WriteLine($"Consumer {workerId} received: {Encoding.UTF8.GetString(data)}"), null);
        /// </code>
        /// </example>
        public void StartAutoConsumers(Action<byte[], uint> consumerFunc, uint? count = null)
        {
            var consumerCount = count ?? _bus.GetScalingStatus().OptimalConsumers;

            for (uint i = 0; i < consumerCount; i++)
            {
                var cts = new CancellationTokenSource();
                _consumers.Add(cts);

                var workerId = i;
                var task = Task.Run(async () =>
                {
                    while (!cts.Token.IsCancellationRequested)
                    {
                        try
                        {
                            var data = _bus.Receive(cts.Token);
                            if (data != null)
                            {
                                consumerFunc(data, workerId);
                            }
                            else
                            {
                                await Task.Delay(1, cts.Token).ConfigureAwait(false);
                            }
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"Consumer {workerId} error: {ex.Message}");
                        }
                    }
                }, cts.Token);

                _runningTasks.Enqueue(task);
            }

            Console.WriteLine($"Started {consumerCount} auto-scaling consumers");
        }

        /// <summary>
        /// Stops all producers and consumers
        /// </summary>
        public async Task StopAsync()
        {
            // Cancel all producers
            foreach (var cts in _producers)
            {
                cts.Cancel();
            }
            _producers.Clear();

            // Cancel all consumers
            foreach (var cts in _consumers)
            {
                cts.Cancel();
            }
            _consumers.Clear();

            // Wait for all tasks to complete
            var tasks = new List<Task>();
            while (_runningTasks.TryDequeue(out var task))
            {
                tasks.Add(task);
            }

            try
            {
                await Task.WhenAll(tasks).ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                // Expected when cancelling tasks
            }

            Console.WriteLine("[C# AutoScale] Stopped all workers");
        }

        /// <summary>
        /// Disposes the auto-scaling bus
        /// </summary>
        public void Dispose()
        {
            if (!_disposed)
            {
                StopAsync().GetAwaiter().GetResult();
                _bus?.Dispose();
                _disposed = true;
            }
        }
    }

    /// <summary>
    /// Example usage and demonstrations
    /// </summary>
    public static class Examples
    {
        /// <summary>
        /// Basic usage example
        /// </summary>
        public static void BasicUsage()
        {
            // Direct usage example
            using var bus = new DirectUniversalBus(1024 * 1024, 0, true, true);

            Console.WriteLine($"GPU Info: {bus.GetGpuInfo()}");
            Console.WriteLine($"Scaling Status: {bus.GetScalingStatus()}");

            // Send test data
            bus.Send("Hello from C#!", 1);
            bus.Send(Encoding.UTF8.GetBytes("Binary data"), 2);

            // Receive data
            byte[] data;
            while ((data = bus.Receive()) != null)
            {
                Console.WriteLine($"Received: {Encoding.UTF8.GetString(data)}");
            }
        }

        /// <summary>
        /// Auto-scaling example
        /// </summary>
        public static async Task AutoScalingExample()
        {
            using var bus = new AutoScalingBus(1024 * 1024, 0, true);

            // Start producers
            bus.StartAutoProducers(workerId => 
                Encoding.UTF8.GetBytes($"Message from producer {workerId} at {DateTime.Now:HH:mm:ss.fff}"), 2);

            // Start consumers
            bus.StartAutoConsumers((data, workerId) =>
                Console.WriteLine($"Consumer {workerId} received: {Encoding.UTF8.GetString(data)}"), 2);

            // Let it run for a while
            await Task.Delay(5000);

            await bus.StopAsync();
        }

        /// <summary>
        /// Performance benchmark
        /// </summary>
        public static TimeSpan BenchmarkSend(byte[] data, int iterations)
        {
            using var bus = new DirectUniversalBus(1024 * 1024, 8, true, false);

            var stopwatch = System.Diagnostics.Stopwatch.StartNew();
            for (int i = 0; i < iterations; i++)
            {
                bus.Send(data, (uint)(i % 256));
            }
            stopwatch.Stop();

            return stopwatch.Elapsed;
        }
    }
}