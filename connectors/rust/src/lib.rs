// Rust Connector for UMSBB WebAssembly Core
// Direct memory binding without API overhead

use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_uint, c_void};

// Error codes matching the C interface
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum UMSBBError {
    Success = 0,
    InvalidParams = -1,
    BufferFull = -2,
    BufferEmpty = -3,
    InvalidHandle = -4,
    MemoryAllocation = -5,
    CorruptedData = -6,
}

impl UMSBBError {
    pub fn as_str(&self) -> &'static str {
        match self {
            UMSBBError::Success => "Success",
            UMSBBError::InvalidParams => "Invalid parameters",
            UMSBBError::BufferFull => "Buffer is full",
            UMSBBError::BufferEmpty => "Buffer is empty",
            UMSBBError::InvalidHandle => "Invalid buffer handle",
            UMSBBError::MemoryAllocation => "Memory allocation failed",
            UMSBBError::CorruptedData => "Corrupted data detected",
        }
    }
}

impl std::fmt::Display for UMSBBError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl std::error::Error for UMSBBError {}

// Buffer statistics
#[derive(Debug, Clone)]
pub struct BufferStats {
    pub total_messages: u64,
    pub total_bytes: u64,
    pub pending_messages: u32,
    pub active_segments: u32,
}

// Result type for UMSBB operations
pub type UMSBBResult<T> = Result<T, UMSBBError>;

// Mock interface for development (when WebAssembly module is not available)
lazy_static::lazy_static! {
    static ref MOCK_BUFFERS: Arc<Mutex<HashMap<u32, MockBuffer>>> = Arc::new(Mutex::new(HashMap::new()));
    static ref NEXT_HANDLE: Arc<Mutex<u32>> = Arc::new(Mutex::new(1));
}

#[derive(Debug)]
struct MockBuffer {
    size_mb: u32,
    messages: Vec<Vec<u8>>,
    total_messages: u64,
    total_bytes: u64,
}

// WebAssembly external functions (will be linked when WASM module is available)
extern "C" {
    fn umsbb_create_buffer(size_mb: u32) -> u32;
    fn umsbb_write_message(handle: u32, data: *const c_void, size: u32) -> c_int;
    fn umsbb_read_message(handle: u32, buffer: *mut c_void, buffer_size: u32, actual_size: *mut u32) -> c_int;
    fn umsbb_get_total_messages(handle: u32) -> u64;
    fn umsbb_get_total_bytes(handle: u32) -> u64;
    fn umsbb_get_pending_messages(handle: u32) -> u32;
    fn umsbb_destroy_buffer(handle: u32) -> c_int;
}

pub struct UMSBBBuffer {
    handle: u32,
    use_mock: bool,
}

impl UMSBBBuffer {
    pub fn new(size_mb: u32) -> UMSBBResult<Self> {
        if size_mb < 1 || size_mb > 64 {
            return Err(UMSBBError::InvalidParams);
        }

        // Try to use real WebAssembly module, fallback to mock
        let (handle, use_mock) = unsafe {
            // In a real implementation, we'd check if the WASM module is loaded
            // For now, always use mock for development
            let handle = Self::mock_create_buffer(size_mb);
            (handle, true)
        };

        if handle == 0 {
            return Err(UMSBBError::MemoryAllocation);
        }

        Ok(UMSBBBuffer { handle, use_mock })
    }

    pub fn write(&self, data: &[u8]) -> UMSBBResult<()> {
        if data.len() > 65536 {
            return Err(UMSBBError::InvalidParams);
        }

        let result = if self.use_mock {
            Self::mock_write_message(self.handle, data)
        } else {
            unsafe {
                umsbb_write_message(
                    self.handle,
                    data.as_ptr() as *const c_void,
                    data.len() as u32,
                )
            }
        };

        match result {
            0 => Ok(()),
            -1 => Err(UMSBBError::InvalidParams),
            -2 => Err(UMSBBError::BufferFull),
            -4 => Err(UMSBBError::InvalidHandle),
            _ => Err(UMSBBError::CorruptedData),
        }
    }

