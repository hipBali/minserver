--
-- minimal http server
-- request handler
--

package.path=package.path.."?.lua;lib/?.lua;"
package.cpath=package.cpath.."lib/?.so;lib/?.dll;"

-- global JSON
json = require "json"
sqlite = require "sqlite"

local mime_types = {
    html = "text/html",
    css  = "text/css",
    js   = "application/javascript",
    json = "application/json",
    png  = "image/png",
    jpg  = "image/jpeg",
    jpeg = "image/jpeg"
}

local function serve_static_file(path)
    local ext = path:match("%.([a-zA-Z0-9]+)$") 
    if not ext then
        return "text/plain", "Invalid file type"
    end
    local mime_type = mime_types[ext] or "application/octet-stream"
    local file = io.open("www" .. path, "rb")
    if not file then
        return "text/plain", "404 Not Found"
    end
    local content = file:read("*all")
    file:close()
    return mime_type, content
end

local function parse_query_string(query)
    local params = {}
    if query then
        for key, value in query:gmatch("([^&=?]+)=([^&=?]+)") do
            key = key:gsub("%%(%x%x)", function(hex) 
                return string.char(tonumber(hex, 16)) 
            end) 
            value = value:gsub("%%(%x%x)", function(hex) 
                return string.char(tonumber(hex, 16)) 
            end) 
            params[key] = value
        end
    end
    return params
end

function safe_run_lua(file_path, global_env, method, params, body)
    local file = io.open(file_path, "r")
    if not file then
        return nil, "File not found: " .. file_path
    end
    local script = file:read("*a")
    file:close()
    local safe_env = setmetatable({}, { __index = global_env or _G })
    local chunk, err
    if _VERSION == "Lua 5.1" then
        chunk, err = loadstring(script, file_path)  -- Lua 5.1
    else
        chunk, err = load(script, file_path, "t", safe_env) -- Lua 5.2+
    end
    if not chunk then
        return nil, "Error loading script: " .. err
    end
    if _VERSION == "Lua 5.1" then
        setfenv(chunk, safe_env)
    end
    local success, result = pcall(chunk)
    if not success then
        return nil, "Error executing script: " .. result
    end
    local method_func = safe_env[method:lower()] 
    if type(method_func) ~= "function" then
        return nil, "Method function '" .. method .. "' not found in script"
    end
    local call_success, call_result = pcall(method_func, params or {}, body or {})
    if not call_success then
        return nil, "Error executing method function: " .. call_result
    end
    return call_result
end

local function generate_dynamic_response(method, url, params, body)
	local api_url = url:gsub("^/", "") .. ".lua"
	local global_env = {
		print = print,
		ipairs = ipairs,
		pairs = pairs,
		tonumber = tonumber,
		tostring = tostring,
		type = type,
		string = string,
		table = table,
		math = math,
		json = json,
		sqlite = sqlite,
	}
	local result, err = safe_run_lua(api_url, global_env, method, params, body)
	if not result then
		return nil, string.format("LUA error: %s", err)
	else
		return "application/json", json.encode(result)
	end
end

function handle_request(method, url, params, body, content_type)
    local parsed_params = parse_query_string(params)
    if content_type == "application/json" then
        local success, decoded_body = pcall(json.decode, body)
        if success then
            body = decoded_body
        else
            body = { error = "Invalid JSON format" }
        end
	elseif content_type == "application/x-www-form-urlencoded" then
        body = parse_query_string(body) 
    end
    if method:upper() == "GET" and url == "/" and params:len() == 0 then
        url = "/index.html"
    end
    if url:match("%.[a-zA-Z0-9]+$") and not url:find("?") then
		return serve_static_file(url)
    end
    return generate_dynamic_response(method, url, parsed_params, body)
end


