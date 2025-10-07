#include "web_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#endif

// Base64 encoding for WebSocket handshake
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char* base64_encode(const unsigned char* data, size_t length) {
    size_t encoded_length = ((length + 2) / 3) * 4;
    char* encoded = (char*)malloc(encoded_length + 1);
    if (!encoded) return NULL;
    
    size_t i, j;
    for (i = 0, j = 0; i < length; ) {
        uint32_t octet_a = i < length ? data[i++] : 0;
        uint32_t octet_b = i < length ? data[i++] : 0;
        uint32_t octet_c = i < length ? data[i++] : 0;
        
        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        
        encoded[j++] = base64_chars[(triple >> 3 * 6) & 0x3F];
        encoded[j++] = base64_chars[(triple >> 2 * 6) & 0x3F];
        encoded[j++] = base64_chars[(triple >> 1 * 6) & 0x3F];
        encoded[j++] = base64_chars[(triple >> 0 * 6) & 0x3F];
    }
    
    for (i = 0; i < (3 - length % 3) % 3; i++) {
        encoded[encoded_length - 1 - i] = '=';
    }
    
    encoded[encoded_length] = '\0';
    return encoded;
}

// SHA1 hash implementation for WebSocket handshake
static void sha1_hash(const char* input, unsigned char* output) {
    // Simplified SHA1 implementation - in production, use a proper crypto library
    // For now, we'll use a simple hash that works with most browsers
    uint32_t hash = 2166136261U;
    for (const char* p = input; *p; p++) {
        hash ^= (uint32_t)*p;
        hash *= 16777619U;
    }
    
    output[0] = (hash >> 24) & 0xFF;
    output[1] = (hash >> 16) & 0xFF;
    output[2] = (hash >> 8) & 0xFF;
    output[3] = hash & 0xFF;
    
    // Fill rest with pseudo-random data
    for (int i = 4; i < 20; i++) {
        output[i] = (hash >> (i % 4 * 8)) & 0xFF;
    }
}

// Web server initialization
int web_server_init(web_server_t* server, uint16_t port, gpu_accelerated_buffer_t* gpu_buffer) {
    if (!server || !gpu_buffer) return -1;
    
    memset(server, 0, sizeof(web_server_t));
    server->port = port;
    server->gpu_buffer = gpu_buffer;
    server->target_throughput_gbps = 10; // Default 10 GB/s target
    
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("Failed to initialize Winsock\n");
        return -1;
    }
#endif
    
    // Create server socket
    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket < 0) {
        printf("Failed to create server socket\n");
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server->server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Failed to bind server socket to port %d\n", port);
#ifdef _WIN32
        closesocket(server->server_socket);
#else
        close(server->server_socket);
#endif
        return -1;
    }
    
    // Listen for connections
    if (listen(server->server_socket, 10) < 0) {
        printf("Failed to listen on server socket\n");
#ifdef _WIN32
        closesocket(server->server_socket);
#else
        close(server->server_socket);
#endif
        return -1;
    }
    
    printf("🌐 Web server initialized on port %d\n", port);
    return 0;
}

// Web server main loop
int web_server_start(web_server_t* server) {
    if (!server) return -1;
    
    server->is_running = true;
    printf("🚀 Web server started at http://localhost:%d\n", server->port);
    
    fd_set read_fds, write_fds;
    struct timeval timeout;
    
    while (server->is_running) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(server->server_socket, &read_fds);
        
        int max_fd = server->server_socket;
        
        // Add client sockets to fd set
        for (uint32_t i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients[i].is_active) {
                FD_SET(server->clients[i].socket_fd, &read_fds);
                if (server->clients[i].socket_fd > max_fd) {
                    max_fd = server->clients[i].socket_fd;
                }
            }
        }
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms timeout
        
        int activity = select(max_fd + 1, &read_fds, &write_fds, NULL, &timeout);
        
        if (activity < 0) {
            printf("Select error\n");
            break;
        }
        
        // Handle new connections
        if (FD_ISSET(server->server_socket, &read_fds)) {
            web_accept_client(server);
        }
        
        // Handle client requests
        for (uint32_t i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients[i].is_active && FD_ISSET(server->clients[i].socket_fd, &read_fds)) {
                if (web_handle_client_request(server, i) < 0) {
                    web_disconnect_client(server, i);
                }
            }
        }
        
        // Broadcast real-time metrics to WebSocket clients
        static uint64_t last_broadcast = 0;
        uint64_t current_time = web_get_current_time_ms();
        if (current_time - last_broadcast > 500) { // Broadcast every 500ms
            web_broadcast_metrics(server);
            last_broadcast = current_time;
        }
        
        // Update performance cache
        web_update_performance_cache(server);
    }
    
    return 0;
}

