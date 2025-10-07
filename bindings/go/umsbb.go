// Universal Multi-Segmented Bi-Buffer Bus - Go Direct Binding
// No API wrapper - Direct FFI connection with auto-scaling and GPU support

package umsbb

/*
#cgo CFLAGS: -I../../include
#cgo LDFLAGS: -L../../lib -luniversal_multi_segmented_bi_buffer_bus

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Language types
typedef enum {
    LANG_C = 0,
    LANG_CPP,
    LANG_PYTHON,
    LANG_JAVASCRIPT,
    LANG_RUST,
    LANG_GO,
    LANG_JAVA,
    LANG_CSHARP,
    LANG_KOTLIN,
    LANG_SWIFT
} language_type_t;

// Universal data structure
typedef struct {
    void* data;
    size_t size;
    uint32_t type_id;
    language_type_t source_lang;
} universal_data_t;

// Scaling configuration
typedef struct {
    uint32_t min_producers;
    uint32_t max_producers;
    uint32_t min_consumers;
    uint32_t max_consumers;
    uint32_t scale_threshold_percent;
    uint32_t scale_cooldown_ms;
    bool gpu_preferred;
    bool auto_balance_load;
} scaling_config_t;

// GPU capabilities
typedef struct {
    bool has_cuda;
    bool has_opencl;
    bool has_compute;
    bool has_memory_pool;
    size_t memory_size;
    int compute_capability;
    size_t max_threads;
} gpu_capabilities_t;

// Core functions
void* umsbb_create_direct(size_t buffer_size, uint32_t segment_count, language_type_t lang);
bool umsbb_submit_direct(void* handle, const universal_data_t* data);
universal_data_t* umsbb_drain_direct(void* handle, language_type_t target_lang);
void umsbb_destroy_direct(void* handle);

// GPU functions
bool initialize_gpu();
bool gpu_available();
gpu_capabilities_t get_gpu_capabilities();

// Scaling functions
bool configure_auto_scaling(const scaling_config_t* config);
uint32_t get_optimal_producer_count();
uint32_t get_optimal_consumer_count();
void trigger_scale_evaluation();

// Memory management
universal_data_t* create_universal_data(void* data, size_t size, uint32_t type_id, language_type_t lang);
void free_universal_data(universal_data_t* data);
*/
import "C"

import (
	"errors"
	"fmt"
	"runtime"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"
)

// LanguageType represents the supported language types
type LanguageType int

const (
	LangC LanguageType = iota
	LangCPP
	LangPython
	LangJavaScript
	LangRust
	LangGo
	LangJava
	LangCSharp
	LangKotlin
	LangSwift
)

// UniversalData represents cross-language data
type UniversalData struct {
	Data       []byte
	TypeID     uint32
	SourceLang LanguageType
}

// ScalingConfig represents auto-scaling configuration
type ScalingConfig struct {
	MinProducers          uint32
	MaxProducers          uint32
	MinConsumers          uint32
	MaxConsumers          uint32
	ScaleThresholdPercent uint32
	ScaleCooldownMs       uint32
	GPUPreferred          bool
	AutoBalanceLoad       bool
}

// GPUInfo represents GPU capabilities
type GPUInfo struct {
	Available         bool
	HasCUDA           bool
	HasOpenCL         bool
	HasCompute        bool
	MemorySize        uint64
	ComputeCapability int
	MaxThreads        uint64
}

// ScalingStatus represents current scaling status
type ScalingStatus struct {
	OptimalProducers uint32
	OptimalConsumers uint32
	GPUInfo          GPUInfo
}

// DirectUniversalBus provides direct access to the Universal Multi-Segmented Bi-Buffer Bus
type DirectUniversalBus struct {
	handle       unsafe.Pointer
	bufferSize   uint64
	segmentCount uint32
	gpuEnabled   bool
	mu           sync.RWMutex
}

