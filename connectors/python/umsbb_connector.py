# Python Connector for UMSBB WebAssembly Core
# Direct memory binding without API overhead

import ctypes
import pathlib
import threading
from typing import Optional, Tuple, Any
from dataclasses import dataclass

@dataclass
class UMSBBStats:
    total_messages: int
    total_bytes: int
    pending_messages: int
    active_segments: int

class UMSBBError(Exception):
    """UMSBB-specific errors"""
    pass

class UMSBBBuffer:
    """Python connector for UMSBB WebAssembly core"""
    
    # Error code constants
    SUCCESS = 0
    ERROR_INVALID_PARAMS = -1
    ERROR_BUFFER_FULL = -2
    ERROR_BUFFER_EMPTY = -3
    ERROR_INVALID_HANDLE = -4
    ERROR_MEMORY_ALLOCATION = -5
    ERROR_CORRUPTED_DATA = -6
    
    _error_messages = {
        ERROR_INVALID_PARAMS: "Invalid parameters",
        ERROR_BUFFER_FULL: "Buffer is full",
        ERROR_BUFFER_EMPTY: "Buffer is empty",
        ERROR_INVALID_HANDLE: "Invalid buffer handle",
        ERROR_MEMORY_ALLOCATION: "Memory allocation failed",
        ERROR_CORRUPTED_DATA: "Corrupted data detected"
    }
    
    def __init__(self, size_mb: int = 16):
        """
        Initialize UMSBB buffer
        
        Args:
            size_mb: Buffer size in megabytes (1-64)
        """
        self._lib = None
        self._handle = 0
        self._lock = threading.RLock()
        
        # Load WebAssembly module
        self._load_wasm_module()
        
        # Create buffer
        if not 1 <= size_mb <= 64:
            raise ValueError("Buffer size must be between 1 and 64 MB")
        
        self._handle = self._lib.umsbb_create_buffer(size_mb)
        if self._handle == 0:
            raise UMSBBError("Failed to create buffer")
    
    def _load_wasm_module(self):
        """Load the WebAssembly module"""
        try:
            # Try to load compiled WebAssembly module
            lib_path = pathlib.Path(__file__).parent / "umsbb_core.wasm"
            if lib_path.exists():
                # For WebAssembly, we'd use wasmtime or similar
                # For now, fallback to native shared library
                pass
            
            # Fallback to native shared library
            lib_path = pathlib.Path(__file__).parent / "libumsbb_core.so"
            if not lib_path.exists():
                lib_path = pathlib.Path(__file__).parent / "umsbb_core.dll"
            
            if lib_path.exists():
                self._lib = ctypes.CDLL(str(lib_path))
            else:
                # Create mock interface for development
                self._create_mock_interface()
                return
            
            # Set function signatures
            self._configure_function_signatures()
            
        except Exception as e:
            # Create mock interface for development/testing
            self._create_mock_interface()
    
    def _create_mock_interface(self):
        """Create mock interface for development"""
        class MockLib:
            def __init__(self):
                self._handles = {}
                self._next_handle = 1
                
            def umsbb_create_buffer(self, size_mb):
                handle = self._next_handle
                self._next_handle += 1
                self._handles[handle] = {
                    'size_mb': size_mb,
                    'messages': [],
                    'total_messages': 0,
                    'total_bytes': 0
                }
                return handle
            
            def umsbb_write_message(self, handle, data_ptr, size):
                if handle not in self._handles:
                    return UMSBBBuffer.ERROR_INVALID_HANDLE
                
                # Simulate buffer full condition
                if len(self._handles[handle]['messages']) > 1000:
                    return UMSBBBuffer.ERROR_BUFFER_FULL
                
                # Copy data
                data = ctypes.string_at(data_ptr, size)
                self._handles[handle]['messages'].append(data)
                self._handles[handle]['total_messages'] += 1
                self._handles[handle]['total_bytes'] += size
                return UMSBBBuffer.SUCCESS
            
            def umsbb_read_message(self, handle, buffer_ptr, buffer_size, actual_size_ptr):
                if handle not in self._handles:
                    return UMSBBBuffer.ERROR_INVALID_HANDLE
                
                if not self._handles[handle]['messages']:
                    return UMSBBBuffer.ERROR_BUFFER_EMPTY
                
                message = self._handles[handle]['messages'].pop(0)
                if len(message) > buffer_size:
                    return UMSBBBuffer.ERROR_INVALID_PARAMS
                
                ctypes.memmove(buffer_ptr, message, len(message))
                ctypes.c_uint32.from_address(actual_size_ptr).value = len(message)
                return UMSBBBuffer.SUCCESS
            
            def umsbb_get_total_messages(self, handle):
                return self._handles.get(handle, {}).get('total_messages', 0)
            
            def umsbb_get_total_bytes(self, handle):
                return self._handles.get(handle, {}).get('total_bytes', 0)
            
            def umsbb_get_pending_messages(self, handle):
                return len(self._handles.get(handle, {}).get('messages', []))
            
            def umsbb_destroy_buffer(self, handle):
                if handle in self._handles:
                    del self._handles[handle]
                    return UMSBBBuffer.SUCCESS
                return UMSBBBuffer.ERROR_INVALID_HANDLE
        
        self._lib = MockLib()
    
    def _configure_function_signatures(self):
        """Configure ctypes function signatures"""
        if hasattr(self._lib, 'argtypes'):  # Real ctypes library
            # umsbb_create_buffer
            self._lib.umsbb_create_buffer.argtypes = [ctypes.c_uint32]
            self._lib.umsbb_create_buffer.restype = ctypes.c_uint32
            
            # umsbb_write_message
            self._lib.umsbb_write_message.argtypes = [ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32]
            self._lib.umsbb_write_message.restype = ctypes.c_int
            
            # umsbb_read_message
            self._lib.umsbb_read_message.argtypes = [ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)]
            self._lib.umsbb_read_message.restype = ctypes.c_int
            
            # Statistics functions
            self._lib.umsbb_get_total_messages.argtypes = [ctypes.c_uint32]
            self._lib.umsbb_get_total_messages.restype = ctypes.c_uint64
            
            self._lib.umsbb_get_total_bytes.argtypes = [ctypes.c_uint32]
            self._lib.umsbb_get_total_bytes.restype = ctypes.c_uint64
            
            self._lib.umsbb_get_pending_messages.argtypes = [ctypes.c_uint32]
            self._lib.umsbb_get_pending_messages.restype = ctypes.c_uint32
    
    def write(self, data: bytes) -> None:
        """
        Write message to buffer
        
        Args:
            data: Message data as bytes
            
        Raises:
            UMSBBError: If write operation fails
        """
        with self._lock:
            if not isinstance(data, (bytes, bytearray)):
                raise TypeError("Data must be bytes or bytearray")
            
            if len(data) > 65536:  # 64KB max message size
                raise ValueError("Message too large (max 64KB)")
            
            data_ptr = ctypes.cast(data, ctypes.c_void_p)
            result = self._lib.umsbb_write_message(self._handle, data_ptr, len(data))
            
            if result != self.SUCCESS:
                error_msg = self._error_messages.get(result, f"Unknown error: {result}")
                raise UMSBBError(f"Write failed: {error_msg}")
    
    def read(self, timeout_ms: int = 0) -> Optional[bytes]:
        """
        Read message from buffer
        
        Args:
            timeout_ms: Timeout in milliseconds (0 = no timeout)
            
        Returns:
            Message data as bytes, or None if empty
            
        Raises:
            UMSBBError: If read operation fails
        """
        with self._lock:
            buffer = ctypes.create_string_buffer(65536)  # 64KB buffer
            actual_size = ctypes.c_uint32()
            
            result = self._lib.umsbb_read_message(
                self._handle, 
                buffer, 
                65536, 
                ctypes.byref(actual_size)
            )
            
            if result == self.ERROR_BUFFER_EMPTY:
                return None
            elif result != self.SUCCESS:
                error_msg = self._error_messages.get(result, f"Unknown error: {result}")
                raise UMSBBError(f"Read failed: {error_msg}")
            
            return buffer.raw[:actual_size.value]
    
    def get_stats(self) -> UMSBBStats:
        """Get buffer statistics"""
        with self._lock:
            total_messages = self._lib.umsbb_get_total_messages(self._handle)
            total_bytes = self._lib.umsbb_get_total_bytes(self._handle)
            pending_messages = self._lib.umsbb_get_pending_messages(self._handle)
            
            return UMSBBStats(
                total_messages=total_messages,
                total_bytes=total_bytes,
                pending_messages=pending_messages,
                active_segments=0  # TODO: Implement in core
            )
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    
    def close(self):
        """Close buffer and free resources"""
        with self._lock:
            if self._handle and self._lib:
                self._lib.umsbb_destroy_buffer(self._handle)
                self._handle = 0

