//! Universal Multi-Segmented Bi-Buffer Bus - Rust Direct Binding
//! No API wrapper - Direct FFI connection with auto-scaling and GPU support

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_uint, c_void};
use std::ptr;
use std::slice;

// Language types
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub enum LanguageType {
    C = 0,
    Cpp = 1,
    Python = 2,
    Javascript = 3,
    Rust = 4,
    Go = 5,
    Java = 6,
    CSharp = 7,
    Kotlin = 8,
    Swift = 9,
}

// Universal data structure
#[repr(C)]
#[derive(Debug)]
pub struct UniversalData {
    pub data: *mut c_void,
    pub size: usize,
    pub type_id: u32,
    pub source_lang: LanguageType,
}

// Scaling configuration
#[repr(C)]
#[derive(Debug, Clone)]
pub struct ScalingConfig {
    pub min_producers: u32,
    pub max_producers: u32,
    pub min_consumers: u32,
    pub max_consumers: u32,
    pub scale_threshold_percent: u32,
    pub scale_cooldown_ms: u32,
    pub gpu_preferred: bool,
    pub auto_balance_load: bool,
}

// GPU capabilities
#[repr(C)]
#[derive(Debug, Clone)]
pub struct GpuCapabilities {
    pub has_cuda: bool,
    pub has_opencl: bool,
    pub has_compute: bool,
    pub has_memory_pool: bool,
    pub memory_size: usize,
    pub compute_capability: i32,
    pub max_threads: usize,
}

// External C functions
extern "C" {
    // Core functions
    fn umsbb_create_direct(buffer_size: usize, segment_count: u32, lang: LanguageType) -> *mut c_void;
    fn umsbb_submit_direct(handle: *mut c_void, data: *const UniversalData) -> bool;
    fn umsbb_drain_direct(handle: *mut c_void, target_lang: LanguageType) -> *mut UniversalData;
    fn umsbb_destroy_direct(handle: *mut c_void);
    
    // GPU functions
    fn initialize_gpu() -> bool;
    fn gpu_available() -> bool;
    fn get_gpu_capabilities() -> GpuCapabilities;
    
    // Scaling functions
    fn configure_auto_scaling(config: *const ScalingConfig) -> bool;
    fn get_optimal_producer_count() -> u32;
    fn get_optimal_consumer_count() -> u32;
    fn trigger_scale_evaluation();
    
    // Memory management
    fn create_universal_data(data: *const c_void, size: usize, type_id: u32, lang: LanguageType) -> *mut UniversalData;
    fn free_universal_data(data: *mut UniversalData);
}

/// Direct Universal Bus for Rust
/// 
/// Provides zero-cost abstractions over the native C implementation
/// with Rust safety guarantees and ergonomic APIs
pub struct DirectUniversalBus {
    handle: *mut c_void,
    buffer_size: usize,
    segment_count: u32,
    gpu_enabled: bool,
}

impl DirectUniversalBus {
    /// Create a new Direct Universal Bus
    /// 
    /// # Arguments
    /// * `buffer_size` - Size of each buffer segment (default: 1MB)
    /// * `segment_count` - Number of segments (0 = auto-determine)
    /// * `gpu_preferred` - Prefer GPU processing for large operations
    /// * `auto_scale` - Enable automatic scaling
    /// 
    /// # Example
    /// ```rust
    /// use umsbb_direct::DirectUniversalBus;
    /// 
    /// let bus = DirectUniversalBus::new(1024 * 1024, 0, true, true)
    ///     .expect("Failed to create bus");
    /// ```
    pub fn new(
        buffer_size: usize,
        segment_count: u32,
        gpu_preferred: bool,
        auto_scale: bool,
    ) -> Result<Self, String> {
        if auto_scale {
            Self::configure_auto_scaling_internal(gpu_preferred)?;
        }

        let handle = unsafe {
            umsbb_create_direct(buffer_size, segment_count, LanguageType::Rust)
        };

        if handle.is_null() {
            return Err("Failed to create Universal Bus".to_string());
        }

        let gpu_enabled = if gpu_preferred {
            unsafe { initialize_gpu() }
        } else {
            false
        };

        println!(
            "[Rust Direct] Bus created with {} byte segments, GPU: {}",
            buffer_size, gpu_enabled
        );

        Ok(DirectUniversalBus {
            handle,
            buffer_size,
            segment_count,
            gpu_enabled,
        })
    }

    /// Configure automatic scaling parameters
    fn configure_auto_scaling_internal(gpu_preferred: bool) -> Result<(), String> {
        let config = ScalingConfig {
            min_producers: 1,
            max_producers: 16,
            min_consumers: 1,
            max_consumers: 8,
            scale_threshold_percent: 75,
            scale_cooldown_ms: 1000,
            gpu_preferred,
            auto_balance_load: true,
        };

        let success = unsafe { configure_auto_scaling(&config) };
        if success {
            Ok(())
        } else {
            Err("Failed to configure auto-scaling".to_string())
        }
    }

