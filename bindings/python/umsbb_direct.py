"""
Universal Multi-Segmented Bi-Buffer Bus - Python Direct Binding
No API wrapper - Direct FFI connection with auto-scaling and GPU support
"""

import ctypes
import ctypes.util
import os
import sys
from typing import Optional, Any, List, Union
from enum import IntEnum

# Load the shared library
def load_umsbb_library():
    """Load the UMSBB library with platform-specific handling"""
    if sys.platform.startswith('win'):
        lib_name = 'universal_multi_segmented_bi_buffer_bus.dll'
    elif sys.platform.startswith('darwin'):
        lib_name = 'libuniversal_multi_segmented_bi_buffer_bus.dylib'
    else:
        lib_name = 'libuniversal_multi_segmented_bi_buffer_bus.so'
    
    # Try current directory first, then system paths
    for path in ['.', './lib', '/usr/local/lib', '/usr/lib']:
        full_path = os.path.join(path, lib_name)
        if os.path.exists(full_path):
            return ctypes.CDLL(full_path)
    
    # Fallback to system search
    lib_path = ctypes.util.find_library('universal_multi_segmented_bi_buffer_bus')
    if lib_path:
        return ctypes.CDLL(lib_path)
    
    raise RuntimeError(f"Could not find {lib_name}")

# Language type enum
class LanguageType(IntEnum):
    C = 0
    CPP = 1
    PYTHON = 2
    JAVASCRIPT = 3
    RUST = 4
    GO = 5
    JAVA = 6
    CSHARP = 7
    KOTLIN = 8
    SWIFT = 9

# Structure definitions
class UniversalData(ctypes.Structure):
    _fields_ = [
        ('data', ctypes.c_void_p),
        ('size', ctypes.c_size_t),
        ('type_id', ctypes.c_uint32),
        ('source_lang', ctypes.c_int)
    ]

class ScalingConfig(ctypes.Structure):
    _fields_ = [
        ('min_producers', ctypes.c_uint32),
        ('max_producers', ctypes.c_uint32),
        ('min_consumers', ctypes.c_uint32),
        ('max_consumers', ctypes.c_uint32),
        ('scale_threshold_percent', ctypes.c_uint32),
        ('scale_cooldown_ms', ctypes.c_uint32),
        ('gpu_preferred', ctypes.c_bool),
        ('auto_balance_load', ctypes.c_bool)
    ]

class GPUCapabilities(ctypes.Structure):
    _fields_ = [
        ('has_cuda', ctypes.c_bool),
        ('has_opencl', ctypes.c_bool),
        ('has_compute', ctypes.c_bool),
        ('has_memory_pool', ctypes.c_bool),
        ('memory_size', ctypes.c_size_t),
        ('compute_capability', ctypes.c_int),
        ('max_threads', ctypes.c_size_t)
    ]

# Python-specific allocator functions
@ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_size_t)
def python_allocator(size):
    """Custom Python allocator for C interop"""
    return ctypes.cast(ctypes.create_string_buffer(size), ctypes.c_void_p).value

@ctypes.CFUNCTYPE(None, ctypes.c_void_p)
def python_deallocator(ptr):
    """Custom Python deallocator"""
    # Python garbage collector handles this
    pass

@ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.POINTER(UniversalData))
def python_validator(data):
    """Validate data for Python compatibility"""
    if not data or not data.contents.data or data.contents.size == 0:
        return False
    return True

@ctypes.CFUNCTYPE(None, ctypes.POINTER(UniversalData))
def python_error_handler(data):
    """Handle errors in Python context"""
    print(f"[Python FFI] Error processing data of size {data.contents.size}")

