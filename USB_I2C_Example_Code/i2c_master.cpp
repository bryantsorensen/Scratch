/**
 * @file i2c_master.cpp
 *
 * @author FTDI
 * @date 2014-07-01
 *
 * Copyright © 2011 Future Technology Devices International Limited
 * Company Confidential
 *
 * Rivision History:
 * 1.0 - initial version
 */

//------------------------------------------------------------------------------
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>

//------------------------------------------------------------------------------
// include FTDI libraries
//
#include "ftd2xx.h"
#include "LibFT4222.h"


std::vector< FT_DEVICE_LIST_INFO_NODE > g_FT4222DevList;

//------------------------------------------------------------------------------
inline std::string DeviceFlagToString(DWORD flags)
{
    std::string msg;
    msg += (flags & 0x1)? "DEVICE_OPEN" : "DEVICE_CLOSED";
    msg += ", ";
    msg += (flags & 0x2)? "High-speed USB" : "Full-speed USB";
    return msg;
}

void ListFtUsbDevices()
{
    FT_STATUS ftStatus = 0;

    DWORD numOfDevices = 0;
    ftStatus = FT_CreateDeviceInfoList(&numOfDevices);

    for(DWORD iDev=0; iDev<numOfDevices; ++iDev)
    {
        FT_DEVICE_LIST_INFO_NODE devInfo;
        memset(&devInfo, 0, sizeof(devInfo));

        ftStatus = FT_GetDeviceInfoDetail(iDev, &devInfo.Flags, &devInfo.Type,
                                        &devInfo.ID, &devInfo.LocId,
                                        devInfo.SerialNumber,
                                        devInfo.Description,
                                        &devInfo.ftHandle);

        if (FT_OK == ftStatus)
        {
            const std::string desc = devInfo.Description;
            if(desc == "FT4222" || desc == "FT4222 A" || desc == "FT4222 B")
            {
                g_FT4222DevList.push_back(devInfo);
            }
        }
    }
}

//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
int main()
{
size_t i;
uint8_t DeviceUsed;
uint8_t Status;

    ListFtUsbDevices();

    if(g_FT4222DevList.empty()) {
        printf("No FT4222 device is found!\n");
        return 0;
    }

    DeviceUsed = 0;
    const FT_DEVICE_LIST_INFO_NODE& devInfo = g_FT4222DevList[DeviceUsed];

    printf("Open Device\n");
    printf("  Flags = %#x, (%s)\n", devInfo.Flags, DeviceFlagToString(devInfo.Flags).c_str());
    printf("  Type = %#x\n",        devInfo.Type);
    printf("  ID = %#x\n",          devInfo.ID);
    printf("  LocId = %#x\n",       devInfo.LocId);
    printf("  SerialNumber = %s\n",  devInfo.SerialNumber);
    printf("  Description = %s\n",   devInfo.Description);
    printf("  ftHandle = %#x\n",    (unsigned int)devInfo.ftHandle);


    FT_HANDLE ftHandle = NULL;

    FT_STATUS ftStatus;
    ftStatus = FT_OpenEx((PVOID)g_FT4222DevList[DeviceUsed].LocId, FT_OPEN_BY_LOCATION, &ftHandle);
    if (FT_OK != ftStatus)
    {
        printf("Open a FT4222 device failed!\n");
        return 0;
    }

    printf("\n\n");

FT4222_ClockRate clk = SYS_CLK_80;  // Also: SYS_CLK_24, SYS_CLK_48, SYS_CLK_60

    ftStatus = FT4222_SetClock(ftHandle, clk);
    if (FT4222_OK != ftStatus)
    {
        printf ("Problem setting FT4222 clock!\n");
    }

    printf("Init FT4222 as I2C master\n");
    ftStatus = FT4222_I2CMaster_Init(ftHandle, 1600);
    if (FT4222_OK != ftStatus)
    {
        printf("Init FT4222 as I2C master device failed!\n");
        return 0;
    }

    // TODO:
    //    Start to work as I2C master, and read/write data to a I2C slave
    //    FT4222_I2CMaster_Read
    //    FT4222_I2CMaster_Write

    const uint16 slaveAddr = 0x50;              // device address b1010000 = 0x50
    uint8 master_data[] = {0x02, 0x47, 0x96};   // addr 2 bytes, then data byte
    uint8 slave_data[2];
    uint16 sizeTransferred = 0;

    printf("I2C master write data to the slave(%#x)... \n", slaveAddr);
    ftStatus = FT4222_I2CMaster_Write(ftHandle, slaveAddr, master_data, sizeof(master_data), &sizeTransferred);
    if (FT4222_OK != ftStatus)
    {
        printf("I2C master write error returned by Write call\n");
    }
    if (sizeof(master_data) != sizeTransferred)
    {
        printf("I2C Master Write error - sizes don't match\n");
    }
    ftStatus = FT4222_I2CMaster_GetStatus(ftHandle, &Status);
    if (Status != 0x20)     // Anything but IDLE
    {
        printf ("I2C Master Status shows error! %#x\n", Status);
        if (I2CM_CONTROLLER_BUSY(Status))
            printf("    - Controller busy\n");
        if (I2CM_DATA_NACK(Status))
            printf("    - Slave NACKed data\n");
        if (I2CM_ADDRESS_NACK(Status))
            printf("    - Slave NACKed address\n");
        if (I2CM_ARB_LOST(Status))
            printf("    - Arbitration lost\n");
        if (I2CM_BUS_BUSY(Status))
            printf("    - I2C Bus Busy\n");
    }

    printf("I2C master read data from the slave(%#x)... \n", slaveAddr);    // Read back from same address just written
    master_data[1] = 0x46;
    ftStatus = FT4222_I2CMaster_Write(ftHandle, slaveAddr, master_data, 2, &sizeTransferred);   // Write EEPROM address
    ftStatus = FT4222_I2CMaster_Read(ftHandle, slaveAddr, slave_data, sizeof(slave_data), &sizeTransferred);    // Read back data
    if (FT4222_OK == ftStatus)
    {
        printf("  Data read from Slave: ");
        for(i=0; i<sizeof(slave_data)-1; ++i)
        {
            printf("%#x, ", slave_data[i]);
        }
        printf("%#x\n", slave_data[i]);
    }
    else
    {
        printf("I2C master read error\n");
    }

    printf("UnInitialize FT4222\n");
    FT4222_UnInitialize(ftHandle);

    printf("Close FT device\n");
    FT_Close(ftHandle);
    return 0;
}
