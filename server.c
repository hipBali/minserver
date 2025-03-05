/*
 * Minimal HTTP Server with LUA
 * (c) 2025, github.com/hipbali
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <process.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#ifndef LUA_OK
	#define LUA_OK 0
#endif

#define DEFAULT_PORT 8080
#define DEFAULT_SCRIPT "server.lua"
#define DEFAULT_HANDLER "handle_request"
#define BUFFER_SIZE 16384
#define MAX_CONTENT_LENGTH 1000000 // 1MB limit to prevent DoS

lua_State *L = NULL; 

#ifdef _WIN32
char *strndup(const char *s, size_t n) {
    size_t len = strnlen(s, n);  // Biztonságos hossz meghatározás
    char *p = (char *)malloc(len + 1);
    if (p) {
        memcpy(p, s, len);
        p[len] = '\0';
    }
    return p;
}
char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *p = (char *)malloc(len);
    if (p) {
        memcpy(p, s, len);
    }
    return p;
}
#endif

void log_message(const char *level, const char *message);
void send_http_error(SOCKET client_socket, int status, const char *message);

void send_response(SOCKET client_socket, const char *method, const char *url, const char *params, const char *body, const char *content_type) {
    if (!L) {
        send_http_error(client_socket, 500, "Lua state not initialized");
        return;
    }

    lua_getglobal(L, DEFAULT_HANDLER);
    if (!lua_isfunction(L, -1)) {
        log_message("ERROR", "Lua handler function not found!");
        send_http_error(client_socket, 500, "Internal Server Error");
        return;
    }

    lua_pushstring(L, method);
    lua_pushstring(L, url);
    lua_pushstring(L, params);
    lua_pushstring(L, body);
    lua_pushstring(L, content_type);

    if (lua_pcall(L, 5, 2, 0) != LUA_OK) {
        log_message("ERROR", lua_tostring(L, -1));
        send_http_error(client_socket, 500, "Internal Server Error");
        return;
    }
    if (lua_tostring(L, -2) == NULL) {
        size_t err_length = 0;
        const char *err_string = lua_tolstring(L, -1, &err_length);
        log_message("ERROR", err_string ? err_string : "Unknown Lua error");
        send_http_error(client_socket, 500, err_string ? err_string : "Internal Server Error");
        lua_pop(L, 2);  
        return;
    }

    size_t body_length = 0;
    const char *response_content_type = lua_tostring(L, -2);
    const char *body_response = lua_tolstring(L, -1, &body_length);

    if (!response_content_type || !body_response) {
        response_content_type = "text/plain";
        body_response = "Error loading content";
        body_length = strlen(body_response);
    }

    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             response_content_type, body_length);
    send(client_socket, header, strlen(header), 0);
    send(client_socket, body_response, body_length, 0);

    lua_settop(L, 0);  // Lua stack tisztítása
}


void handle_client(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    int total_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (total_received <= 0) {
        log_message("ERROR", "Error reading socket.");
        closesocket(client_socket);
        return;
    }
    buffer[total_received] = '\0';

    char method[16], url[256], protocol[16];
    if (sscanf(buffer, "%15s %255s %15s", method, url, protocol) < 3) {
        send_http_error(client_socket, 400, "Bad Request");
        closesocket(client_socket);
        return;
    }
	
    char *body_start = strstr(buffer, "\r\n\r\n");
    if (!body_start) {
        send_http_error(client_socket, 400, "Bad Request");
        closesocket(client_socket);
        return;
    }
    body_start += 4;
    
    char *params = strchr(url, '?');
    if (params) {
        *params = '\0';
        params++;
    } else {
        params = "";
    }

    int content_length = 0;
    char *content_length_str = strstr(buffer, "Content-Length:");
    if (content_length_str) {
        sscanf(content_length_str, "Content-Length: %d", &content_length);
        if (content_length > MAX_CONTENT_LENGTH) {
            send_http_error(client_socket, 413, "Payload Too Large");
            closesocket(client_socket);
            return;
        }
    }
	
	char *content_type = "text/plain";
    char *content_type_str = strstr(buffer, "Content-Type:");
    if (content_type_str) {
        content_type_str += 13; 
        while (*content_type_str == ' ') content_type_str++; 
        char *end = strstr(content_type_str, "\r\n");
        if (end) {
            *end = '\0'; 
            content_type = content_type_str;
        }
    }
	
	char log_msg[1024];
	snprintf(log_msg, sizeof(log_msg), "Http request [%d] %s : %s (%s)", client_socket, method, url, content_type);
	log_message("INFO", log_msg);
	
    char *body = content_length > 0 ? strndup(body_start, content_length) : strdup("");
    send_response(client_socket, method, url, params, body, content_type);
    free(body);
    closesocket(client_socket);
}

void send_http_error(SOCKET client_socket, int status, const char *message) {
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             status, message, strlen(message), message);
    send(client_socket, response, strlen(response), 0);
}

void log_message(const char *level, const char *message) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    FILE *output = (strcmp(level, "ERROR") == 0) ? stderr : stdout;
    fprintf(output, "%s [%s]: %s\n", timestamp, level, message);
    fflush(output);  
}

#ifdef _WIN32
unsigned __stdcall thread_function(void *arg) {
    SOCKET client_socket = *(SOCKET*)arg;
    handle_client(client_socket);
    free(arg);
    return 0;
}
#endif

void print_timestamp(FILE *stream) {
    time_t now;
    struct tm *timeinfo;
    char buffer[64];

    time(&now);
    timeinfo = localtime(&now);
    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", timeinfo);
    fprintf(stream, "%s ", buffer);
}

void start_server(int port) {
    #ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    #endif
    
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("Bind failed");
		exit(EXIT_FAILURE);
	}

    if (listen(server_socket, 5) < 0) {
		perror("Listen failed");
		exit(EXIT_FAILURE);
	}
    char log_msg[100];
	snprintf(log_msg, sizeof(log_msg), "Server started on port %d", port);
	log_message("INFO", log_msg);
    
    while (1) {
        SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket == INVALID_SOCKET) continue;
        
        #ifdef _WIN32
        SOCKET *new_sock = malloc(sizeof(SOCKET));
        *new_sock = client_socket;
        HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, thread_function, new_sock, 0, NULL);
        CloseHandle(thread);
        #else
        pid_t pid = fork();
        if (pid == 0) {
            close(server_socket);
            handle_client(client_socket);
            exit(0);
        } else {
            close(client_socket);
        }
        while (waitpid(-1, NULL, WNOHANG) > 0);
        #endif
    }
}

void initialize_lua() {
    L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, DEFAULT_SCRIPT) != LUA_OK) {
        log_message("ERROR", lua_tostring(L, -1));
        lua_close(L);
        L = NULL;
    } else {
        log_message("INFO", "Lua script loaded successfully.");
    }
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 2 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
    }
    log_message("INFO", "Server starting...");
    initialize_lua();  
    start_server(port);
    if (L) {
        lua_close(L); 
    }
    return 0;
}