// NewDirectUniversalBus creates a new Direct Universal Bus
//
// Parameters:
//   - bufferSize: Size of each buffer segment (default: 1MB)
//   - segmentCount: Number of segments (0 = auto-determine)
//   - gpuPreferred: Prefer GPU processing for large operations
//   - autoScale: Enable automatic scaling
//
// Example:
//
//	bus, err := umsbb.NewDirectUniversalBus(1024*1024, 0, true, true)
//	if err != nil {
//	    log.Fatal(err)
//	}
//	defer bus.Close()
func NewDirectUniversalBus(bufferSize uint64, segmentCount uint32, gpuPreferred, autoScale bool) (*DirectUniversalBus, error) {
	if autoScale {
		if err := configureAutoScalingInternal(gpuPreferred); err != nil {
			return nil, fmt.Errorf("failed to configure auto-scaling: %w", err)
		}
	}

	handle := C.umsbb_create_direct(C.size_t(bufferSize), C.uint32_t(segmentCount), C.LANG_GO)
	if handle == nil {
		return nil, errors.New("failed to create Universal Bus")
	}

	gpuEnabled := false
	if gpuPreferred {
		gpuEnabled = bool(C.initialize_gpu())
	}

	bus := &DirectUniversalBus{
		handle:       handle,
		bufferSize:   bufferSize,
		segmentCount: segmentCount,
		gpuEnabled:   gpuEnabled,
	}

	// Set finalizer to ensure cleanup
	runtime.SetFinalizer(bus, (*DirectUniversalBus).Close)

	fmt.Printf("[Go Direct] Bus created with %d byte segments, GPU: %t\n", bufferSize, gpuEnabled)
	return bus, nil
}

// configureAutoScalingInternal configures automatic scaling parameters
func configureAutoScalingInternal(gpuPreferred bool) error {
	config := C.scaling_config_t{
		min_producers:            1,
		max_producers:            16,
		min_consumers:            1,
		max_consumers:            8,
		scale_threshold_percent:  75,
		scale_cooldown_ms:        1000,
		gpu_preferred:            C.bool(gpuPreferred),
		auto_balance_load:        C.bool(true),
	}

	if !bool(C.configure_auto_scaling(&config)) {
		return errors.New("failed to configure auto-scaling")
	}
	return nil
}

// Send sends data to the bus
//
// Parameters:
//   - data: Data to send (any byte slice)
//   - typeID: Type identifier for routing
//
// Example:
//
//	err := bus.Send([]byte("Hello from Go!"), 1)
//	if err != nil {
//	    log.Printf("Send failed: %v", err)
//	}
func (b *DirectUniversalBus) Send(data []byte, typeID uint32) error {
	b.mu.RLock()
	defer b.mu.RUnlock()

	if b.handle == nil {
		return errors.New("bus is closed")
	}

	if len(data) == 0 {
		return errors.New("data cannot be empty")
	}

	// Create C data pointer
	cData := C.malloc(C.size_t(len(data)))
	if cData == nil {
		return errors.New("memory allocation failed")
	}
	defer C.free(cData)

	// Copy Go data to C memory
	C.memcpy(cData, unsafe.Pointer(&data[0]), C.size_t(len(data)))

	// Create universal data structure
	udata := C.create_universal_data(cData, C.size_t(len(data)), C.uint32_t(typeID), C.LANG_GO)
	if udata == nil {
		return errors.New("failed to create universal data")
	}
	defer C.free_universal_data(udata)

	// Submit data
	if !bool(C.umsbb_submit_direct(b.handle, udata)) {
		return errors.New("failed to submit data")
	}

	return nil
}

// Receive receives data from the bus
//
// Returns:
//   - data: Received data as byte slice, or nil if nothing available
//   - error: Error if any
//
// Example:
//
//	data, err := bus.Receive()
//	if err != nil {
//	    log.Printf("Receive failed: %v", err)
//	} else if data != nil {
//	    fmt.Printf("Received: %s\n", string(data))
//	}
func (b *DirectUniversalBus) Receive() ([]byte, error) {
	b.mu.RLock()
	defer b.mu.RUnlock()

	if b.handle == nil {
		return nil, errors.New("bus is closed")
	}

	udataPtr := C.umsbb_drain_direct(b.handle, C.LANG_GO)
	if udataPtr == nil {
		return nil, nil // No data available
	}
	defer C.free_universal_data(udataPtr)

	// Extract data from universal data structure
	udata := *udataPtr
	if udata.data == nil || udata.size == 0 {
		return nil, nil
	}

	// Copy C data to Go slice
	result := make([]byte, udata.size)
	C.memcpy(unsafe.Pointer(&result[0]), udata.data, udata.size)

	return result, nil
}

