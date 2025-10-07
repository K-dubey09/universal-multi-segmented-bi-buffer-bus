#ifndef WEB_CONTROLLER_H
#define WEB_CONTROLLER_H

#include "gpu_accelerated_buffer.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CLIENTS 100
#define WEB_BUFFER_SIZE 65536
#define DEFAULT_WEB_PORT 8080
#define WEBSOCKET_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// Web client structure
typedef struct {
    int socket_fd;
    bool is_websocket;
    bool is_active;
    uint64_t last_activity;
    char buffer[WEB_BUFFER_SIZE];
    size_t buffer_length;
    uint32_t client_id;
} web_client_t;

// Web server structure
typedef struct {
    int server_socket;
    uint16_t port;
    bool is_running;
    web_client_t clients[MAX_CLIENTS];
    uint32_t active_clients;
    
    // Reference to GPU buffer for monitoring
    gpu_accelerated_buffer_t* gpu_buffer;
    
    // Performance data cache
    gpu_performance_metrics_t last_metrics;
    uint64_t last_metrics_update;
    
    // Control commands
    bool pause_processing;
    bool reset_counters;
    uint32_t target_throughput_gbps;
    
} web_server_t;

// JSON response structure
typedef struct {
    char* json_data;
    size_t json_length;
    size_t json_capacity;
} json_response_t;

// Web API endpoints
typedef enum {
    WEB_ENDPOINT_STATUS = 0,
    WEB_ENDPOINT_METRICS = 1,
    WEB_ENDPOINT_CONTROL = 2,
    WEB_ENDPOINT_DASHBOARD = 3,
    WEB_ENDPOINT_WEBSOCKET = 4
} web_endpoint_t;

// Function declarations

// Web server lifecycle
int web_server_init(web_server_t* server, uint16_t port, gpu_accelerated_buffer_t* gpu_buffer);
int web_server_start(web_server_t* server);
void web_server_stop(web_server_t* server);
void web_server_cleanup(web_server_t* server);

// Client management
int web_accept_client(web_server_t* server);
void web_disconnect_client(web_server_t* server, uint32_t client_id);
int web_handle_client_request(web_server_t* server, uint32_t client_id);

// HTTP handling
int web_parse_http_request(const char* request, char* method, char* path, char* headers);
int web_send_http_response(int socket_fd, int status_code, const char* content_type, const char* body, size_t body_length);
int web_send_websocket_frame(int socket_fd, const char* data, size_t length);

// WebSocket handling
bool web_is_websocket_upgrade(const char* headers);
int web_perform_websocket_handshake(int socket_fd, const char* headers);
int web_handle_websocket_frame(web_server_t* server, uint32_t client_id, const char* data, size_t length);

// API endpoints
int web_handle_status_request(web_server_t* server, json_response_t* response);
int web_handle_metrics_request(web_server_t* server, json_response_t* response);
int web_handle_control_request(web_server_t* server, const char* post_data, json_response_t* response);
int web_handle_dashboard_request(web_server_t* server, char** html_content, size_t* content_length);

// JSON utilities
json_response_t* json_create_response(void);
void json_free_response(json_response_t* response);
int json_add_string(json_response_t* response, const char* key, const char* value);
int json_add_number(json_response_t* response, const char* key, double value);
int json_add_integer(json_response_t* response, const char* key, uint64_t value);
int json_add_boolean(json_response_t* response, const char* key, bool value);
int json_finalize(json_response_t* response);

// Real-time data broadcasting
int web_broadcast_metrics(web_server_t* server);
int web_update_performance_cache(web_server_t* server);

// Utility functions
const char* web_get_mime_type(const char* path);
char* web_url_decode(const char* encoded);
uint64_t web_get_current_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif // WEB_CONTROLLER_H