class DirectUniversalBus:
    """
    Direct interface to Universal Multi-Segmented Bi-Buffer Bus
    - No API wrapper overhead
    - Auto-scaling based on load
    - GPU acceleration when available
    - Native Python integration
    """
    
    def __init__(self, buffer_size: int = 1024*1024, segment_count: int = 0, 
                 gpu_preferred: bool = True, auto_scale: bool = True):
        """
        Initialize direct bus connection
        
        Args:
            buffer_size: Size of each buffer segment
            segment_count: Number of segments (0 = auto-determine)
            gpu_preferred: Prefer GPU processing for large operations
            auto_scale: Enable automatic scaling of producers/consumers
        """
        self.lib = load_umsbb_library()
        self._setup_function_signatures()
        self._register_python_runtime()
        
        if auto_scale:
            self._configure_auto_scaling(gpu_preferred)
        
        # Create bus handle
        self.handle = self.lib.umsbb_create_direct(
            ctypes.c_size_t(buffer_size),
            ctypes.c_uint32(segment_count),
            ctypes.c_int(LanguageType.PYTHON)
        )
        
        if not self.handle:
            raise RuntimeError("Failed to create Universal Bus")
        
        print(f"[Python Direct] Bus created with {buffer_size} byte segments")
    
    def _setup_function_signatures(self):
        """Setup C function signatures for type safety"""
        # Core functions
        self.lib.umsbb_create_direct.argtypes = [ctypes.c_size_t, ctypes.c_uint32, ctypes.c_int]
        self.lib.umsbb_create_direct.restype = ctypes.c_void_p
        
        self.lib.umsbb_submit_direct.argtypes = [ctypes.c_void_p, ctypes.POINTER(UniversalData)]
        self.lib.umsbb_submit_direct.restype = ctypes.c_bool
        
        self.lib.umsbb_drain_direct.argtypes = [ctypes.c_void_p, ctypes.c_int]
        self.lib.umsbb_drain_direct.restype = ctypes.POINTER(UniversalData)
        
        self.lib.umsbb_destroy_direct.argtypes = [ctypes.c_void_p]
        self.lib.umsbb_destroy_direct.restype = None
        
        # GPU functions
        self.lib.initialize_gpu.restype = ctypes.c_bool
        self.lib.gpu_available.restype = ctypes.c_bool
        self.lib.get_gpu_capabilities.restype = GPUCapabilities
        
        # Scaling functions
        self.lib.configure_auto_scaling.argtypes = [ctypes.POINTER(ScalingConfig)]
        self.lib.configure_auto_scaling.restype = ctypes.c_bool
        
        self.lib.get_optimal_producer_count.restype = ctypes.c_uint32
        self.lib.get_optimal_consumer_count.restype = ctypes.c_uint32
    
    def _register_python_runtime(self):
        """Register Python runtime with the FFI system"""
        # This would be called if we had runtime registration functions
        pass
    
    def _configure_auto_scaling(self, gpu_preferred: bool):
        """Configure automatic scaling parameters"""
        config = ScalingConfig()
        config.min_producers = 1
        config.max_producers = 16
        config.min_consumers = 1
        config.max_consumers = 8
        config.scale_threshold_percent = 75
        config.scale_cooldown_ms = 1000
        config.gpu_preferred = gpu_preferred
        config.auto_balance_load = True
        
        self.lib.configure_auto_scaling(ctypes.byref(config))
    
    def send(self, data: Union[bytes, bytearray, str], type_id: int = 0) -> bool:
        """
        Send data directly to the bus
        
        Args:
            data: Data to send (automatically converted to bytes)
            type_id: Type identifier for routing
            
        Returns:
            True if successful, False otherwise
        """
        if isinstance(data, str):
            data = data.encode('utf-8')
        elif not isinstance(data, (bytes, bytearray)):
            # Try to serialize common Python objects
            import json
            data = json.dumps(data).encode('utf-8')
        
        # Create universal data structure
        buffer = ctypes.create_string_buffer(data)
        udata = UniversalData()
        udata.data = ctypes.cast(buffer, ctypes.c_void_p)
        udata.size = len(data)
        udata.type_id = type_id
        udata.source_lang = LanguageType.PYTHON
        
        return self.lib.umsbb_submit_direct(self.handle, ctypes.byref(udata))
    
    def receive(self, timeout_ms: int = 0) -> Optional[bytes]:
        """
        Receive data directly from the bus
        
        Args:
            timeout_ms: Timeout in milliseconds (0 = non-blocking)
            
        Returns:
            Received data as bytes, or None if nothing available
        """
        udata_ptr = self.lib.umsbb_drain_direct(self.handle, ctypes.c_int(LanguageType.PYTHON))
        
        if not udata_ptr:
            return None
        
        udata = udata_ptr.contents
        
        # Copy data from C memory to Python bytes
        result = ctypes.string_at(udata.data, udata.size)
        
        # Free the universal data structure
        self.lib.free_universal_data(udata_ptr)
        
        return result
    
    def get_gpu_info(self) -> dict:
        """Get GPU capabilities information"""
        if hasattr(self.lib, 'get_gpu_capabilities'):
            caps = self.lib.get_gpu_capabilities()
            return {
                'available': self.lib.gpu_available(),
                'has_cuda': caps.has_cuda,
                'has_opencl': caps.has_opencl,
                'has_compute': caps.has_compute,
                'memory_size': caps.memory_size,
                'compute_capability': caps.compute_capability,
                'max_threads': caps.max_threads
            }
        return {'available': False}
    
    def get_scaling_status(self) -> dict:
        """Get current auto-scaling status"""
        return {
            'optimal_producers': self.lib.get_optimal_producer_count(),
            'optimal_consumers': self.lib.get_optimal_consumer_count(),
            'gpu_info': self.get_gpu_info()
        }
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    
    def close(self):
        """Close the bus and cleanup resources"""
        if hasattr(self, 'handle') and self.handle:
            self.lib.umsbb_destroy_direct(self.handle)
            self.handle = None