// SendAndReceive sends data and waits for a response
//
// Parameters:
//   - data: Data to send
//   - typeID: Type identifier
//   - timeoutMs: Timeout in milliseconds
//
// Returns:
//   - response: Response data, or nil if no response within timeout
//   - error: Error if any
//
// Example:
//
//	response, err := bus.SendAndReceive([]byte("ping"), 1, 5000)
//	if err != nil {
//	    log.Printf("SendAndReceive failed: %v", err)
//	} else if response != nil {
//	    fmt.Printf("Response: %s\n", string(response))
//	}
func (b *DirectUniversalBus) SendAndReceive(data []byte, typeID uint32, timeoutMs uint64) ([]byte, error) {
	if err := b.Send(data, typeID); err != nil {
		return nil, err
	}

	start := time.Now()
	for {
		response, err := b.Receive()
		if err != nil {
			return nil, err
		}
		if response != nil {
			return response, nil
		}

		if time.Since(start).Milliseconds() >= int64(timeoutMs) {
			return nil, nil // Timeout
		}

		time.Sleep(100 * time.Microsecond)
	}
}

// GetGPUInfo returns GPU capabilities information
func (b *DirectUniversalBus) GetGPUInfo() GPUInfo {
	caps := C.get_gpu_capabilities()
	available := bool(C.gpu_available())

	return GPUInfo{
		Available:         available,
		HasCUDA:           bool(caps.has_cuda),
		HasOpenCL:         bool(caps.has_opencl),
		HasCompute:        bool(caps.has_compute),
		MemorySize:        uint64(caps.memory_size),
		ComputeCapability: int(caps.compute_capability),
		MaxThreads:        uint64(caps.max_threads),
	}
}

// GetScalingStatus returns current auto-scaling status
func (b *DirectUniversalBus) GetScalingStatus() ScalingStatus {
	optimalProducers := uint32(C.get_optimal_producer_count())
	optimalConsumers := uint32(C.get_optimal_consumer_count())
	gpuInfo := b.GetGPUInfo()

	return ScalingStatus{
		OptimalProducers: optimalProducers,
		OptimalConsumers: optimalConsumers,
		GPUInfo:          gpuInfo,
	}
}

// TriggerScaleEvaluation triggers manual scale evaluation
func (b *DirectUniversalBus) TriggerScaleEvaluation() {
	C.trigger_scale_evaluation()
}

// Close closes the bus and cleanup resources
func (b *DirectUniversalBus) Close() error {
	b.mu.Lock()
	defer b.mu.Unlock()

	if b.handle != nil {
		C.umsbb_destroy_direct(b.handle)
		b.handle = nil
		runtime.SetFinalizer(b, nil)
		fmt.Println("[Go Direct] Bus closed")
	}
	return nil
}

// AutoScalingBus provides auto-scaling producer-consumer system
type AutoScalingBus struct {
	bus       *DirectUniversalBus
	producers []chan struct{}
	consumers []chan struct{}
	shutdown  int32
	wg        sync.WaitGroup
}

// NewAutoScalingBus creates a new auto-scaling bus
func NewAutoScalingBus(bufferSize uint64, segmentCount uint32, gpuPreferred bool) (*AutoScalingBus, error) {
	bus, err := NewDirectUniversalBus(bufferSize, segmentCount, gpuPreferred, true)
	if err != nil {
		return nil, err
	}

	return &AutoScalingBus{
		bus:       bus,
		producers: make([]chan struct{}, 0),
		consumers: make([]chan struct{}, 0),
		shutdown:  0,
	}, nil
}

// StartAutoProducers starts auto-scaling producers
//
// Parameters:
//   - producerFunc: Function that generates data
//   - count: Number of producers (0 = auto-determine)
//
// Example:
//
//	bus.StartAutoProducers(func(workerID uint32) []byte {
//	    return []byte(fmt.Sprintf("Message from producer %d", workerID))
//	}, 0)
func (ab *AutoScalingBus) StartAutoProducers(producerFunc func(uint32) []byte, count uint32) {
	if count == 0 {
		count = ab.bus.GetScalingStatus().OptimalProducers
	}

	for i := uint32(0); i < count; i++ {
		stopCh := make(chan struct{})
		ab.producers = append(ab.producers, stopCh)

		ab.wg.Add(1)
		go func(workerID uint32, stop <-chan struct{}) {
			defer ab.wg.Done()
			
			ticker := time.NewTicker(100 * time.Microsecond)
			defer ticker.Stop()

			for {
				select {
				case <-stop:
					return
				case <-ticker.C:
					if atomic.LoadInt32(&ab.shutdown) != 0 {
						return
					}

					data := producerFunc(workerID)
					if data != nil {
						_ = ab.bus.Send(data, workerID)
					}
				}
			}
		}(i, stopCh)
	}

	fmt.Printf("Started %d auto-scaling producers\n", count)
}

