/*
 * RACK - Robotics Application Construction Kit
 * Copyright (C) 2005-2006 University of Hannover
 *                         Institute for Systems Engineering - RTS
 *                         Professor Bernardo Wagner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Authors
 *      Oliver Wulf      <wulf@rts.uni-hannover.de>
 *      Daniel Lecking   <lecking@rts.uni-hannover.de>
 *
 */
#include <iostream>

// include own header file
#include "ladar_hokuyo_urg.h"

// init_flags (for init and cleanup)
#define INIT_BIT_DATA_MODULE                0
#define INIT_BIT_SERIALPORT_OPEN            1

LadarHokuyoUrg *p_inst;

argTable_t argTab[] = {

    { ARGOPT_REQ, "serialDev", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "The number of the local serial device", { -1 } },

    { ARGOPT_OPT, "start", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "Point of the area from where the data reading starts, default 44",
      { 44 } },

    { ARGOPT_OPT, "end", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "Point of the area from where the data reading stops, default 725",
      { 725 } },

    { ARGOPT_OPT, "cluster", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "Number of neighboring points that are grouped as a cluster, default 3",
      { 3 } },

    { ARGOPT_OPT, "startAngle", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "startAngle, default 120", { 120 } },

    { 0, "", 0, 0, "", { 0 } } // last entry

};

/*******************************************************************************
 *   !!! REALTIME CONTEXT !!!
 *
 *   moduleOn,
 *   moduleOff,
 *   moduleLoop,
 *   moduleCommand,
 *
 *   own realtime user functions
 ******************************************************************************/

int  LadarHokuyoUrg::moduleOn(void)
{
    int ret;

    // get dynamic module parameter
    start       = getInt32Param("start");
    end         = getInt32Param("end");
    cluster     = getInt32Param("cluster");
    startAngle  = getInt32Param("startAngle");

    if (start > 44)
    {
 //       startAngle  = startAngle + 681 / (start - 44) * 240;
    }

    if (cluster < 0 || cluster > 3)
    {
        GDOS_ERROR("Invalid argument -> cluster [use 1..3] \n");
    }

    GDOS_DBG_INFO("Connect\n");

    ret = serialPort.clean();
    if (ret)
    {
        GDOS_ERROR("Can't clean serial port, code = %d \n", ret);
    }

    RackTask::sleep(50000000); // 50 ms

    ret = serialPort.send(sCommand115200, 15);
    if (ret)
    {
        GDOS_ERROR("Can't send S-Command to serial dev,code=%d\n", ret);
        return ret;
    }

    GDOS_DBG_INFO("Set baudrate to 115200\n");

    ret = serialPort.recv(serialBuffer, 1);
    if (ret)
    {
        GDOS_WARNING("Can't read set baudrate ack, code=%d\n", ret);
        GDOS_WARNING("Try on work baudrate 115200\n");
    }

    RackTask::sleep(200000000); // 200 ms

    ret = serialPort.setBaudrate(115200);
    if (ret)
    {
        GDOS_ERROR("Can't set baudrate to 115200, code=%d\n", ret);
        return ret;
    }

    RackTask::sleep(200000000); // 200 ms

    ret = serialPort.clean();
    if (ret)
    {
        GDOS_ERROR("Can't clean serial port\n", ret);
        return ret;
    }

    return RackDataModule::moduleOn();   // has to be last command in moduleOn();
}

void LadarHokuyoUrg::moduleOff(void)
{
    RackDataModule::moduleOff();         // has to be first command in moduleOff();

    GDOS_DBG_INFO("Disconnect\n");

    serialPort.send(sCommand19200, 15);

    RackTask::sleep(200000000); // 200 ms

    serialPort.setBaudrate(19200);
}

