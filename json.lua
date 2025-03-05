local json = require "cjson"

local function file_load(filename)
    local file
    if filename == nil then
        file = io.stdin
    else
        local err
        file, err = io.open(filename, "rb")
        if file == nil then
            error(("Unable to read '%s': %s"):format(filename, err))
        end
    end
    local data = file:read("*a")

    if filename ~= nil then
        file:close()
    end

    if data == nil then
        error("Failed to read " .. filename)
    end

    return json.decode(data)
end

local function file_save(filename, data)
    local file
    if filename == nil then
        file = io.stdout
    else
        local err
        file, err = io.open(filename, "wb")
        if file == nil then
            error(("Unable to write '%s': %s"):format(filename, err))
        end
    end
    file:write(json.encode(data))
    if filename ~= nil then
        file:close()
    end
end 

json.save = file_save
json.load = file_load

return json