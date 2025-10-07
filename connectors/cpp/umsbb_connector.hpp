/*
 * C++ Connector for UMSBB WebAssembly Core
 * Direct memory binding without API overhead
 */

#ifndef UMSBB_CPP_CONNECTOR_HPP
#define UMSBB_CPP_CONNECTOR_HPP

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <stdexcept>
#include <cstdint>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/bind.h>
#else
// Mock WebAssembly interface for development
extern "C" {
    uint32_t umsbb_create_buffer(uint32_t size_mb);
    int umsbb_write_message(uint32_t handle, const void* data, uint32_t size);
    int umsbb_read_message(uint32_t handle, void* buffer, uint32_t buffer_size, uint32_t* actual_size);
    uint64_t umsbb_get_total_messages(uint32_t handle);
    uint64_t umsbb_get_total_bytes(uint32_t handle);
    uint32_t umsbb_get_pending_messages(uint32_t handle);
    int umsbb_destroy_buffer(uint32_t handle);
}
#endif

namespace umsbb {

enum class ErrorCode : int {
    SUCCESS = 0,
    INVALID_PARAMS = -1,
    BUFFER_FULL = -2,
    BUFFER_EMPTY = -3,
    INVALID_HANDLE = -4,
    MEMORY_ALLOCATION = -5,
    CORRUPTED_DATA = -6
};

class UMSBBException : public std::runtime_error {
public:
    explicit UMSBBException(const std::string& message, ErrorCode code = ErrorCode::INVALID_PARAMS)
        : std::runtime_error(message), error_code_(code) {}
    
    ErrorCode getErrorCode() const noexcept { return error_code_; }

private:
    ErrorCode error_code_;
};

struct BufferStats {
    uint64_t total_messages{0};
    uint64_t total_bytes{0};
    uint32_t pending_messages{0};
    uint32_t active_segments{0};
};

class UMSBBBuffer {
public:
    explicit UMSBBBuffer(uint32_t size_mb = 16) : handle_(0) {
        if (size_mb < 1 || size_mb > 64) {
            throw UMSBBException("Buffer size must be between 1 and 64 MB");
        }
        
        handle_ = umsbb_create_buffer(size_mb);
        if (handle_ == 0) {
            throw UMSBBException("Failed to create buffer", ErrorCode::MEMORY_ALLOCATION);
        }
    }

    ~UMSBBBuffer() {
        if (handle_ != 0) {
            umsbb_destroy_buffer(handle_);
        }
    }

    // Move constructor and assignment
    UMSBBBuffer(UMSBBBuffer&& other) noexcept : handle_(other.handle_) {
        other.handle_ = 0;
    }

    UMSBBBuffer& operator=(UMSBBBuffer&& other) noexcept {
        if (this != &other) {
            if (handle_ != 0) {
                umsbb_destroy_buffer(handle_);
            }
            handle_ = other.handle_;
            other.handle_ = 0;
        }
        return *this;
    }

    // Delete copy constructor and assignment
    UMSBBBuffer(const UMSBBBuffer&) = delete;
    UMSBBBuffer& operator=(const UMSBBBuffer&) = delete;

    void write(const std::vector<uint8_t>& data) {
        write(data.data(), data.size());
    }

    void write(const std::string& data) {
        write(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
    }

    void write(const uint8_t* data, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (size > 65536) { // 64KB max
            throw UMSBBException("Message too large (max 64KB)", ErrorCode::INVALID_PARAMS);
        }

        int result = umsbb_write_message(handle_, data, static_cast<uint32_t>(size));
        if (result != static_cast<int>(ErrorCode::SUCCESS)) {
            throw UMSBBException(getErrorString(static_cast<ErrorCode>(result)), 
                                static_cast<ErrorCode>(result));
        }
    }

    std::vector<uint8_t> read() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<uint8_t> buffer(65536); // 64KB buffer
        uint32_t actual_size = 0;
        
        int result = umsbb_read_message(handle_, buffer.data(), 
                                       static_cast<uint32_t>(buffer.size()), &actual_size);
        
        if (result == static_cast<int>(ErrorCode::BUFFER_EMPTY)) {
            return {}; // Empty vector indicates no data
        } else if (result != static_cast<int>(ErrorCode::SUCCESS)) {
            throw UMSBBException(getErrorString(static_cast<ErrorCode>(result)),
                                static_cast<ErrorCode>(result));
        }
        
        buffer.resize(actual_size);
        return buffer;
    }

    std::string readString() {
        auto data = read();
        if (data.empty()) {
            return "";
        }
        return std::string(data.begin(), data.end());
    }

