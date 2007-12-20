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
 *      Joerg Langenberg <joerg.langenberg@gmx.net>
 *
 */
#include <iostream>

#include "odometry_chassis.h"
#include <main/angle_tool.h>

// init_flags (for init and cleanup)
#define INIT_BIT_DATA_MODULE            0
#define INIT_BIT_MBX_CHASSIS            1
#define INIT_BIT_MBX_WORK               2
#define INIT_BIT_PROXY_CHASSIS          3

//
// data structures
//

OdometryChassis *p_inst;

argTable_t argTab[] = {

    { ARGOPT_OPT, "chassisInst", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "The instance number of the chassis module", { 0 } },

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

int  OdometryChassis::moduleOn(void)
{
    int         ret;

    oldPositionX   = 0.0f;
    oldPositionY   = 0.0f;
    oldPositionRho = 0.0f;

    ret = chassis->on();
    if (ret)
    {
        GDOS_ERROR("Can't switch on chassis, code = %d\n", ret);
        return ret;
    }

    ret = chassis->getContData(0, &chassisMbx, &dataBufferPeriodTime);
    if (ret)
    {
        GDOS_ERROR("Can't get continuous data from chassis module, "
                   "code = %d\n", ret);
        return ret;
    }

    return RackDataModule::moduleOn();    // has to be last command in moduleOn();
}

void OdometryChassis::moduleOff(void)
{
    RackDataModule::moduleOff();          // has to be first command in moduleOff();

    chassis->stopContData(&chassisMbx);
}

int  OdometryChassis::moduleLoop(void)
{
    int             ret;
    message_info    info;
    odometry_data*  p_odo;
    float           positionX, positionY, positionRho;
    float           sinOldRho, cosOldRho;
    float           dX, dY;

    p_odo = (odometry_data *)getDataBufferWorkSpace();

    ret = chassisMbx.recvDataMsgTimed(300000000llu, &chassisData,
                                      sizeof(chassisData), &info);
    if (ret)
    {
        GDOS_ERROR("Can't read chassis data, code = %d\n", ret);
        return ret;
    }

    // check data
    if (info.type != MSG_DATA)
    {
        GDOS_ERROR("Message is no data message\n");
        return -EINVAL;
    }

    ChassisData::parse(&info);

//    GDOS_DBG_DETAIL("Robot data vx %f vy %f omega %a time %i\n",
//                    chassisData.vx, chassisData.vy, chassisData.omega,
//                    chassisData.recordingTime);

    // calculate new position
    dX        = chassisData.deltaX;
    dY        = chassisData.deltaY;
    sinOldRho = sin(oldPositionRho);
    cosOldRho = cos(oldPositionRho);

    positionX   = (oldPositionX + dX * cosOldRho - dY * sinOldRho);
    positionY   = (oldPositionY + dX * sinOldRho + dY * cosOldRho);
    positionRho = normaliseAngle(oldPositionRho + chassisData.deltaRho);

    p_odo->recordingTime = chassisData.recordingTime;
    p_odo->pos.x         = (int)rint(positionX);
    p_odo->pos.y         = (int)rint(positionY);
    p_odo->pos.z         = 0;
    p_odo->pos.phi       = 0.0f;
    p_odo->pos.psi       = 0.0f;
    p_odo->pos.rho       = positionRho;

    oldPositionX   = positionX;
    oldPositionY   = positionY;
    oldPositionRho = positionRho;

    GDOS_DBG_DETAIL("recordingTime %i x %i y %i rho %a\n",
                    p_odo->recordingTime, p_odo->pos.x, p_odo->pos.y, p_odo->pos.rho);

    putDataBufferWorkSpace(sizeof(odometry_data));

    return 0;
}

int  OdometryChassis::moduleCommand(message_info *msgInfo)
{
    switch(msgInfo->type)
    {
        case MSG_ODOMETRY_RESET:
            oldPositionX    = 0.0;
            oldPositionY    = 0.0;
            oldPositionRho  = 0.0;

            cmdMbx.sendMsgReply(MSG_OK, msgInfo);
            break;

        default:
            // not for me -> ask RackDataModule
            return RackDataModule::moduleCommand(msgInfo);
      }
      return 0;
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

int  OdometryChassis::moduleInit(void)
{
    int ret;

    // call RackDataModule init function (first command in init)
    ret = RackDataModule::moduleInit();
    if (ret)
    {
        return ret;
    }
    initBits.setBit(INIT_BIT_DATA_MODULE);

    // work mailbox
    ret = createMbx(&workMbx, 1, 128, MBX_IN_KERNELSPACE | MBX_SLOT);
    if (ret)
    {
        goto init_error;
    }
    initBits.setBit(INIT_BIT_MBX_WORK);

    // chassis mailbox
    ret = createMbx(&chassisMbx, 4, sizeof(chassis_data),
                    MBX_IN_KERNELSPACE | MBX_SLOT);
    if (ret)
    {
        goto init_error;
    }
    initBits.setBit(INIT_BIT_MBX_CHASSIS);

    // chassis
    chassis = new ChassisProxy(&workMbx, 0, chassisInst);
    if (!chassis)
    {
        ret = -ENOMEM;
        goto init_error;
    }
    initBits.setBit(INIT_BIT_PROXY_CHASSIS);

    return 0;

init_error:
    moduleCleanup();
    return ret;
}

void OdometryChassis::moduleCleanup(void)
{
    // call RackDataModule cleanup function
    if (initBits.testAndClearBit(INIT_BIT_DATA_MODULE))
    {
        RackDataModule::moduleCleanup();
    }

    // free proxy
    if (initBits.testAndClearBit(INIT_BIT_PROXY_CHASSIS))
    {
        delete chassis;
    }

    // delete chassis mailbox
    if (initBits.testAndClearBit(INIT_BIT_MBX_CHASSIS))
    {
        destroyMbx(&chassisMbx);
    }

    // delete work mailbox
    if (initBits.testAndClearBit(INIT_BIT_MBX_WORK))
    {
        destroyMbx(&workMbx);
    }
}

OdometryChassis::OdometryChassis()
      : RackDataModule( MODULE_CLASS_ID,
                    2000000000llu,    // 2s datatask error sleep time
                    16,               // command mailbox slots
                    48,               // command mailbox data size per slot
                    MBX_IN_KERNELSPACE | MBX_SLOT,  // command mailbox flags
                    1000,              // max buffer entries
                    10)               // data buffer listener
{
    // get value(s) out of your argument table
    chassisInst   = getIntArg("chassisInst", argTab);

    dataBufferMaxDataSize = sizeof(odometry_data);
}

int  main(int argc, char *argv[])
{
      int ret;

      // get args

      ret = RackModule::getArgs(argc, argv, argTab, "OdometryChassis");
      if (ret)
      {
        printf("Invalid arguments -> EXIT \n");
        return ret;
      }

      // create new OdometryChassis

      p_inst = new OdometryChassis();
      if (!p_inst)
      {
        printf("Can't create new OdometryChassis -> EXIT\n");
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