    /// Send data to the bus
    /// 
    /// # Arguments
    /// * `data` - Data to send (any type that can be converted to bytes)
    /// * `type_id` - Type identifier for routing
    /// 
    /// # Example
    /// ```rust
    /// bus.send("Hello from Rust!", 1)?;
    /// bus.send(&[1, 2, 3, 4], 2)?;
    /// ```
    pub fn send<T: AsRef<[u8]>>(&self, data: T, type_id: u32) -> Result<(), String> {
        let bytes = data.as_ref();
        
        let udata = unsafe {
            create_universal_data(
                bytes.as_ptr() as *const c_void,
                bytes.len(),
                type_id,
                LanguageType::Rust,
            )
        };

        if udata.is_null() {
            return Err("Failed to create universal data".to_string());
        }

        let result = unsafe { umsbb_submit_direct(self.handle, udata) };
        unsafe { free_universal_data(udata) };

        if result {
            Ok(())
        } else {
            Err("Failed to submit data".to_string())
        }
    }

    /// Receive data from the bus
    /// 
    /// # Returns
    /// * `Some(Vec<u8>)` - Received data
    /// * `None` - No data available
    /// 
    /// # Example
    /// ```rust
    /// if let Some(data) = bus.receive() {
    ///     println!("Received: {:?}", data);
    /// }
    /// ```
    pub fn receive(&self) -> Option<Vec<u8>> {
        let udata_ptr = unsafe { umsbb_drain_direct(self.handle, LanguageType::Rust) };

        if udata_ptr.is_null() {
            return None;
        }

        let udata = unsafe { &*udata_ptr };
        let data_slice = unsafe { slice::from_raw_parts(udata.data as *const u8, udata.size) };
        let result = data_slice.to_vec();

        unsafe { free_universal_data(udata_ptr) };

        Some(result)
    }

    /// Send data and wait for a response
    /// 
    /// # Arguments
    /// * `data` - Data to send
    /// * `type_id` - Type identifier
    /// * `timeout_ms` - Timeout in milliseconds
    /// 
    /// # Returns
    /// * `Some(Vec<u8>)` - Response data
    /// * `None` - No response within timeout
    pub fn send_and_receive<T: AsRef<[u8]>>(
        &self,
        data: T,
        type_id: u32,
        timeout_ms: u64,
    ) -> Option<Vec<u8>> {
        if self.send(data, type_id).is_err() {
            return None;
        }

        let start = std::time::Instant::now();
        loop {
            if let Some(response) = self.receive() {
                return Some(response);
            }

            if start.elapsed().as_millis() >= timeout_ms as u128 {
                return None;
            }

            std::thread::sleep(std::time::Duration::from_micros(100));
        }
    }

    /// Get GPU capabilities information
    pub fn get_gpu_info(&self) -> GpuInfo {
        let caps = unsafe { get_gpu_capabilities() };
        let available = unsafe { gpu_available() };

        GpuInfo {
            available,
            has_cuda: caps.has_cuda,
            has_opencl: caps.has_opencl,
            has_compute: caps.has_compute,
            memory_size: caps.memory_size,
            compute_capability: caps.compute_capability,
            max_threads: caps.max_threads,
        }
    }

    /// Get current auto-scaling status
    pub fn get_scaling_status(&self) -> ScalingStatus {
        let optimal_producers = unsafe { get_optimal_producer_count() };
        let optimal_consumers = unsafe { get_optimal_consumer_count() };
        let gpu_info = self.get_gpu_info();

        ScalingStatus {
            optimal_producers,
            optimal_consumers,
            gpu_info,
        }
    }

    /// Trigger manual scale evaluation
    pub fn trigger_scale_evaluation(&self) {
        unsafe { trigger_scale_evaluation() };
    }
}

impl Drop for DirectUniversalBus {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe { umsbb_destroy_direct(self.handle) };
            self.handle = ptr::null_mut();
            println!("[Rust Direct] Bus destroyed");
        }
    }
}

// Helper structures for better Rust ergonomics
#[derive(Debug, Clone)]
pub struct GpuInfo {
    pub available: bool,
    pub has_cuda: bool,
    pub has_opencl: bool,
    pub has_compute: bool,
    pub memory_size: usize,
    pub compute_capability: i32,
    pub max_threads: usize,
}

#[derive(Debug, Clone)]
pub struct ScalingStatus {
    pub optimal_producers: u32,
    pub optimal_consumers: u32,
    pub gpu_info: GpuInfo,
}

/// Auto-scaling producer-consumer system for Rust
pub struct AutoScalingBus {
    bus: DirectUniversalBus,
    producers: Vec<std::thread::JoinHandle<()>>,
    consumers: Vec<std::thread::JoinHandle<()>>,
    shutdown: std::sync::Arc<std::sync::atomic::AtomicBool>,
}

impl AutoScalingBus {
    /// Create a new auto-scaling bus
    pub fn new(
        buffer_size: usize,
        segment_count: u32,
        gpu_preferred: bool,
    ) -> Result<Self, String> {
        let bus = DirectUniversalBus::new(buffer_size, segment_count, gpu_preferred, true)?;
        
        Ok(AutoScalingBus {
            bus,
            producers: Vec::new(),
            consumers: Vec::new(),
            shutdown: std::sync::Arc::new(std::sync::atomic::AtomicBool::new(false)),
        })
    }

