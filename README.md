# Minimal HTTP Server
A lightweight HTTP server written in C with Lua scripting support.

## Features
- Supports HTTP request handling with Lua scripting.
- Multi-threaded (Windows) and multi-process (Linux) support.
- Handles GET and POST requests.
- Configurable via `server.lua` script.

## Prerequisites

### Linux
- GCC Compiler
- Lua 5.x or LuaJIT 2.1
- Required libraries: `pthread`, `dl`, `m`

### Windows
- MinGW or MSVC
- Winsock2 library

## Installation

### Linux
```sh
sudo  apt  update
sudo  apt  install  -y  gcc  libluajit-5.1-dev  lua5.4  liblua5.4-dev
```

## Compilation

### Linux (LuaJIT 2.1)

Compile the server using the following command:

```sh
gcc  -o  server  src/server.c  -I/usr/include/luajit-2.1  -L/usr/lib  -lluajit-5.1  -ldl  -lm  -lpthread
```

### Linux (Lua 5.4)

Compile the server using the following command:

```sh
gcc  -o  server  src/server.c  -I/usr/include/lua5.4  -L/usr/lib  -llua5.4  -ldl  -lm  -lpthread
```
### Windows (MinGW)

```sh
gcc  -o  server.exe  src/server.c  -I<lua_include_path>  -L<lua_lib_path>  -llua5x  -lws2_32
```

## Usage

Run the server with:

```sh
./server
```

By default, the server listens on port `8080`. You can specify a different port using:

```sh
./server  -p <port>
```

## Lua Scripting

The server executes the Lua script `server.lua` to handle incoming requests. The script must define a function
  
```lua
--
-- minimal request handler in lua
--
function  handle_request(method, url, params, body, content_type)
  return  "text/plain", "Hello, World!"
end
```
or 

```lua
function  handle_request(method, url, params, body, content_type)
  return  "application/json", {name="John Doe", age=33}
end
```

## Server.lua 

  ### Advanced Request Handler for Minimal HTTP Server
  
  This `server.lua` script is the request handler for a minimal HTTP server. It processes HTTP requests, serves static files, and executes Lua scripts dynamically.

### Features

-  **Serves static files** (HTML, CSS, JS, PNG, JPG, etc.).
-  **Executes Lua scripts dynamically** based on the request URL.
-  **Handles query parameters** from the URL.
-  **Supports JSON and URL-encoded request bodies.**
-  **Implements a safe Lua execution environment** to prevent unintended side effects.

### File Structure

-  `server.lua` – Main Lua request handler.
-  `www/` – Directory containing static files.
-  `lib/` – Directory for additional Lua modules.
-  `api/` – Directory containing dynamic Lua scripts.

### How It Works

#### 1. **Static File Handling**

Static files are served from the `www/` directory.

Example:
- Request: `GET /index.html`
- Response: Returns `www/index.html`
  

#### 2. **Dynamic Lua Script Execution**

If a request is made to `/someapi`, it tries to execute `someapi.lua`.

Example:
- Request: `POST /process_data` with JSON body
- Execution: Runs `process_data.lua` and passes request parameters.

#### 3. **Query String Parsing**

Query strings are automatically parsed:

-  `GET /data?id=5&name=John` → `{ id = "5", name = "John" }`

#### 4. **Handling JSON and Form Data**

The request body is automatically parsed based on `Content-Type`:

-  **JSON (`application/json`)**
-  `{ "key": "value" }` → Parsed as Lua table
-  **Form-encoded (`application/x-www-form-urlencoded`)**
-  `key=value&name=John` → Parsed as Lua table

### Example: Writing a Custom API Endpoint

Create a file `api/v1/example.lua`:
```lua
function  post(params, body)
  return { message = "Received!", data = body }
end
```

Request:

```sh
curl  -X  POST  http://localhost:8080/api/v1/example  -H  "Content-Type: application/json"  -d  '{"name":"Alice"}'
```

Response:

```json
{
"message": "Received!",
"data": { "name": "Alice" }
}
```


## Error Handling

- If a script is missing → `404 Not Found`
- If JSON parsing fails → `{ "error": "Invalid JSON format" }`
- If a script does not define a proper function → `Method function not found`


## License

MIT License (c) 2025 github.com/hipbali
