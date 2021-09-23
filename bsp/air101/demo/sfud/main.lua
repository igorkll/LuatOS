
-- LuaTools需要PROJECT和VERSION这两个信息
PROJECT = "sfuddemo"
VERSION = "1.0.0"

local sys = require "sys"

sys.taskInit(function()
    log.info("sfud.init",sfud.init(0,20,20 * 1000 * 1000))
    log.info("sfud.get_device_num",sfud.get_device_num())
    local sfud_device = sfud.get_device_table()
    log.info("sfud.write",sfud.write(sfud_device,1024,"sfud"))
    log.info("sfud.read",sfud.read(sfud_device,1024,4))
    while 1 do
        sys.wait(1000)
    end
end)


-- 用户代码已结束---------------------------------------------
-- 结尾总是这一句
sys.run()
-- sys.run()之后后面不要加任何语句!!!!!