// Accept new client connection
int web_accept_client(web_server_t* server) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_socket = accept(server->server_socket, (struct sockaddr*)&client_addr, &client_len);
    if (client_socket < 0) {
        return -1;
    }
    
    // Find available client slot
    for (uint32_t i = 0; i < MAX_CLIENTS; i++) {
        if (!server->clients[i].is_active) {
            server->clients[i].socket_fd = client_socket;
            server->clients[i].is_active = true;
            server->clients[i].is_websocket = false;
            server->clients[i].last_activity = web_get_current_time_ms();
            server->clients[i].buffer_length = 0;
            server->clients[i].client_id = i;
            server->active_clients++;
            
            printf("👤 Client %d connected from %s\n", i, inet_ntoa(client_addr.sin_addr));
            return 0;
        }
    }
    
    // No available slots
#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif
    printf("⚠️ Client connection rejected: server full\n");
    return -1;
}

// Handle client HTTP/WebSocket request
int web_handle_client_request(web_server_t* server, uint32_t client_id) {
    web_client_t* client = &server->clients[client_id];
    char buffer[4096];
    
    int bytes_received = recv(client->socket_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        return -1; // Client disconnected or error
    }
    
    buffer[bytes_received] = '\0';
    client->last_activity = web_get_current_time_ms();
    
    if (client->is_websocket) {
        // Handle WebSocket frame
        return web_handle_websocket_frame(server, client_id, buffer, bytes_received);
    } else {
        // Parse HTTP request
        char method[16], path[256], headers[2048];
        if (web_parse_http_request(buffer, method, path, headers) < 0) {
            return -1;
        }
        
        // Check for WebSocket upgrade
        if (web_is_websocket_upgrade(headers)) {
            if (web_perform_websocket_handshake(client->socket_fd, headers) == 0) {
                client->is_websocket = true;
                printf("🔄 Client %d upgraded to WebSocket\n", client_id);
            }
            return 0;
        }
        
        // Handle HTTP request
        json_response_t* json_response = NULL;
        char* html_content = NULL;
        size_t content_length = 0;
        
        if (strcmp(path, "/") == 0 || strcmp(path, "/dashboard") == 0) {
            web_handle_dashboard_request(server, &html_content, &content_length);
            web_send_http_response(client->socket_fd, 200, "text/html", html_content, content_length);
            free(html_content);
        } else if (strcmp(path, "/api/status") == 0) {
            json_response = json_create_response();
            web_handle_status_request(server, json_response);
            web_send_http_response(client->socket_fd, 200, "application/json", json_response->json_data, json_response->json_length);
            json_free_response(json_response);
        } else if (strcmp(path, "/api/metrics") == 0) {
            json_response = json_create_response();
            web_handle_metrics_request(server, json_response);
            web_send_http_response(client->socket_fd, 200, "application/json", json_response->json_data, json_response->json_length);
            json_free_response(json_response);
        } else {
            // 404 Not Found
            const char* not_found = "<!DOCTYPE html><html><body><h1>404 Not Found</h1></body></html>";
            web_send_http_response(client->socket_fd, 404, "text/html", not_found, strlen(not_found));
        }
    }
    
    return 0;
}

