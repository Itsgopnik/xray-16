-- Copies all *.script files visible to the engine (including encrypted archives)
-- to $XRAY_DUMP_DIR, then quits. XRAY_DUMP_ALIAS and XRAY_DUMP_MASK select other
-- files, e.g. XRAY_DUMP_ALIAS='$game_config$' XRAY_DUMP_MASK='*.ltx' (recursive). Run through -test_script, e.g. together with
-- -dump_bindings to get the engine's script API in the same run:
--
--   XRAY_DUMP_DIR=/tmp/scripts xr_3da -fsltx <fsgame.ltx> -nointro \
--       -test_script misc/tests/tools/dump_scripts.lua -dump_bindings
--
-- Only for local analysis: game scripts are copyrighted and must not be committed.

local out_dir = os.getenv("XRAY_DUMP_DIR")
if not out_dir or dump_scripts_done then
    return
end
dump_scripts_done = true

local alias = os.getenv("XRAY_DUMP_ALIAS") or "$game_scripts$"
local mask = os.getenv("XRAY_DUMP_MASK") or "*.script"

local fs = getFS()
local list = fs:file_list_open_ex(alias, FS.FS_ListFiles, mask)
local count, bytes = 0, 0
for i = 0, list:Size() - 1 do
    local name = list:GetAt(i):NameFull()
    local full = fs:update_path(alias, name)
    -- Archived files are registered with backslashes
    local size = fs:file_length(full)
    if size <= 0 then
        size = fs:file_length((full:gsub("/", "\\")))
    end
    local reader = fs:r_open(alias, name)
    if reader and size > 0 then
        local chunks, buf = {}, {}
        for n = 1, size do
            buf[#buf + 1] = reader:r_u8()
            if #buf == 4096 or n == size then
                chunks[#chunks + 1] = string.char(unpack(buf))
                buf = {}
            end
        end
        fs:r_close(reader)
        -- Flatten subdirectories: ui\\pda.xml -> ui__pda.xml
        local f = io.open(out_dir .. "/" .. name:gsub("[\\/]", "__"), "wb")
        f:write(table.concat(chunks))
        f:close()
        count, bytes = count + 1, bytes + size
    else
        log1(string.format("! dump_scripts: cannot read %s (path %s, size %s)", name, full, tostring(size)))
    end
end
log1(string.format("* dump_scripts: %d files, %d bytes written to %s", count, bytes, out_dir))
get_console():execute("quit")