    pub fn write_string(&self, data: &str) -> UMSBBResult<()> {
        self.write(data.as_bytes())
    }

    pub fn read(&self) -> UMSBBResult<Option<Vec<u8>>> {
        let mut buffer = vec![0u8; 65536]; // 64KB buffer
        let mut actual_size: u32 = 0;

        let result = if self.use_mock {
            Self::mock_read_message(self.handle, &mut buffer, &mut actual_size)
        } else {
            unsafe {
                umsbb_read_message(
                    self.handle,
                    buffer.as_mut_ptr() as *mut c_void,
                    buffer.len() as u32,
                    &mut actual_size,
                )
            }
        };

        match result {
            0 => {
                buffer.truncate(actual_size as usize);
                Ok(Some(buffer))
            }
            -3 => Ok(None), // Buffer empty
            -1 => Err(UMSBBError::InvalidParams),
            -4 => Err(UMSBBError::InvalidHandle),
            _ => Err(UMSBBError::CorruptedData),
        }
    }

    pub fn read_string(&self) -> UMSBBResult<Option<String>> {
        match self.read()? {
            Some(data) => match String::from_utf8(data) {
                Ok(s) => Ok(Some(s)),
                Err(_) => Err(UMSBBError::CorruptedData),
            },
            None => Ok(None),
        }
    }

    pub fn get_stats(&self) -> BufferStats {
        if self.use_mock {
            Self::mock_get_stats(self.handle)
        } else {
            unsafe {
                BufferStats {
                    total_messages: umsbb_get_total_messages(self.handle),
                    total_bytes: umsbb_get_total_bytes(self.handle),
                    pending_messages: umsbb_get_pending_messages(self.handle),
                    active_segments: 0, // TODO: Implement in core
                }
            }
        }
    }

    pub fn is_empty(&self) -> bool {
        self.get_stats().pending_messages == 0
    }

    pub fn pending_count(&self) -> u32 {
        self.get_stats().pending_messages
    }

    // Mock implementation for development
    fn mock_create_buffer(size_mb: u32) -> u32 {
        let mut handles = MOCK_BUFFERS.lock().unwrap();
        let mut next_handle = NEXT_HANDLE.lock().unwrap();
        
        let handle = *next_handle;
        *next_handle += 1;
        
        handles.insert(handle, MockBuffer {
            size_mb,
            messages: Vec::new(),
            total_messages: 0,
            total_bytes: 0,
        });
        
        handle
    }

    fn mock_write_message(handle: u32, data: &[u8]) -> c_int {
        let mut handles = MOCK_BUFFERS.lock().unwrap();
        
        if let Some(buffer) = handles.get_mut(&handle) {
            if buffer.messages.len() > 1000 {
                return -2; // Buffer full
            }
            
            buffer.messages.push(data.to_vec());
            buffer.total_messages += 1;
            buffer.total_bytes += data.len() as u64;
            0 // Success
        } else {
            -4 // Invalid handle
        }
    }

    fn mock_read_message(handle: u32, buffer: &mut [u8], actual_size: &mut u32) -> c_int {
        let mut handles = MOCK_BUFFERS.lock().unwrap();
        
        if let Some(mock_buffer) = handles.get_mut(&handle) {
            if mock_buffer.messages.is_empty() {
                return -3; // Buffer empty
            }
            
            let message = mock_buffer.messages.remove(0);
            if message.len() > buffer.len() {
                return -1; // Invalid params
            }
            
            buffer[..message.len()].copy_from_slice(&message);
            *actual_size = message.len() as u32;
            0 // Success
        } else {
            -4 // Invalid handle
        }
    }

    fn mock_get_stats(handle: u32) -> BufferStats {
        let handles = MOCK_BUFFERS.lock().unwrap();
        
        if let Some(buffer) = handles.get(&handle) {
            BufferStats {
                total_messages: buffer.total_messages,
                total_bytes: buffer.total_bytes,
                pending_messages: buffer.messages.len() as u32,
                active_segments: 0,
            }
        } else {
            BufferStats {
                total_messages: 0,
                total_bytes: 0,
                pending_messages: 0,
                active_segments: 0,
            }
        }
    }
}