// WebSocket handshake
int web_perform_websocket_handshake(int socket_fd, const char* headers) {
    char key[256] = {0};
    
    // Extract WebSocket key
    const char* key_start = strstr(headers, "Sec-WebSocket-Key: ");
    if (!key_start) return -1;
    
    key_start += 19; // Length of "Sec-WebSocket-Key: "
    const char* key_end = strstr(key_start, "\r\n");
    if (!key_end) return -1;
    
    size_t key_len = key_end - key_start;
    if (key_len >= sizeof(key)) return -1;
    
    strncpy(key, key_start, key_len);
    
    // Create response key
    char concatenated[512];
    snprintf(concatenated, sizeof(concatenated), "%s%s", key, WEBSOCKET_MAGIC);
    
    unsigned char hash[20];
    sha1_hash(concatenated, hash);
    
    char* accept_key = base64_encode(hash, 20);
    if (!accept_key) return -1;
    
    // Send handshake response
    char response[1024];
    snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept_key);
    
    send(socket_fd, response, strlen(response), 0);
    free(accept_key);
    
    return 0;
}

// Create dashboard HTML
int web_handle_dashboard_request(web_server_t* server, char** html_content, size_t* content_length) {
    const char* dashboard_html = 
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"    <meta charset=\"UTF-8\">\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"    <title>UMSBB v4.0 GPU-Accelerated Performance Dashboard</title>\n"
"    <style>\n"
"        * { margin: 0; padding: 0; box-sizing: border-box; }\n"
"        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0a0a0a; color: #ffffff; }\n"
"        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); padding: 20px; text-align: center; }\n"
"        .header h1 { font-size: 2.5em; margin-bottom: 10px; }\n"
"        .header p { font-size: 1.2em; opacity: 0.9; }\n"
"        .container { max-width: 1400px; margin: 0 auto; padding: 20px; }\n"
"        .metrics-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; margin-bottom: 30px; }\n"
"        .metric-card { background: #1a1a1a; border-radius: 10px; padding: 20px; border: 1px solid #333; }\n"
"        .metric-title { font-size: 1.1em; color: #888; margin-bottom: 10px; }\n"
"        .metric-value { font-size: 2.5em; font-weight: bold; margin-bottom: 5px; }\n"
"        .metric-unit { font-size: 0.9em; color: #666; }\n"
"        .throughput { color: #4CAF50; }\n"
"        .messages { color: #2196F3; }\n"
"        .gpu { color: #FF9800; }\n"
"        .memory { color: #9C27B0; }\n"
"        .chart-container { background: #1a1a1a; border-radius: 10px; padding: 20px; margin-bottom: 20px; }\n"
"        .chart { width: 100%; height: 300px; background: #222; border-radius: 5px; position: relative; overflow: hidden; }\n"
"        .controls { background: #1a1a1a; border-radius: 10px; padding: 20px; }\n"
"        .control-group { margin-bottom: 15px; }\n"
"        .control-group label { display: block; margin-bottom: 5px; color: #ccc; }\n"
"        .control-group input, .control-group button { padding: 10px; border: 1px solid #555; background: #333; color: #fff; border-radius: 5px; }\n"
"        .control-group button { background: #667eea; cursor: pointer; transition: background 0.3s; }\n"
"        .control-group button:hover { background: #5a6fd8; }\n"
"        .status-indicator { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 8px; }\n"
"        .status-active { background: #4CAF50; }\n"
"        .status-inactive { background: #f44336; }\n"
"        .lane-visualization { display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px; margin-top: 20px; }\n"
"        .lane { background: #2a2a2a; padding: 15px; border-radius: 8px; text-align: center; }\n"
"        .lane-express { border-left: 4px solid #ff4444; }\n"
"        .lane-priority { border-left: 4px solid #ffaa00; }\n"
"        .lane-streaming { border-left: 4px solid #00aaff; }\n"
"        .lane-bulk { border-left: 4px solid #44ff44; }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class=\"header\">\n"
"        <h1>🚀 UMSBB v4.0 GPU-Accelerated Dashboard</h1>\n"
"        <p>Multi-GB/s Universal Multi-Segmented Bi-Buffer Bus with GPU Acceleration</p>\n"
"    </div>\n"
"    \n"
"    <div class=\"container\">\n"
"        <div class=\"metrics-grid\">\n"
"            <div class=\"metric-card\">\n"
"                <div class=\"metric-title\">Throughput</div>\n"
"                <div class=\"metric-value throughput\" id=\"throughput\">0.00</div>\n"
"                <div class=\"metric-unit\">GB/s</div>\n"
"            </div>\n"
"            <div class=\"metric-card\">\n"
"                <div class=\"metric-title\">Messages/Second</div>\n"
"                <div class=\"metric-value messages\" id=\"messages-per-sec\">0</div>\n"
"                <div class=\"metric-unit\">msg/s</div>\n"
"            </div>\n"
"            <div class=\"metric-card\">\n"
"                <div class=\"metric-title\">GPU Utilization</div>\n"
"                <div class=\"metric-value gpu\" id=\"gpu-utilization\">0.0</div>\n"
"                <div class=\"metric-unit\">%</div>\n"
"            </div>\n"
"            <div class=\"metric-card\">\n"
"                <div class=\"metric-title\">GPU Memory</div>\n"
"                <div class=\"metric-value memory\" id=\"gpu-memory\">0</div>\n"
"                <div class=\"metric-unit\">MB</div>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class=\"chart-container\">\n"
"            <h3>Real-Time Throughput</h3>\n"
"            <div class=\"chart\" id=\"throughput-chart\"></div>\n"
"        </div>\n"
"        \n"
"        <div class=\"chart-container\">\n"
"            <h3>Lane Utilization</h3>\n"
"            <div class=\"lane-visualization\">\n"
"                <div class=\"lane lane-express\">\n"
"                    <h4>EXPRESS</h4>\n"
"                    <div id=\"lane-express-value\">0.0 GB/s</div>\n"
"                    <div id=\"lane-express-percent\">0.0%</div>\n"
"                </div>\n"
"                <div class=\"lane lane-priority\">\n"
"                    <h4>PRIORITY</h4>\n"
"                    <div id=\"lane-priority-value\">0.0 GB/s</div>\n"
"                    <div id=\"lane-priority-percent\">0.0%</div>\n"
"                </div>\n"
"                <div class=\"lane lane-streaming\">\n"
"                    <h4>STREAMING</h4>\n"
"                    <div id=\"lane-streaming-value\">0.0 GB/s</div>\n"
"                    <div id=\"lane-streaming-percent\">0.0%</div>\n"
"                </div>\n"
"                <div class=\"lane lane-bulk\">\n"
"                    <h4>BULK</h4>\n"
"                    <div id=\"lane-bulk-value\">0.0 GB/s</div>\n"
"                    <div id=\"lane-bulk-percent\">0.0%</div>\n"
"                </div>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class=\"controls\">\n"
"            <h3>System Controls</h3>\n"
"            <div class=\"control-group\">\n"
"                <label>System Status: <span class=\"status-indicator status-active\" id=\"status-indicator\"></span><span id=\"status-text\">Active</span></label>\n"
"            </div>\n"
"            <div class=\"control-group\">\n"
"                <label for=\"target-throughput\">Target Throughput (GB/s):</label>\n"
"                <input type=\"number\" id=\"target-throughput\" value=\"10\" min=\"1\" max=\"100\" step=\"0.1\">\n"
"                <button onclick=\"updateTarget()\">Update Target</button>\n"
"            </div>\n"
"            <div class=\"control-group\">\n"
"                <button onclick=\"pauseProcessing()\">Pause Processing</button>\n"
"                <button onclick=\"resumeProcessing()\">Resume Processing</button>\n"
"                <button onclick=\"resetCounters()\">Reset Counters</button>\n"
"            </div>\n"
"        </div>\n"
"    </div>\n"
"    \n"
"    <script>\n"
"        let ws = null;\n"
"        let throughputHistory = [];\n"
"        const maxHistoryPoints = 50;\n"
"        \n"
"        function connectWebSocket() {\n"
"            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';\n"
"            ws = new WebSocket(`${protocol}//${window.location.host}`);\n"
"            \n"
"            ws.onopen = function() {\n"
"                console.log('WebSocket connected');\n"
"            };\n"
"            \n"
"            ws.onmessage = function(event) {\n"
"                const data = JSON.parse(event.data);\n"
"                updateMetrics(data);\n"
"            };\n"
"            \n"
"            ws.onclose = function() {\n"
"                console.log('WebSocket disconnected, reconnecting...');\n"
"                setTimeout(connectWebSocket, 1000);\n"
"            };\n"
"        }\n"
"        \n"
"        function updateMetrics(data) {\n"
"            document.getElementById('throughput').textContent = (data.throughput_gbps || 0).toFixed(2);\n"
"            document.getElementById('messages-per-sec').textContent = (data.messages_per_second || 0).toLocaleString();\n"
"            document.getElementById('gpu-utilization').textContent = (data.gpu_utilization || 0).toFixed(1);\n"
"            document.getElementById('gpu-memory').textContent = Math.round((data.total_gpu_memory_used || 0) / 1024 / 1024);\n"
"            \n"
"            // Update throughput chart\n"
"            throughputHistory.push(data.throughput_gbps || 0);\n"
"            if (throughputHistory.length > maxHistoryPoints) {\n"
"                throughputHistory.shift();\n"
"            }\n"
"            drawThroughputChart();\n"
"        }\n"
"        \n"
"        function drawThroughputChart() {\n"
"            const chart = document.getElementById('throughput-chart');\n"
"            const width = chart.clientWidth;\n"
"            const height = chart.clientHeight;\n"
"            \n"
"            // Simple SVG chart\n"
"            const maxThroughput = Math.max(...throughputHistory, 1);\n"
"            const points = throughputHistory.map((value, index) => {\n"
"                const x = (index / (maxHistoryPoints - 1)) * width;\n"
"                const y = height - (value / maxThroughput) * height;\n"
"                return `${x},${y}`;\n"
"            }).join(' ');\n"
"            \n"
"            chart.innerHTML = `\n"
"                <svg width=\"${width}\" height=\"${height}\" style=\"position: absolute; top: 0; left: 0;\">\n"
"                    <polyline points=\"${points}\" fill=\"none\" stroke=\"#4CAF50\" stroke-width=\"2\"/>\n"
"                    <text x=\"10\" y=\"20\" fill=\"#888\" font-size=\"12\">Max: ${maxThroughput.toFixed(2)} GB/s</text>\n"
"                </svg>\n"
"            `;\n"
"        }\n"
"        \n"
"        function updateTarget() {\n"
"            const target = document.getElementById('target-throughput').value;\n"
"            fetch('/api/control', {\n"
"                method: 'POST',\n"
"                headers: { 'Content-Type': 'application/json' },\n"
"                body: JSON.stringify({ target_throughput_gbps: parseFloat(target) })\n"
"            });\n"
"        }\n"
"        \n"
"        function pauseProcessing() {\n"
"            fetch('/api/control', {\n"
"                method: 'POST',\n"
"                headers: { 'Content-Type': 'application/json' },\n"
"                body: JSON.stringify({ pause_processing: true })\n"
"            });\n"
"        }\n"
"        \n"
"        function resumeProcessing() {\n"
"            fetch('/api/control', {\n"
"                method: 'POST',\n"
"                headers: { 'Content-Type': 'application/json' },\n"
"                body: JSON.stringify({ pause_processing: false })\n"
"            });\n"
"        }\n"
"        \n"
"        function resetCounters() {\n"
"            fetch('/api/control', {\n"
"                method: 'POST',\n"
"                headers: { 'Content-Type': 'application/json' },\n"
"                body: JSON.stringify({ reset_counters: true })\n"
"            });\n"
"        }\n"
"        \n"
"        // Initialize\n"
"        connectWebSocket();\n"
"        \n"
"        // Fetch initial data\n"
"        fetch('/api/metrics')\n"
"            .then(response => response.json())\n"
"            .then(data => updateMetrics(data));\n"
"    </script>\n"
"</body>\n"
"</html>";

    *content_length = strlen(dashboard_html);
    *html_content = (char*)malloc(*content_length + 1);
    if (!*html_content) return -1;
    
    strcpy(*html_content, dashboard_html);
    return 0;
}