// StartAutoConsumers starts auto-scaling consumers
//
// Parameters:
//   - consumerFunc: Function that processes data
//   - count: Number of consumers (0 = auto-determine)
//
// Example:
//
//	bus.StartAutoConsumers(func(data []byte, workerID uint32) {
//	    fmt.Printf("Consumer %d received: %s\n", workerID, string(data))
//	}, 0)
func (ab *AutoScalingBus) StartAutoConsumers(consumerFunc func([]byte, uint32), count uint32) {
	if count == 0 {
		count = ab.bus.GetScalingStatus().OptimalConsumers
	}

	for i := uint32(0); i < count; i++ {
		stopCh := make(chan struct{})
		ab.consumers = append(ab.consumers, stopCh)

		ab.wg.Add(1)
		go func(workerID uint32, stop <-chan struct{}) {
			defer ab.wg.Done()
			
			ticker := time.NewTicker(100 * time.Microsecond)
			defer ticker.Stop()

			for {
				select {
				case <-stop:
					return
				case <-ticker.C:
					if atomic.LoadInt32(&ab.shutdown) != 0 {
						return
					}

					data, err := ab.bus.Receive()
					if err == nil && data != nil {
						consumerFunc(data, workerID)
					}
				}
			}
		}(i, stopCh)
	}

	fmt.Printf("Started %d auto-scaling consumers\n", count)
}

// Stop stops all producers and consumers
func (ab *AutoScalingBus) Stop() {
	atomic.StoreInt32(&ab.shutdown, 1)

	// Stop all producers
	for _, stopCh := range ab.producers {
		close(stopCh)
	}
	ab.producers = ab.producers[:0]

	// Stop all consumers
	for _, stopCh := range ab.consumers {
		close(stopCh)
	}
	ab.consumers = ab.consumers[:0]

	ab.wg.Wait()
	fmt.Println("[Go AutoScale] Stopped all workers")
}

// Close closes the auto-scaling bus
func (ab *AutoScalingBus) Close() error {
	ab.Stop()
	return ab.bus.Close()
}

// Example usage
func ExampleUsage() {
	// Direct usage example
	bus, err := NewDirectUniversalBus(1024*1024, 0, true, true)
	if err != nil {
		fmt.Printf("Error creating bus: %v\n", err)
		return
	}
	defer bus.Close()

	fmt.Printf("GPU Info: %+v\n", bus.GetGPUInfo())
	fmt.Printf("Scaling Status: %+v\n", bus.GetScalingStatus())

	// Send test data
	_ = bus.Send([]byte("Hello from Go!"), 1)
	_ = bus.Send([]byte{1, 2, 3, 4, 5}, 2)

	// Receive data
	for {
		data, err := bus.Receive()
		if err != nil {
			fmt.Printf("Receive error: %v\n", err)
			break
		}
		if data == nil {
			break // No more data
		}
		fmt.Printf("Received: %s\n", string(data))
	}
}

// Benchmark functions for performance testing
func BenchmarkSend(data []byte, iterations int) time.Duration {
	bus, err := NewDirectUniversalBus(1024*1024, 8, true, false)
	if err != nil {
		return 0
	}
	defer bus.Close()

	start := time.Now()
	for i := 0; i < iterations; i++ {
		_ = bus.Send(data, uint32(i%256))
	}
	return time.Since(start)
}

func BenchmarkReceive(iterations int) time.Duration {
	bus, err := NewDirectUniversalBus(1024*1024, 8, true, false)
	if err != nil {
		return 0
	}
	defer bus.Close()

	// Pre-populate with data
	testData := []byte("benchmark test message")
	for i := 0; i < iterations; i++ {
		_ = bus.Send(testData, uint32(i%256))
	}

	start := time.Now()
	received := 0
	for received < iterations {
		data, _ := bus.Receive()
		if data != nil {
			received++
		}
	}
	return time.Since(start)
}