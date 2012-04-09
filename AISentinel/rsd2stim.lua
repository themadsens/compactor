#!/usr/bin/env lua

-- This is CPUFREQ / CKDIV / BAUDRATE
local period = 250      -- Baud 38.400 @ 9.6 MHz 
local uart_mask = 0x01  -- Uart on PINB0

-- Generate 8/N/1 -- 8 bit / No parity / 1 stop bit

local function pin(bitno, val)
   print(("%09d:%02x"):format(bitno * period, (val == 0 and uart_mask or 0)))
end

local function main(arg)
   local str = arg[1]

   pin(0, 0)
-- print ""
   bitoff = 1000

   for c in str:gmatch(".") do
      local bitval = string.byte(c)
--    print(("BYTE '%s' %02x %03d"):format(c, bitval, bitval))
      pin(bitoff + 0, 1) -- start bit
      local mask = 1
      for bitno=1,8 do
         pin(bitoff + bitno, math.floor(bitval / mask) % 2) -- data bit
         mask = mask * 2
      end
      pin(bitoff + 9, 0) -- stop bit
      bitoff = bitoff + 15
 --   print ""
   end
   print("999999999:FF")
end

main(arg)

-- vim: set sw=3 ts=3 et nu:
