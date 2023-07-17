Created by Bryant Sorensen  17 July 2023

The i2c_master.cpp file originated from the FT4222 library set of samples.
It has been modified to show extra things that need to be done to get it to work properly.

-- I had to copy the ftd2xx.h file from "CDM v2.12.36.4 WHQL Certified.zip" to replace the one that came with the FT4222 library install

-- For debug, I had to copy LibFT4222.dll to the same directory as i2c_master.vcxproj

-- I changed jumpers JP2 and JP3 to put the board into Mode 3 (board comes set up as Mode 0)

-- After the call to FT_GetDeviceInfoDetail(), in looking at the device info Description, I put in check of "FT4222 B" as well as "FT4222 A" for Mode 0 when detecting the board.  Detecting the string "FT4222" works for detecting Mode 3.

-- If using Mode 0, I _think_ both "FT4222 A" and "FT4222 B" will work.  I punted and used Mode3.

-- MUST DO: Need to check I2C master status after doing a write. The write function does not report if a NACK happened or other bus error.
FT4222_I2CMaster_GetStatus()

-- Can change the clock frequency of the FT4222 chip; not required, may be helpful
FT4222_SetClock()