    BufferStats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        BufferStats stats;
        stats.total_messages = umsbb_get_total_messages(handle_);
        stats.total_bytes = umsbb_get_total_bytes(handle_);
        stats.pending_messages = umsbb_get_pending_messages(handle_);
        // TODO: Implement active_segments in core
        
        return stats;
    }

    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return umsbb_get_pending_messages(handle_) == 0;
    }

    uint32_t pendingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return umsbb_get_pending_messages(handle_);
    }

private:
    static std::string getErrorString(ErrorCode code) {
        switch (code) {
            case ErrorCode::SUCCESS: return "Success";
            case ErrorCode::INVALID_PARAMS: return "Invalid parameters";
            case ErrorCode::BUFFER_FULL: return "Buffer is full";
            case ErrorCode::BUFFER_EMPTY: return "Buffer is empty";
            case ErrorCode::INVALID_HANDLE: return "Invalid buffer handle";
            case ErrorCode::MEMORY_ALLOCATION: return "Memory allocation failed";
            case ErrorCode::CORRUPTED_DATA: return "Corrupted data detected";
            default: return "Unknown error";
        }
    }

    uint32_t handle_;
    mutable std::mutex mutex_;
};

// Convenience function
inline std::unique_ptr<UMSBBBuffer> createBuffer(uint32_t size_mb = 16) {
    return std::make_unique<UMSBBBuffer>(size_mb);
}

// Performance test utility
class PerformanceTest {
public:
    static void run(uint32_t message_count = 10000, uint32_t buffer_size_mb = 32) {
        std::cout << "UMSBB C++ Connector Test\n";
        std::cout << std::string(40, '=') << "\n";

        auto buffer = createBuffer(buffer_size_mb);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Producer thread
        std::thread producer([&buffer, message_count]() {
            for (uint32_t i = 0; i < message_count; ++i) {
                std::string message = "Message " + std::to_string(i);
                buffer->write(message);
                
                if (i % 1000 == 0) {
                    std::cout << "Produced " << i << " messages\n";
                }
            }
        });
        
        // Consumer thread
        std::thread consumer([&buffer, message_count]() {
            uint32_t received = 0;
            while (received < message_count) {
                auto message = buffer->read();
                if (!message.empty()) {
                    received++;
                    if (received % 1000 == 0) {
                        std::cout << "Consumed " << received << " messages\n";
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        });
        
        producer.join();
        consumer.join();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        double duration_sec = duration.count() / 1000.0;
        
        auto stats = buffer->getStats();
        
        std::cout << "\nTest Results:\n";
        std::cout << "Duration: " << std::fixed << std::setprecision(3) << duration_sec << " seconds\n";
        std::cout << "Messages: " << stats.total_messages << "\n";
        std::cout << "Bytes: " << stats.total_bytes << "\n";
        std::cout << "Messages/sec: " << static_cast<uint64_t>(stats.total_messages / duration_sec) << "\n";
        std::cout << "MB/sec: " << std::fixed << std::setprecision(2) 
                  << (stats.total_bytes / (1024.0 * 1024.0) / duration_sec) << "\n";
    }
};

} // namespace umsbb

#ifdef EMSCRIPTEN
// Emscripten bindings for JavaScript interop
EMSCRIPTEN_BINDINGS(umsbb_module) {
    emscripten::register_vector<uint8_t>("vector<uint8_t>");
    
    emscripten::value_object<umsbb::BufferStats>("BufferStats")
        .field("totalMessages", &umsbb::BufferStats::total_messages)
        .field("totalBytes", &umsbb::BufferStats::total_bytes)
        .field("pendingMessages", &umsbb::BufferStats::pending_messages)
        .field("activeSegments", &umsbb::BufferStats::active_segments);
    
    emscripten::class_<umsbb::UMSBBBuffer>("UMSBBBuffer")
        .constructor<uint32_t>()
        .function("writeVector", emscripten::select_overload<void(const std::vector<uint8_t>&)>(&umsbb::UMSBBBuffer::write))
        .function("writeString", emscripten::select_overload<void(const std::string&)>(&umsbb::UMSBBBuffer::write))
        .function("read", &umsbb::UMSBBBuffer::read)
        .function("readString", &umsbb::UMSBBBuffer::readString)
        .function("getStats", &umsbb::UMSBBBuffer::getStats)
        .function("isEmpty", &umsbb::UMSBBBuffer::isEmpty)
        .function("pendingCount", &umsbb::UMSBBBuffer::pendingCount);
    
    emscripten::function("createBuffer", &umsbb::createBuffer, emscripten::allow_raw_pointers());
    
    emscripten::class_<umsbb::UMSBBException>("UMSBBException")
        .function("what", &umsbb::UMSBBException::what);
}
#endif

#endif // UMSBB_CPP_CONNECTOR_HPP