int  LadarHokuyoUrg::moduleLoop(void)
{
    ladar_data  *p_data;
    int         serialDataLen;
    int         i, j, ret;
    float       angleResolution;

    p_data = (ladar_data *)getDataBufferWorkSpace();

    p_data->duration        = 1000;
    p_data->maxRange        = 4000;

    // send G-Command (Distance Data Acquisition)
    gCommand[1] = (int)(start/100)      + 0x30;
    gCommand[2] = ((int)(start/10))%10  + 0x30;
    gCommand[3] = (start%10)            + 0x30;

    gCommand[4] = (int)(end/100)        + 0x30;
    gCommand[5] = ((int)(end/10))%10    + 0x30;
    gCommand[6] = (end%10) + 0x30;

    gCommand[7] = (int)(cluster/10)     + 0x30;
    gCommand[8] = (cluster%10)          + 0x30;

    ret = serialPort.send(gCommand, 10);
    if (ret)
    {
        GDOS_ERROR("Can't send G-Command to serial dev, code=%d\n", ret);
        return ret;
    }

    // receive G-Command (Distance Data Acquisition)

    // synchronize on message head, timeout after 200 attempts *****
    i = 0;
    serialBuffer[0] = 0;

    while((i < 2000) && (serialBuffer[0] != 'G'))
    {
        // Read next character
        ret = serialPort.recv(serialBuffer, 1, &(p_data->recordingTime), 2000000000ll);
        if (ret)
        {
            GDOS_ERROR("Can't read data from serial dev, code=%d\n", ret);
            return ret;
        }
        i++;
    }

    if(i == 2000)
    {
        GDOS_ERROR("Can't read data 2 from serial dev\n");
        return -1;
    }

    angleResolution         = (float)cluster * -0.3515625 * M_PI/180.0;
    p_data->pointNum        = (int32_t)((end - start)/cluster);     //max 681
    p_data->startAngle      = (float)startAngle * M_PI/180.0;
    p_data->endAngle        = normaliseAngleSym0(p_data->startAngle +
                                                 angleResolution * p_data->pointNum);

    GDOS_DBG_DETAIL("pointNum %i, startAngle %a, endAngle %a, angleResolution %a\n",p_data->pointNum,p_data->startAngle,p_data->endAngle, angleResolution);
    
    serialDataLen = 15 + p_data->pointNum * 2 + p_data->pointNum / 32;

    ret = serialPort.recv(serialBuffer, serialDataLen, NULL, 5000000000ll);
    if (ret)
    {
        GDOS_ERROR("Can't read data 3 from serial dev, code=%d\n",ret);
        return ret;
    }

    j = 10;
    for (i = 0; i < p_data->pointNum; i++)
    {
        if ((i % 32) == 0)
        {
            j += 1;  // first j = 11
        }

        p_data->point[i].distance = ((int32_t)(serialBuffer[j] - 0x30) << 6) |
                                     (int32_t)(serialBuffer[j + 1] - 0x30);
        if (p_data->point[i].distance < 20)
        {
            p_data->point[i].distance = 0;
        }

        p_data->point[i].angle = normaliseAngleSym0(p_data->startAngle + angleResolution * i);
        p_data->point[i].type  = LADAR_POINT_TYPE_UNKNOWN;

        j += 2;
    }

    putDataBufferWorkSpace(sizeof(ladar_data) + sizeof(ladar_point) * p_data->pointNum);
    return 0;
}

int  LadarHokuyoUrg::moduleCommand(message_info *msgInfo)
{
    // not for me -> ask RackDataModule
    return RackDataModule::moduleCommand(msgInfo);
}

/*******************************************************************************
 *   !!! NON REALTIME CONTEXT !!!
 *
 *   moduleInit,
 *   moduleCleanup,
 *   Constructor,
 *   Destructor,
 *   main,
 *
 *   own non realtime user functions
 ******************************************************************************/

int  LadarHokuyoUrg::moduleInit(void)
{
    int ret;

    ret = RackDataModule::moduleInit();
    if (ret)
    {
        return ret;
    }
    initBits.setBit(INIT_BIT_DATA_MODULE);

    ret = serialPort.open(serialDev, &urg_serial_config, this);
    if (ret)
    {
        GDOS_ERROR("Can't open serialDev %i, code=%d\n", serialDev, ret);
        goto init_error;
    }

    GDOS_DBG_INFO("serialDev %d has been opened \n", serialDev);
    initBits.setBit(INIT_BIT_SERIALPORT_OPEN);
    return 0;

init_error:

    // !!! call local cleanup function !!!
    LadarHokuyoUrg::moduleCleanup();
    return ret;
}

// non realtime context
void LadarHokuyoUrg::moduleCleanup(void)
{
    // call RackDataModule cleanup function
    if (initBits.testAndClearBit(INIT_BIT_DATA_MODULE))
    {
        RackDataModule::moduleCleanup();
    }

    if (initBits.testAndClearBit(INIT_BIT_SERIALPORT_OPEN))
    {
        serialPort.close();
    }
}

LadarHokuyoUrg::LadarHokuyoUrg()
      : RackDataModule( MODULE_CLASS_ID,
                    5000000000llu,    // 5s datatask error sleep time
                    16,               // command mailbox slots
                    48,               // command mailbox data size per slot
                    MBX_IN_KERNELSPACE | MBX_SLOT,  // command mailbox flags
                    5,                // max buffer entries
                    10)               // data buffer listener
{

    // get static module parameter
    serialDev   = getIntArg("serialDev", argTab);

    dataBufferMaxDataSize   = sizeof(ladar_data_msg);
    dataBufferPeriodTime    = 100;
}

int  main(int argc, char *argv[])
{
    int ret;

    // get args
    ret = RackModule::getArgs(argc, argv, argTab, "LadarHokuyoUrg");
    if (ret)
    {
        printf("Invalid arguments -> EXIT \n");
        return ret;
    }

    // create new LadarHokuyoUrg

    p_inst = new LadarHokuyoUrg();
    if (!p_inst)
    {
        printf("Can't create new LadarHokuyoUrg -> EXIT\n");
        return -ENOMEM;
    }

    // init

    ret = p_inst->moduleInit();
    if (ret)
        goto exit_error;

    p_inst->run();

    return 0;

exit_error:

    delete (p_inst);
    return ret;
}