# High-level Python interface
class AutoScalingBus:
    """
    High-level auto-scaling bus for Python applications
    Automatically manages producers and consumers based on load
    """
    
    def __init__(self, **kwargs):
        self.bus = DirectUniversalBus(**kwargs)
        self._producers = []
        self._consumers = []
    
    def start_auto_producers(self, producer_func, count: int = None):
        """Start auto-scaling producers"""
        import threading
        
        if count is None:
            count = self.bus.get_scaling_status()['optimal_producers']
        
        for i in range(count):
            producer = threading.Thread(target=self._producer_worker, args=(producer_func, i))
            producer.daemon = True
            producer.start()
            self._producers.append(producer)
        
        print(f"Started {count} auto-scaling producers")
    
    def start_auto_consumers(self, consumer_func, count: int = None):
        """Start auto-scaling consumers"""
        import threading
        
        if count is None:
            count = self.bus.get_scaling_status()['optimal_consumers']
        
        for i in range(count):
            consumer = threading.Thread(target=self._consumer_worker, args=(consumer_func, i))
            consumer.daemon = True
            consumer.start()
            self._consumers.append(consumer)
        
        print(f"Started {count} auto-scaling consumers")
    
    def _producer_worker(self, producer_func, worker_id):
        """Producer worker thread"""
        while True:
            try:
                data = producer_func(worker_id)
                if data is not None:
                    self.bus.send(data)
            except Exception as e:
                print(f"Producer {worker_id} error: {e}")
    
    def _consumer_worker(self, consumer_func, worker_id):
        """Consumer worker thread"""
        import time
        while True:
            try:
                data = self.bus.receive()
                if data:
                    consumer_func(data, worker_id)
                else:
                    time.sleep(0.001)  # Small delay when no data
            except Exception as e:
                print(f"Consumer {worker_id} error: {e}")

# Example usage
if __name__ == "__main__":
    # Direct usage example
    with DirectUniversalBus(gpu_preferred=True) as bus:
        print("GPU Info:", bus.get_gpu_info())
        print("Scaling Status:", bus.get_scaling_status())
        
        # Send some test data
        bus.send("Hello from Python!", type_id=1)
        bus.send(b"Binary data", type_id=2)
        bus.send({"json": "data", "number": 42}, type_id=3)
        
        # Receive data
        while True:
            data = bus.receive()
            if not data:
                break
            print(f"Received: {data}")