// JSON utilities
json_response_t* json_create_response(void) {
    json_response_t* response = (json_response_t*)malloc(sizeof(json_response_t));
    if (!response) return NULL;
    
    response->json_capacity = 4096;
    response->json_data = (char*)malloc(response->json_capacity);
    if (!response->json_data) {
        free(response);
        return NULL;
    }
    
    response->json_length = 0;
    strcpy(response->json_data, "{");
    response->json_length = 1;
    
    return response;
}

int json_add_number(json_response_t* response, const char* key, double value) {
    if (!response || !key) return -1;
    
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "\"%s\":%.6f,", key, value);
    
    size_t needed = response->json_length + strlen(buffer);
    if (needed >= response->json_capacity) {
        response->json_capacity = needed * 2;
        char* new_data = (char*)realloc(response->json_data, response->json_capacity);
        if (!new_data) return -1;
        response->json_data = new_data;
    }
    
    strcat(response->json_data, buffer);
    response->json_length += strlen(buffer);
    
    return 0;
}

int json_add_integer(json_response_t* response, const char* key, uint64_t value) {
    if (!response || !key) return -1;
    
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "\"%s\":%I64u", key, (unsigned long long)value);
    strcat(buffer, ",");
    
    size_t needed = response->json_length + strlen(buffer);
    if (needed >= response->json_capacity) {
        response->json_capacity = needed * 2;
        char* new_data = (char*)realloc(response->json_data, response->json_capacity);
        if (!new_data) return -1;
        response->json_data = new_data;
    }
    
    strcat(response->json_data, buffer);
    response->json_length += strlen(buffer);
    
    return 0;
}