    /// Start auto-scaling producers
    /// 
    /// # Arguments
    /// * `producer_fn` - Function that generates data
    /// * `count` - Number of producers (None = auto-determine)
    pub fn start_auto_producers<F>(&mut self, producer_fn: F, count: Option<u32>)
    where
        F: Fn(u32) -> Option<Vec<u8>> + Send + Sync + Clone + 'static,
    {
        let count = count.unwrap_or_else(|| self.bus.get_scaling_status().optimal_producers);

        for worker_id in 0..count {
            let bus_handle = unsafe { std::ptr::read(&self.bus.handle) };
            let producer_fn = producer_fn.clone();
            let shutdown = self.shutdown.clone();

            let producer = std::thread::spawn(move || {
                while !shutdown.load(std::sync::atomic::Ordering::Relaxed) {
                    if let Some(data) = producer_fn(worker_id) {
                        // Create a temporary bus instance for this thread
                        let temp_bus = DirectUniversalBus {
                            handle: bus_handle,
                            buffer_size: 0,
                            segment_count: 0,
                            gpu_enabled: false,
                        };
                        let _ = temp_bus.send(&data, worker_id);
                        std::mem::forget(temp_bus); // Don't drop the handle
                    }
                    std::thread::sleep(std::time::Duration::from_micros(100));
                }
            });

            self.producers.push(producer);
        }

        println!("Started {} auto-scaling producers", count);
    }

    /// Start auto-scaling consumers
    /// 
    /// # Arguments
    /// * `consumer_fn` - Function that processes data
    /// * `count` - Number of consumers (None = auto-determine)
    pub fn start_auto_consumers<F>(&mut self, consumer_fn: F, count: Option<u32>)
    where
        F: Fn(Vec<u8>, u32) + Send + Sync + Clone + 'static,
    {
        let count = count.unwrap_or_else(|| self.bus.get_scaling_status().optimal_consumers);

        for worker_id in 0..count {
            let bus_handle = unsafe { std::ptr::read(&self.bus.handle) };
            let consumer_fn = consumer_fn.clone();
            let shutdown = self.shutdown.clone();

            let consumer = std::thread::spawn(move || {
                while !shutdown.load(std::sync::atomic::Ordering::Relaxed) {
                    // Create a temporary bus instance for this thread
                    let temp_bus = DirectUniversalBus {
                        handle: bus_handle,
                        buffer_size: 0,
                        segment_count: 0,
                        gpu_enabled: false,
                    };
                    
                    if let Some(data) = temp_bus.receive() {
                        consumer_fn(data, worker_id);
                    } else {
                        std::thread::sleep(std::time::Duration::from_micros(100));
                    }
                    std::mem::forget(temp_bus); // Don't drop the handle
                }
            });

            self.consumers.push(consumer);
        }

        println!("Started {} auto-scaling consumers", count);
    }

    /// Stop all producers and consumers
    pub fn stop(&mut self) {
        self.shutdown.store(true, std::sync::atomic::Ordering::Relaxed);

        // Wait for all producers to finish
        while let Some(producer) = self.producers.pop() {
            let _ = producer.join();
        }

        // Wait for all consumers to finish
        while let Some(consumer) = self.consumers.pop() {
            let _ = consumer.join();
        }

        println!("[Rust AutoScale] Stopped all workers");
    }
}

impl Drop for AutoScalingBus {
    fn drop(&mut self) {
        self.stop();
    }
}

// Example usage and tests
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_direct_bus_creation() {
        let bus = DirectUniversalBus::new(1024 * 1024, 4, false, false);
        assert!(bus.is_ok());
    }

    #[test]
    fn test_send_receive() {
        let bus = DirectUniversalBus::new(1024 * 1024, 4, false, false).unwrap();
        
        // Send test data
        assert!(bus.send("Hello Rust!", 1).is_ok());
        assert!(bus.send(&[1, 2, 3, 4], 2).is_ok());
        
        // Receive data
        if let Some(data) = bus.receive() {
            println!("Received: {:?}", String::from_utf8_lossy(&data));
        }
    }

    #[test]
    fn test_gpu_info() {
        let bus = DirectUniversalBus::new(1024 * 1024, 4, true, false).unwrap();
        let gpu_info = bus.get_gpu_info();
        println!("GPU Info: {:?}", gpu_info);
    }
}

// Example usage
fn main() -> Result<(), String> {
    // Direct usage example
    let bus = DirectUniversalBus::new(1024 * 1024, 0, true, true)?;
    
    println!("GPU Info: {:?}", bus.get_gpu_info());
    println!("Scaling Status: {:?}", bus.get_scaling_status());
    
    // Send test data
    bus.send("Hello from Rust!", 1)?;
    bus.send(&[1, 2, 3, 4, 5], 2)?;
    
    // Receive data
    while let Some(data) = bus.receive() {
        println!("Received: {:?}", String::from_utf8_lossy(&data));
    }
    
    Ok(())
}