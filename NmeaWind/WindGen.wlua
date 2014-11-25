-------------------------------------------------------------------------=---
-- Name:        choices.wx.lua
-- Purpose:     Tests wxRadioBox, wxNotebook controls
-- Author:      J Winwood, Francesco Montorsi
-- Created:     March 2002
-- Copyright:   (c) 2002-5 Lomtick Software. All rights reserved.
-- Licence:     wxWidgets licence
-------------------------------------------------------------------------=---

-- Load the wxLua module, does nothing if running from wxLua, wxLuaFreeze, or wxLuaEdit
package.cpath = package.cpath..";./?.dll;./?.so;../lib/?.so;../lib/vc_dll/?.dll;../lib/bcc_dll/?.dll;../lib/mingw_dll/?.dll;"
require("wx")

local frame = nil
local PI = 3.1415

local sockAddress = wx.wxIPV4address()
sockAddress:Hostname("127.0.0.1")
sockAddress:Service  (11083)       -- PolarCom is listening on this

local function main()
    -- create the hierarchy: frame -> notebook
    frame = wx.wxFrame(wx.NULL, wx.wxID_ANY, "wxLua Wind Data Generator",
                       wx.wxDefaultPosition, wx.wxDefaultSize)
    frame:CreateStatusBar(1)
    frame:SetStatusText("NMEA sentence will be displayed here", 0)

    local defWxPoint = wx.wxDefaultPosition
    local defWxSize = wx.wxDefaultSize
    local span1 = wx.wxGBSpan(1, 1)
    local growAll = wx.wxEXPAND+wx.wxALL

    local panel = wx.wxPanel(frame, wx.wxID_ANY)
    local frameSizer = wx.wxBoxSizer(wx.wxVERTICAL)

    local animStaticBox = wx.wxStaticBoxSizer(wx.wxStaticBox(panel, -1, "Animation"), wx.wxVERTICAL)
    frameSizer:Add(animStaticBox, 1, wx.wxALL + wx.wxGROW, 5)

    local animSizer = wx.wxGridBagSizer(3, 3)
    animSizer:AddGrowableCol(5, 1)
    animStaticBox:Add(animSizer, 1, wx.wxGROW+wx.wxALL+wx.wxCENTER, 5)

    animSizer:Add(wx.wxStaticText(panel, wx.wxID_ANY, "Angle +/-"),
                                  wx.wxGBPosition(0, 0), span1, growAll, 5)
    local aniAngle = wx.wxSpinCtrl(panel, wx.wxID_ANY, "5", defWxPoint, defWxSize,
                                wx.wxSP_ARROW_KEYS + wx.wxSP_WRAP, -90, 90, 0)
    animSizer:Add(aniAngle, wx.wxGBPosition(0, 1), span1, growAll, 5)
    animSizer:Add(wx.wxStaticText(panel, wx.wxID_ANY, "Speed/+-"), wx.wxGBPosition(0, 2), span1, growAll, 5)
    local aniSpeed = wx.wxSpinCtrl(panel, wx.wxID_ANY, "10", defWxPoint, defWxSize,
                                wx.wxSP_ARROW_KEYS, 0, 50, 10)
    animSizer:Add(aniSpeed, wx.wxGBPosition(0, 3), span1, growAll, 5)
    local aniSpeedVar = wx.wxSpinCtrl(panel, wx.wxID_ANY, "10", defWxPoint, defWxSize,
                                   wx.wxSP_ARROW_KEYS + wx.wxSP_WRAP, 0, 49, 10)
    animSizer:Add(aniSpeedVar, wx.wxGBPosition(0, 4), span1, growAll, 5)

    animSizer:Add(wx.wxStaticText(panel, wx.wxID_ANY, "Step/sec"), wx.wxGBPosition(1, 0), span1, growAll, 5)
    local aniOffBtn = wx.wxButton(panel, wx.wxID_ANY, "Stop")
    animSizer:Add(aniOffBtn, wx.wxGBPosition(1, 1), span1, growAll, 5)
    local animSlider = wx.wxSlider(panel, wx.wxID_ANY, 0, 0, 20)
    animSizer:Add(animSlider, wx.wxGBPosition(1, 2), wx.wxGBSpan(1, 2), growAll, 5)
    local animTime = wx.wxSpinCtrl(panel, wx.wxID_ANY, "0", defWxPoint, defWxSize,
                               wx.wxSP_ARROW_KEYS, 0, 20, 0)
    animSizer:Add(animTime, wx.wxGBPosition(1, 4), span1, growAll, 5)
    animSizer:Fit(animStaticBox:GetStaticBox())

    local windStaticBox = wx.wxStaticBoxSizer(wx.wxStaticBox(panel, -1, "Wind"), wx.wxVERTICAL)
    frameSizer:Add(windStaticBox, 1, wx.wxALL+wx.wxGROW, 5)

    local windSizer = wx.wxGridBagSizer(3, 3)
    windStaticBox:Add(windSizer, 1, wx.wxGROW+wx.wxALL+wx.wxCENTER, 5)
    windSizer:AddGrowableCol(1, 3)

    windSizer:Add(wx.wxStaticText(panel, wx.wxID_ANY, "Angle"), wx.wxGBPosition(0, 0),
                  span1, growAll, 5)
    local windASlider = wx.wxSlider(panel, wx.wxID_ANY, 0, 0, 359)
    windSizer:Add(windASlider, wx.wxGBPosition(0, 1), span1, growAll, 5)
    local windAngle = wx.wxSpinCtrl(panel, wx.wxID_ANY, "0", defWxPoint, defWxSize,
                                wx.wxSP_ARROW_KEYS + wx.wxSP_WRAP, 0, 359, 0)
    windSizer:Add(windAngle, wx.wxGBPosition(0, 2), span1, growAll, 5)

    windSizer:Add(wx.wxStaticText(panel, wx.wxID_ANY, "Speed kt"), wx.wxGBPosition(1, 0),
                  span1, growAll, 5)
    local windSSlider = wx.wxSlider(panel, wx.wxID_ANY, 0, 0, 99)
    windSizer:Add(windSSlider, wx.wxGBPosition(1, 1), span1, growAll, 5)
    local windSpeed = wx.wxSpinCtrl(panel, wx.wxID_ANY, "0", defWxPoint, defWxSize,
                                wx.wxSP_ARROW_KEYS, 0, 99, 0)
    windSizer:Add(windSpeed, wx.wxGBPosition(1, 2), span1, growAll, 5)
    windSizer:Fit(windStaticBox:GetStaticBox())

    panel:SetSizer(frameSizer)
    frameSizer:SetSizeHints(frame)
    frame:Show(true)

    local clientSock
    local function sockSend(s)
        if not clientSock then
            sockConnecting = true
            clientSock = wx.wxSocketClient(wx.wxSOCKET_WAITALL)
            clientSock:Connect(sockAddress, false)
        end
        if clientSock and not clientSock:IsConnected() then
            local res = clientSock:WaitOnConnect(0, 5)
            if res then
                if not clientSock:IsConnected() then
                    -- Unsuccessful connect - Try again next time
                    clientSock:Destroy()
                    clientSock = nil
                end
            end
        end

        if clientSock and clientSock:IsConnected() then
            clientSock:Write(s)
            if clientSock:Error() then
                clientSock:Destroy()
                clientSock = nil
            else
                return true
            end
        end
        return false
    end

    local function checkSum(s)
        local val = 0
        for c in s:gmatch(".") do
          val = bit.bxor(val, string.byte(c))
        end
        return ("*%02X"):format(val)
    end
    local function doNmea(angle, speed)
        local s = ("IIMWV,%d.0,R,%d.0,N,A"):format(angle, speed)
        s = "$"..s..checkSum(s)
        local c = sockSend(s.."\r\n")
        print((c and "*" or " ") .. s)
        frame:SetStatusText((c and "CON " or " -- ") .. s, 0)
    end

    local TIMER_ID = 997 -- Say cheese
    local timer = wx.wxTimer(frame, TIMER_ID)
    timer:Stop()

    local function doAnim(val)
        -- print("doAnim", val)
        animSlider:SetValue(val)
        animTime:SetValue(("%d"):format(val))
        if val == 0 then
            timer:Stop()
        else
            timer:Start(math.ceil(1000 / val))
        end
    end

    local function doWind(angle, speed)
        -- print("doWind", angle, speed)
        windASlider:SetValue(angle)
        windSSlider:SetValue(speed)
        windAngle:SetValue(tostring(angle))
        windSpeed:SetValue(tostring(speed))
        doNmea(angle, speed)
    end

    frame:Connect(TIMER_ID, wx.wxEVT_TIMER, function(event)
        local angle = windASlider:GetValue() + tonumber(aniAngle:GetValue())
        angle = angle % 360
        local speed = tonumber(aniSpeed:GetValue()) +
                      math.ceil((math.sin(angle/180*PI) * tonumber(aniSpeedVar:GetValue())) + 0.5)
        doWind(angle, speed)
    end)

    local scrollEvents = {
        wx.wxEVT_SCROLL_TOP, wx.wxEVT_SCROLL_BOTTOM, wx.wxEVT_SCROLL_LINEUP,
        wx.wxEVT_SCROLL_LINEDOWN, wx.wxEVT_SCROLL_PAGEUP, wx.wxEVT_SCROLL_PAGEDOWN, 
        wx.wxEVT_SCROLL_THUMBTRACK, wx.wxEVT_SCROLL_THUMBRELEASE
    }


    frame:Connect(aniOffBtn:GetId(), wx.wxEVT_COMMAND_BUTTON_CLICKED, function(event)
        doAnim(0)
    end)
    for _,i in ipairs(scrollEvents) do
        frame:Connect(animSlider:GetId(), i, function(event)
            doAnim(animSlider:GetValue())
            event:Skip()
        end)
    end
    frame:Connect(animTime:GetId(), wx.wxEVT_COMMAND_SPINCTRL_UPDATED, function(event)
        doAnim(animTime:GetValue())
        event:Skip()
    end)

    for _,i in ipairs(scrollEvents) do
        frame:Connect(windASlider:GetId(), i, function(event)
            doWind(windASlider:GetValue(), windSSlider:GetValue())
            doAnim(0)
            event:Skip()
        end)
        frame:Connect(windSSlider:GetId(), i, function(event)
            doWind(windASlider:GetValue(), windSSlider:GetValue())
            doAnim(0)
            event:Skip()
        end)
    end
    frame:Connect(windSpeed:GetId(), wx.wxEVT_COMMAND_SPINCTRL_UPDATED, function(event)
        doWind(windASlider:GetValue(), tonumber(windSpeed:GetValue()))
        doAnim(0)
        event:Skip()
    end)
    frame:Connect(windAngle:GetId(), wx.wxEVT_COMMAND_SPINCTRL_UPDATED, function(event)
        doWind(tonumber(windAngle:GetValue()), windSSlider:GetValue())
        doAnim(0)
        event:Skip()
    end)

end

wxprint = print

function print(...)
    for i,a in ipairs(arg) do
        io.write(tostring(a)..(i<arg.n and "\t" or ""))
    end
    io.write("\n")
end

main()

-- Call wx.wxGetApp():MainLoop() last to start the wxWidgets event loop,
-- otherwise the wxLua program will exit immediately.
-- Does nothing if running from wxLua, wxLuaFreeze, or wxLuaEdit since the
-- MainLoop is already running or will be started by the C++ program.
wx.wxGetApp():MainLoop()