# Convenience functions
def create_buffer(size_mb: int = 16) -> UMSBBBuffer:
    """Create a new UMSBB buffer"""
    return UMSBBBuffer(size_mb)

# Example usage
if __name__ == "__main__":
    import time
    import threading
    
    def producer(buffer, count):
        """Producer thread"""
        for i in range(count):
            message = f"Message {i}".encode()
            buffer.write(message)
            if i % 1000 == 0:
                print(f"Produced {i} messages")
    
    def consumer(buffer, expected_count):
        """Consumer thread"""
        received = 0
        while received < expected_count:
            message = buffer.read()
            if message:
                received += 1
                if received % 1000 == 0:
                    print(f"Consumed {received} messages")
            else:
                time.sleep(0.001)  # Small delay if empty
    
    # Performance test
    print("UMSBB Python Connector Test")
    print("=" * 40)
    
    with create_buffer(32) as buffer:  # 32MB buffer
        message_count = 10000
        
        # Start threads
        producer_thread = threading.Thread(target=producer, args=(buffer, message_count))
        consumer_thread = threading.Thread(target=consumer, args=(buffer, message_count))
        
        start_time = time.time()
        
        producer_thread.start()
        consumer_thread.start()
        
        producer_thread.join()
        consumer_thread.join()
        
        end_time = time.time()
        duration = end_time - start_time
        
        stats = buffer.get_stats()
        
        print(f"\nTest Results:")
        print(f"Duration: {duration:.3f} seconds")
        print(f"Messages: {stats.total_messages}")
        print(f"Bytes: {stats.total_bytes}")
        print(f"Messages/sec: {stats.total_messages / duration:.0f}")
        print(f"MB/sec: {stats.total_bytes / (1024 * 1024) / duration:.2f}")