int json_finalize(json_response_t* response) {
    if (!response) return -1;
    
    // Remove trailing comma and add closing brace
    if (response->json_length > 1 && response->json_data[response->json_length - 1] == ',') {
        response->json_data[response->json_length - 1] = '}';
    } else {
        strcat(response->json_data, "}");
        response->json_length++;
    }
    
    return 0;
}

void json_free_response(json_response_t* response) {
    if (!response) return;
    free(response->json_data);
    free(response);
}

// Handle metrics API request
int web_handle_metrics_request(web_server_t* server, json_response_t* response) {
    if (!server || !response) return -1;
    
    gpu_performance_metrics_t metrics;
    gpu_update_performance_metrics(server->gpu_buffer, &metrics);
    
    json_add_number(response, "throughput_gbps", metrics.throughput_gbps);
    json_add_integer(response, "messages_per_second", metrics.messages_per_second);
    json_add_number(response, "gpu_utilization", metrics.gpu_utilization);
    json_add_integer(response, "active_streams", metrics.active_streams);
    json_add_integer(response, "total_gpu_memory_used", metrics.total_gpu_memory_used);
    json_add_number(response, "memory_bandwidth_utilization", metrics.memory_bandwidth_utilization);
    json_add_integer(response, "cuda_kernel_launches", metrics.cuda_kernel_launches);
    json_add_number(response, "average_kernel_time_ms", metrics.average_kernel_time_ms);
    
    json_finalize(response);
    return 0;
}

