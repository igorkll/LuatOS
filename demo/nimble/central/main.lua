
-- LuaTools需要PROJECT和VERSION这两个信息
PROJECT = "bledemo"
VERSION = "1.0.0"

--[[
BLE centraldemo, 扫码并连接其他设备 ** 未完成 **
支持的模块:
1. Air101/Air103, 开发板的BLE天线未引出, 需要靠近使用, 且功耗高
2. ESP32系列, 包括ESP32C3/ESP32S3

-- 配合微信小程序 "LuatOS蓝牙调试"
-- 1. 若开发板无天线, 将手机尽量靠近芯片也能搜到
-- 2. 该小程序是开源的, 每次write会自动分包
-- https://gitee.com/openLuat/luatos-miniapps
]]

log.info("main", PROJECT, VERSION)

-- 一定要添加sys.lua !!!!
sys = require("sys")

--添加硬狗防止程序卡死
if wdt then
    wdt.init(9000)--初始化watchdog设置为9s
    sys.timerLoopStart(wdt.feed, 3000)--3s喂一次狗
end

-- 监听GATT服务器的WRITE_CHR, 也就是收取数据的回调
sys.subscribe("BLE_GATT_WRITE_CHR", function(info, data)
    -- info 是个table, 但当前没有数据
    log.info("ble", "data got!!", data:toHex())
end)

-- 接收扫描结果
sys.subscribe("BLE_SCAN_RESULT", function(addr, name, uuids)
    log.info("ble scan", (addr:toHex()), name, json.encode(uuids))
    if name == "LOS-065614A23900" then
        nimble.connect(addr)
    end
end)

sys.taskInit(function()
    sys.wait(2000)

    -- BLE模式, 默认是SERVER/Peripheral,即外设模式, 等待被连接的设
    nimble.mode(nimble.CLIENT) -- 默认就是它, 不用调用

    -- 可以自定义名称
    -- nimble.init("LuatOS-Wendal") -- 蓝牙名称可修改,也有默认值LOS-$mac地址
    nimble.init() -- 蓝牙名称可修改,也有默认值LOS-$mac地址

    sys.wait(500)
    -- 打印MAC地址
    local mac = nimble.mac()
    log.info("ble", "mac", mac and mac:toHex() or "Unknwn")
    sys.wait(1000)

    -- 发送数据
    while 1 do
        nimble.scan()
        sys.wait(120000)
    end
end)

-- 用户代码已结束---------------------------------------------
-- 结尾总是这一句
sys.run()
-- sys.run()之后后面不要加任何语句!!!!!
