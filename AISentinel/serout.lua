local rs232 = require("luars232")

local e, p = rs232.open("com8")
if e ~= rs232.RS232_ERR_NOERROR then
	-- handle error
	out:write(string.format("can't open serial port '%s', error: '%s'\n",
			port_name, rs232.error_tostring(e)))
	return
end

assert(p:set_baud_rate(rs232.RS232_BAUD_38400) == rs232.RS232_ERR_NOERROR)
--assert(p:set_baud_rate(rs232.RS232_BAUD_2400) == rs232.RS232_ERR_NOERROR)
assert(p:set_data_bits(rs232.RS232_DATA_8) == rs232.RS232_ERR_NOERROR)
assert(p:set_parity(rs232.RS232_PARITY_NONE) == rs232.RS232_ERR_NOERROR)
assert(p:set_stop_bits(rs232.RS232_STOP_1) == rs232.RS232_ERR_NOERROR)
assert(p:set_flow_control(rs232.RS232_FLOW_OFF)  == rs232.RS232_ERR_NOERROR)

local cnt = 0
while true do

    -- read with timeout
    local err, data_read, size = p:read(1, 50)
    assert(e == rs232.RS232_ERR_NOERROR)

    io.write("-")
--[[
    err, size = p:write("UU", 100)
    ]]
    -- write with timeout 100 msec
    err, size = p:write("$AUz dabba dabba doo\r\n", 100)
    err, data_read, size = p:read(1, 2)
    err, size = p:write("!AIVDM,1,1,,A,X1u?etPv2;0n:dDP,0*24\r\n", 100) -- Ord.Pos
    assert(err == rs232.RS232_ERR_NOERROR)
    err, data_read, size = p:read(1, 2)
    cnt = cnt + 1
    if 1000 == cnt then
        cnt = 0
        io.write("|--")
        err, size = p:write("!AIVDM,1,1,,B,1>M46P>P00Os:IfM5ITf4?w60<00,0*37", 100) -- sharp
        assert(err == rs232.RS232_ERR_NOERROR)
    elseif 0 == (cnt % 1) then
--      io.write("")
--      err, size = p:write("!AIVDM,1,1,,B,1>M46P>P00Os:IfM5ITf4?w60<00,0*37", 100) -- sharp
        err, size = p:write("!AIVDM,1,1,,B,1>M46P?P00Os:IdM5IaN4?vR1P00,0*21", 100) -- test
        assert(err == rs232.RS232_ERR_NOERROR)
    end
    io.write("\b.")
end