impl Drop for UMSBBBuffer {
    fn drop(&mut self) {
        if self.use_mock {
            let mut handles = MOCK_BUFFERS.lock().unwrap();
            handles.remove(&self.handle);
        } else {
            unsafe {
                umsbb_destroy_buffer(self.handle);
            }
        }
    }
}

// Convenience function
pub fn create_buffer(size_mb: u32) -> UMSBBResult<UMSBBBuffer> {
    UMSBBBuffer::new(size_mb)
}

// Performance test
pub fn performance_test(message_count: u32, buffer_size_mb: u32) -> UMSBBResult<()> {
    use std::thread;
    use std::time::Instant;
    use std::sync::Arc;

    println!("UMSBB Rust Connector Test");
    println!("{}", "=".repeat(40));

    let buffer = Arc::new(create_buffer(buffer_size_mb)?);
    let buffer_producer = Arc::clone(&buffer);
    let buffer_consumer = Arc::clone(&buffer);

    let start_time = Instant::now();

    // Producer thread
    let producer = thread::spawn(move || {
        for i in 0..message_count {
            let message = format!("Message {}", i);
            buffer_producer.write_string(&message).unwrap();
            
            if i % 1000 == 0 {
                println!("Produced {} messages", i);
            }
        }
    });

    // Consumer thread
    let consumer = thread::spawn(move || {
        let mut received = 0;
        while received < message_count {
            if let Ok(Some(_message)) = buffer_consumer.read() {
                received += 1;
                if received % 1000 == 0 {
                    println!("Consumed {} messages", received);
                }
            } else {
                thread::sleep(std::time::Duration::from_micros(100));
            }
        }
    });

    producer.join().unwrap();
    consumer.join().unwrap();

    let duration = start_time.elapsed();
    let duration_sec = duration.as_secs_f64();

    let stats = buffer.get_stats();

    println!("\nTest Results:");
    println!("Duration: {:.3} seconds", duration_sec);
    println!("Messages: {}", stats.total_messages);
    println!("Bytes: {}", stats.total_bytes);
    println!("Messages/sec: {:.0}", stats.total_messages as f64 / duration_sec);
    println!("MB/sec: {:.2}", stats.total_bytes as f64 / (1024.0 * 1024.0) / duration_sec);

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_buffer_creation() {
        let buffer = create_buffer(16).unwrap();
        assert!(buffer.is_empty());
    }

    #[test]
    fn test_write_read() {
        let buffer = create_buffer(16).unwrap();
        
        let test_data = b"Hello, UMSBB!";
        buffer.write(test_data).unwrap();
        
        let read_data = buffer.read().unwrap().unwrap();
        assert_eq!(read_data, test_data);
    }

    #[test]
    fn test_string_operations() {
        let buffer = create_buffer(16).unwrap();
        
        let test_message = "Hello, World!";
        buffer.write_string(test_message).unwrap();
        
        let read_message = buffer.read_string().unwrap().unwrap();
        assert_eq!(read_message, test_message);
    }

    #[test]
    fn test_statistics() {
        let buffer = create_buffer(16).unwrap();
        
        buffer.write_string("Test message 1").unwrap();
        buffer.write_string("Test message 2").unwrap();
        
        let stats = buffer.get_stats();
        assert_eq!(stats.total_messages, 2);
        assert_eq!(stats.pending_messages, 2);
    }

    #[test]
    fn test_invalid_size() {
        assert!(create_buffer(0).is_err());
        assert!(create_buffer(65).is_err());
    }

    #[test]
    fn test_large_message() {
        let buffer = create_buffer(16).unwrap();
        let large_data = vec![0u8; 65537]; // Larger than 64KB
        
        assert!(buffer.write(&large_data).is_err());
    }
}

// Example usage
fn main() -> UMSBBResult<()> {
    // Run performance test
    performance_test(10000, 32)?;
    Ok(())
}