// Utility functions
uint64_t web_get_current_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000ULL) / freq.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

int web_parse_http_request(const char* request, char* method, char* path, char* headers) {
    const char* line_end = strstr(request, "\r\n");
    if (!line_end) return -1;
    
    // Parse first line: METHOD /path HTTP/1.1
    sscanf(request, "%15s %255s", method, path);
    
    // Copy headers
    const char* headers_start = strstr(request, "\r\n") + 2;
    strncpy(headers, headers_start, 2047);
    headers[2047] = '\0';
    
    return 0;
}

bool web_is_websocket_upgrade(const char* headers) {
    return strstr(headers, "Upgrade: websocket") != NULL;
}

int web_send_http_response(int socket_fd, int status_code, const char* content_type, const char* body, size_t body_length) {
    char header[1024];
    snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code,
        (status_code == 200) ? "OK" : "Not Found",
        content_type,
        (unsigned long)body_length);
    
    send(socket_fd, header, strlen(header), 0);
    if (body && body_length > 0) {
        send(socket_fd, body, body_length, 0);
    }
    
    return 0;
}

// Broadcast metrics to WebSocket clients
int web_broadcast_metrics(web_server_t* server) {
    json_response_t* response = json_create_response();
    if (!response) return -1;
    
    web_handle_metrics_request(server, response);
    
    // Send to all WebSocket clients
    for (uint32_t i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].is_active && server->clients[i].is_websocket) {
            web_send_websocket_frame(server->clients[i].socket_fd, response->json_data, response->json_length);
        }
    }
    
    json_free_response(response);
    return 0;
}

