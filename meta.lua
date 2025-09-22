---@meta
error("Cannot import a meta module")

---@class Tomlua.DateTable
---@field toml_type TomlType|TomlTypeNum -- one of tomlua.types, e.g., OFFSET_DATETIME
---@field year integer                   -- 1979, 3333, etc.
---@field month integer                  -- 1–12
---@field day integer                    -- 1–31
---@field hour integer                   -- 0–23
---@field minute integer                 -- 0–59
---@field second integer                 -- 0–59, leap second 60 optional
---@field fractional integer             -- 0–999999 (microsecond precision optional)
---@field offset_hour integer            -- UTC offset hours, e.g., -7
---@field offset_minute integer          -- UTC offset minutes, usually 0, can be 30/45

---@alias Tomlua.Date Tomlua.DateTable | userdata

---@class TomluaOptions
---@field fancy_tables? boolean
---@field int_keys? boolean
---@field fancy_dates? boolean
---@field multi_strings? boolean
---@field mark_inline? boolean
---@field overflow_errors? boolean
---@field underflow_errors? boolean

---@alias TomlType
---| "UNTYPED"
---| "STRING"
---| "STRING_MULTI"
---| "INTEGER"
---| "FLOAT"
---| "BOOL"
---| "ARRAY"
---| "TABLE"
---| "ARRAY_INLINE"
---| "TABLE_INLINE"
---| "LOCAL_DATE"
---| "LOCAL_TIME"
---| "LOCAL_DATETIME"
---| "OFFSET_DATETIME"

---@alias TomlTypeNum
---| 0
---| 1
---| 2
---| 3
---| 4
---| 5
---| 6
---| 7
---| 8
---| 9
---| 10
---| 11
---| 12
---| 13

---@class Tomlua.main
---@field opts TomluaOptions
---@field types table<TomlType, TomlTypeNum>
---@field decode fun(str:string):(any, string?): table?, string? -- returns result?, err?
---@field encode fun(val:any):(string, string?): string?, string? -- returns result?, err?
---@field type fun(val:any):TomlType
---@field type_of fun(val:any):TomlTypeNum
---@field typename fun(typ:TomlTypeNum):TomlType
---@field str_2_mul fun(s:string):userdata -- can call tostring on the result to get it back, written as multiline string by encode.
---@field new_date fun(src:string|number|Tomlua.DateTable|Tomlua.Date?):Tomlua.Date

---@alias Tomlua Tomlua.main | fun(opts?:TomluaOptions):Tomlua
