-- Sky Islands BN Port - Utilities
-- Shared utility functions

local util = {}

-- Debug logging wrapper - set to true to enable debug output
util.DEBUG = false

function util.debug_log(msg)
  if util.DEBUG then
    gdebug.log_info(msg)
  end
end

return util