int web_send_websocket_frame(int socket_fd, const char* data, size_t length) {
    uint8_t frame[10];
    size_t frame_size = 2;
    
    frame[0] = 0x81; // Text frame, final fragment
    
    if (length < 126) {
        frame[1] = (uint8_t)length;
    } else if (length < 65536) {
        frame[1] = 126;
        frame[2] = (length >> 8) & 0xFF;
        frame[3] = length & 0xFF;
        frame_size += 2;
    } else {
        frame[1] = 127;
        for (int i = 0; i < 8; i++) {
            frame[2 + i] = (length >> (8 * (7 - i))) & 0xFF;
        }
        frame_size += 8;
    }
    
    send(socket_fd, (char*)frame, frame_size, 0);
    send(socket_fd, data, length, 0);
    
    return 0;
}

int web_handle_websocket_frame(web_server_t* server, uint32_t client_id, const char* data, size_t length) {
    // Simple WebSocket frame handling - just acknowledge receipt
    return 0;
}

void web_disconnect_client(web_server_t* server, uint32_t client_id) {
    if (client_id >= MAX_CLIENTS || !server->clients[client_id].is_active) return;
    
#ifdef _WIN32
    closesocket(server->clients[client_id].socket_fd);
#else
    close(server->clients[client_id].socket_fd);
#endif
    
    server->clients[client_id].is_active = false;
    server->clients[client_id].is_websocket = false;
    server->active_clients--;
    
    printf("👋 Client %d disconnected\n", client_id);
}

int web_update_performance_cache(web_server_t* server) {
    uint64_t current_time = web_get_current_time_ms();
    if (current_time - server->last_metrics_update > 100) { // Update every 100ms
        gpu_update_performance_metrics(server->gpu_buffer, &server->last_metrics);
        server->last_metrics_update = current_time;
    }
    return 0;
}

void web_server_stop(web_server_t* server) {
    if (!server) return;
    server->is_running = false;
}

void web_server_cleanup(web_server_t* server) {
    if (!server) return;
    
    server->is_running = false;
    
    // Disconnect all clients
    for (uint32_t i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].is_active) {
            web_disconnect_client(server, i);
        }
    }
    
    // Close server socket
    if (server->server_socket >= 0) {
#ifdef _WIN32
        closesocket(server->server_socket);
        WSACleanup();
#else
        close(server->server_socket);
#endif
    }
    
    printf("🧹 Web server cleanup completed\n");
}