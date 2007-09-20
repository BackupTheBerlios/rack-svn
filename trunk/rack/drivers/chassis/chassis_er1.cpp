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
 *      Marko Reimer <marko.reimer@gmx.de>
 *
 */
#include <iostream>

#include "chassis_er1.h"

//
// data structures
//

// init_flags (for init and cleanup)
#define INIT_BIT_DATA_MODULE                0
#define INIT_BIT_SERIAL_OPENED              1
#define INIT_BIT_MTX_CREATED                2

#define MAX_SIP_PACKAGE_SIZE 208

#define SAMPLING_RATE   10
#define TICKS_PER_MM    -70
#define CALIBRATION_ROT 1 


ChassisER1 *p_inst;

argTable_t argTab[] = {

    { ARGOPT_REQ, "serialDev", ARGOPT_REQVAL, ARGOPT_VAL_STR,
      "Serial device number", { 0 } },

    { ARGOPT_OPT, "axisWidth", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "width of the driven axis, default 380 mm", { 380 } },

    { ARGOPT_OPT, "vxMax", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "max vehicle velocity in x direction, default 350 mm/s", { 1200 } },

    { ARGOPT_OPT, "vxMin", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "min vehicle velocity in x direction, default 10 mm/s)", { 80 } },

    { ARGOPT_OPT, "axMax", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "max vehicle acceleration in x direction, default 500", { 500 } },

    { ARGOPT_OPT, "omegaMax", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "omegaMax, default 30 deg/s", { 30 } },

    { ARGOPT_OPT, "minTurningRadius", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "min vehicle turning radius, default 200 (mm)", { 200 } },

    { ARGOPT_OPT, "breakConstant", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "breakConstant, default 100 (1.0f)", { 100 } },

    { ARGOPT_OPT, "safetyMargin", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "safetyMargin, default 50 (mm)", { 50 } },

    { ARGOPT_OPT, "safetyMarginMove", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "safety margin in move directiom, default 200 (mm)", { 200 } },

    { ARGOPT_OPT, "comfortMargin", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "comfortMargin, default 300 (mm)", { 300 } },

    { ARGOPT_OPT, "front", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "front, default 250 (mm)", { 250 } },

    { ARGOPT_OPT, "back", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "back, default 250 (mm)", { 250 } },

    { ARGOPT_OPT, "left", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "left, default 250 (mm)", { 250 } },

    { ARGOPT_OPT, "right", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "right, default 250 (mm)", { 250 } },

    { ARGOPT_OPT, "wheelBase", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "wheel distance, default 380 (mm)", { 380 } },

    { ARGOPT_OPT, "wheelRadius", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "wheelRadius, default 50 (mm)", { 50 } },

    { ARGOPT_OPT, "trackWidth", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "trackWidth, default 380 (mm)", { 380 } },

    { ARGOPT_OPT, "pilotParameterA", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "pilotParameterA, default 10 (0.001f)", { 10 } },

    { ARGOPT_OPT, "pilotParameterB", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "pilotParameterB, default 200 (2.0f)", { 200 } },

    { ARGOPT_OPT, "pilotVTransMax", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "pilotVTransMax, default 200", { 200 } },

    { 0, "", 0, 0, "", { 0 } } // last entry
};

// vehicle parameter

chassis_param_data param = {
    vxMax:            1200,                  // mm/s
    vyMax:            0,
    vxMin:            80,                   // mm/s
    vyMin:            0,
    axMax:            200,                  // mm/s
    ayMax:            0,
    omegaMax:         (30.0 * M_PI / 180.0),// rad/s
    minTurningRadius: 200,                  // mm

    breakConstant:    1.0f,                 // mm/mm/s
    safetyMargin:     50,                   // mm
    safetyMarginMove: 200,                  // mm
    comfortMargin:    300,                  // mm

    boundaryFront:    250,                  // mm
    boundaryBack:     250,                  // mm
    boundaryLeft:     250,                  // mm
    boundaryRight:    250,                  // mm

    wheelBase:        380,                  // mm
    wheelRadius:       50,                  // mm
    trackWidth:       380,

    pilotParameterA:  0.001f,
    pilotParameterB:  2.0f,
    pilotVTransMax:   200,                  // mm/s
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

 int ChassisER1::moduleOn(void)
{
    RackTask::disableRealtimeMode();
    GDOS_DBG_DETAIL("calling wheel init");
 
    SendCmd(1,Reset);
//    pwdreply_print(&(SendCmd(1,Reset)));
    
    rcm_WheelInit();
    GDOS_DBG_DETAIL("finished wheel init");

    watchdogCounter = 0;
    activePilot     = CHASSIS_INVAL_PILOT;
    
    GDOS_DBG_DETAIL("calling modul on");
    
    return RackDataModule::moduleOn();  // has to be last command in moduleOn();
}

// realtime context
void ChassisER1::moduleOff(void)
{
    RackDataModule::moduleOff();        // has to be first command in moduleOff();

    activePilot = CHASSIS_INVAL_PILOT;

    rcm_WheelSetPower(0x00);
    rcm_WheelUpdate();

}

// realtime context
int ChassisER1::moduleLoop(void)
{
    chassis_data*   p_data = NULL;
    ssize_t         datalength = 0;
    rack_time_t     time;
    float           deltaT;


    // get datapointer from rackdatabuffer
//    GDOS_DBG_DETAIL("modul loop starting");
    p_data = (chassis_data *)getDataBufferWorkSpace();

    time    = rackTime.get();
    deltaT  = (float)(time - oldTime) / 1000.0;
    oldTime = time;
    
    p_data->deltaX   = deltaT * (speedLeft + speedRight) / 2 ;
    p_data->deltaY   = 0.0f;
    p_data->deltaRho = deltaT * (speedLeft - speedRight) / 2.0 / 200.0 * CALIBRATION_ROT;

    p_data->vx    = (speedLeft + speedRight) / 2.0;
    p_data->vy    = 0.0f;
    p_data->omega = (speedLeft - speedRight) / 2.0 / 200.0 * CALIBRATION_ROT;

    p_data->recordingTime = rackTime.get();
    p_data->battery       = battery;
    p_data->activePilot   = activePilot;

    datalength = sizeof(chassis_data);

    putDataBufferWorkSpace(datalength);

/*    GDOS_DBG_DETAIL("lEncoder=%i rEncoder=%i vx=%f mm/s omega=%a deg/s\n",
//                    leftEncoderNew, rightEncoderNew,
                    speedLeft,speedRight,
                    p_data->vx, p_data->omega);
*/
    RackTask::sleep(100000000llu);   //100ms
    
    return 0;
}

// realtime context
int ChassisER1::moduleCommand(message_info *msgInfo)
{
    unsigned int pilot_mask = RackName::getSysMask() |
                              RackName::getClassMask() |
                              RackName::getInstMask();
    chassis_move_data               *p_move;
    chassis_set_active_pilot_data   *p_pilot;

    RackTask::disableRealtimeMode();

    switch (msgInfo->type)
    {
    case MSG_CHASSIS_MOVE:
        if (status != MODULE_STATE_ENABLED)
        {
            cmdMbx.sendMsgReply(MSG_ERROR, msgInfo);
            break;
        }

        p_move = ChassisMoveData::parse(msgInfo);
        if ((msgInfo->src & pilot_mask) != activePilot)
        {
            cmdMbx.sendMsgReply(MSG_ERROR, msgInfo);
            break;
        }

        // set min speed
        if ((p_move->vx > 0) && (p_move->vx < param.vxMin))
            p_move->vx = param.vxMin;
        if ((p_move->vx < 0) && (p_move->vx > -param.vxMin))
            p_move->vx = -param.vxMin;

        if (sendMovePackage(p_move->vx, p_move->omega))
        {
            cmdMbx.sendMsgReply(MSG_ERROR, msgInfo);
            break;
        }
        cmdMbx.sendMsgReply(MSG_OK, msgInfo);
        break;

    case MSG_CHASSIS_GET_PARAMETER:
        cmdMbx.sendDataMsgReply(MSG_CHASSIS_PARAMETER, msgInfo, 1, &param,
                                sizeof(chassis_param_data));
        break;

    case MSG_CHASSIS_SET_ACTIVE_PILOT:
        p_pilot = ChassisSetActivePilotData::parse(msgInfo);

        if (((msgInfo->src & pilot_mask) == RackName::create(GUI, 0)) ||
                ((msgInfo->src & pilot_mask) == RackName::create(PILOT, 0)))
        {
            activePilot = p_pilot->activePilot & pilot_mask;
            sendMovePackage(0, 0);
            GDOS_DBG_INFO("%x Changed active pilot to %x", msgInfo->src, activePilot);
            cmdMbx.sendMsgReply(MSG_OK, msgInfo);
        }
        else
        {
            GDOS_ERROR("%x has no permission to change active pilot to %x",
                       msgInfo->src, p_pilot->activePilot);
            cmdMbx.sendMsgReply(MSG_ERROR, msgInfo);
        }
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

int ChassisER1::moduleInit(void)
{
    int ret;
    struct termios oldtio,newtio;
    struct serial_struct serial;
    int fd;

    // call RackDataModule init function (first command in init)
    ret = RackDataModule::moduleInit();
    if (ret)
    {
        return ret;
    }
    initBits.setBit(INIT_BIT_DATA_MODULE);

    // create hardware mutex
    ret = hwMtx.create();
    if (ret)
    {
        goto init_error;
    }
    initBits.setBit(INIT_BIT_MTX_CREATED);

    // open serial port
/*    ret = serialPort.open(serialDev, &pioneer_serial_config, this);
    if (ret)
    {
        printf("Can't open serialDev %i\n", serialDev);
        goto init_error;
    }
*/

    fd = rcm.handle= open(serialDev, O_RDWR|O_NOCTTY);

    if (fd < 0 ) {
        printf("Can't open serialDev\n");
        goto init_error;
    }

    tcgetattr(fd,&oldtio);
    bzero(&newtio,sizeof(newtio));

    newtio.c_cflag = B38400|CS8|CLOCAL|CREAD;
    newtio.c_iflag = IGNPAR | ICRNL ;
    newtio.c_oflag = 0;
//      newtio.c_lflag = ICANON;
    cfmakeraw(&newtio);

    newtio.c_cc[VMIN] = 1;
    newtio.c_cc[VTIME] = 0;

    tcflush(fd,TCIFLUSH);
    tcsetattr(fd,TCSANOW,&newtio);

    ioctl(fd,TIOCGSERIAL,&serial);
    serial.custom_divisor = serial.baud_base / 250000;
    serial.flags = 0x0030;
    ioctl(fd,TIOCSSERIAL,&serial);

    initBits.setBit(INIT_BIT_SERIAL_OPENED);

    printf("Completed SERIAL Initialize\n");

    return 0;

init_error:
    ChassisER1::moduleCleanup();
    return ret;
}

void ChassisER1::moduleCleanup(void)
{
    // call RackDataModule cleanup function
    if (initBits.testAndClearBit(INIT_BIT_DATA_MODULE))
    {
        RackDataModule::moduleCleanup();
    }

    // destroy mutex
    if (initBits.testAndClearBit(INIT_BIT_MTX_CREATED))
    {
        hwMtx.destroy();
    }

    // close serial port
    if (initBits.testAndClearBit(INIT_BIT_SERIAL_OPENED))
    {
        close(rcm.handle);
    }
}

ChassisER1::ChassisER1()
        : RackDataModule( MODULE_CLASS_ID,
                      5000000000llu,        // 5s datatask error sleep time
                      16,                   // command mailbox slots
                      48,                   // command mailbox data size per slot
                      MBX_IN_KERNELSPACE | MBX_SLOT, // command mailbox flags
                      5,                    // max buffer entries
                      10)                   // data buffer listener
{
    // get value(s) out of your argument table
    serialDev               = getStrArg("serialDev", argTab);
    axisWidth               = getIntArg("axisWidth", argTab);
    param.vxMax             = getIntArg("vxMax", argTab);
    param.vxMin             = getIntArg("vxMin", argTab);
    param.axMax             = getIntArg("axMax", argTab);
    param.omegaMax          = getIntArg("omegaMax", argTab) * M_PI / 180.0;
    param.minTurningRadius  = getIntArg("minTurningRadius", argTab);
    param.breakConstant     = (float)getIntArg("breakConstant", argTab) / 100.0f;
    param.safetyMargin      = getIntArg("safetyMargin", argTab);
    param.safetyMarginMove  = getIntArg("safetyMarginMove", argTab);
    param.comfortMargin     = getIntArg("comfortMargin", argTab);
    param.boundaryFront     = getIntArg("front", argTab);
    param.boundaryBack      = getIntArg("back", argTab);
    param.boundaryLeft      = getIntArg("left", argTab);
    param.boundaryRight     = getIntArg("right", argTab);
    param.wheelBase         = getIntArg("wheelBase", argTab);
    param.wheelRadius       = getIntArg("wheelRadius", argTab);
    param.trackWidth        = getIntArg("trackWidth", argTab);
    param.pilotParameterA   = (float)getIntArg("pilotParameterA", argTab) / 10000.0f;
    param.pilotParameterB   = (float)getIntArg("pilotParameterB", argTab) / 100.0f;
    param.pilotVTransMax    = getIntArg("pilotVTransMax", argTab);

    // set dataBuffer size
    setDataBufferMaxDataSize(sizeof(chassis_data));

    // set databuffer period time
    setDataBufferPeriodTime(1000/SAMPLING_RATE);
}

int main(int argc, char *argv[])
{
    int ret;


    // get args
    ret = RackModule::getArgs(argc, argv, argTab, "ChassisER1");
    if (ret)
    {
        printf("Invalid arguments -> EXIT \n");
        return ret;
    }

    // create new ChassisER1

    p_inst = new ChassisER1();
    if (!p_inst)
    {
        printf("Can't create new ChassisER1 -> EXIT\n");
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

int ChassisER1::getPositionPackage(int *leftPos, int *rightPos)
{
    PWDReply replyL , replyR; 

    GDOS_DBG_DETAIL("requesting position from er1\n");
    
    hwMtx.lock(RACK_INFINITE);
    
    replyL     = SendCmd(0,GetPostion);
    replyR     = SendCmd(1,GetPostion);

    hwMtx.unlock();

    pwdreply_print(&replyL);
    *leftPos  = pwdreply_data(&replyL);
    
    pwdreply_print(&replyR);
    *rightPos  = pwdreply_data(&replyR);

    return 0;
}


int ChassisER1::sendMovePackage(int vx, float omega)
{
    long   speedLeftL, speedRightL;

    speedLeft  = (float)vx;
    speedRight = (float)vx;

    speedLeft  += (omega *((float)axisWidth / 2.0f));
    speedRight -= (omega *((float)axisWidth / 2.0f));

    speedLeftL  = (long) (speedLeft  * TICKS_PER_MM / SAMPLING_RATE);
    speedRightL = (long) (speedRight * TICKS_PER_MM / SAMPLING_RATE);
    
    GDOS_DBG_DETAIL("vx= %d setting rcm wheel velocity to: %d %d\n", vx, speedLeftL, speedRightL);
    
    hwMtx.lock(RACK_INFINITE);

    rcm_WheelSetVelocity_LR(speedLeftL, speedRightL);
    rcm_WheelUpdate();

/*    if (speedLeftL != 0)
    {
        PWDReply replyL , replyR; 
        GDOS_DBG_DETAIL("requesting position from er1\n");    
        replyL     = SendCmd(0,GetVelocity);
    //    replyR     = SendCmd(1,GetPostion);
        GDOS_DBG_DETAIL("outputting answer\n");    
        pwdreply_print(&replyL);
        GDOS_DBG_DETAIL("parsing answer\n");    
      //  pwdreply_print(&replyR);
        GDOS_DBG_DETAIL("returned int values: %d \n",pwdreply_data(&replyL));    
    }
*/
    hwMtx.unlock();

    return 0;
}


PWDCmd ChassisER1::SetCmd(unsigned char  paddress, unsigned char pcode)
{
    PWDCmd cmd;

    cmd.address=paddress;
    cmd.code=pcode;
    cmd.axis=0;

    cmd.checksum = ~(paddress + pcode) + 1;
    cmd.size=4;

    return cmd;
}

// paddress = [0|1] left channel or right channel, pcode = code, uwData
PWDCmd ChassisER1::SetCmd_with_arg(unsigned char  paddress, unsigned char  pcode,
                  int  wData1)
{
    PWDCmd cmd;

    cmd=SetCmd(paddress,pcode);
    cmd.data[0]=HIBYTE(wData1);
    cmd.data[1]=LOBYTE(wData1);

    cmd.checksum = ~(~(cmd.checksum-1)+ cmd.data[0] + cmd.data[1]) + 1;
    cmd.size = 6;

    return cmd;
}

//void PWDCmd::SetCmd(byte address, byte code, byte data, byte data)
PWDCmd ChassisER1::SetCmd_with_arg2(unsigned char paddress, unsigned char pcode,
                   int wData1, int wData2)
{
    PWDCmd cmd;

    cmd = SetCmd_with_arg(paddress, pcode, wData1);

    cmd.data[2]=HIBYTE(wData2);
    cmd.data[3]=LOBYTE(wData2);

    cmd.checksum = ~(~(cmd.checksum - 1) + cmd.data[2] + cmd.data[3]) + 1;
    cmd.size = 8;

    return cmd;
}

PWDReply ChassisER1::_SendCmd(PWDCmd *cmd)
{
    int n;
    PWDReply reply;

    reply.dwSize=0;
    memset(reply.data,0,PWMREPLYSIZE);

    if ( !rcm.nosend ) {
        if ( (n = write(rcm.handle, cmd, cmd->size)) > 0 ) {
            reply.dwSize=read(rcm.handle, reply.data, 2);
        }
    }

    if ( rcm.debug ) {
        printf("size= %d, cmd=%2x %2x %2x %2x %2x %2x %2x %2x %2x %2x\n",
            cmd->size, cmd->address, cmd->checksum, cmd->axis, cmd->code,
            cmd->data[0], cmd->data[1], cmd->data[2], cmd->data[3],
            cmd->data[4], cmd->data[5]);
        if ( !rcm.nosend ) {
            pwdreply_print(&reply);
        }
    }

    return reply;
}

PWDReply ChassisER1::SendCmd_with_arg2(unsigned char paddress, unsigned char pcode,
                  int wData1, int wData2)
{
    PWDCmd cmd;

    cmd=SetCmd_with_arg2(paddress, pcode, wData1, wData2);
    return _SendCmd(&cmd);
}

PWDReply ChassisER1::SendCmd_with_arg(unsigned char paddress, unsigned char pcode,
                 int wData1)
{
    PWDCmd cmd;

    cmd=SetCmd_with_arg(paddress, pcode, wData1);
    return _SendCmd(&cmd);
}

PWDReply ChassisER1::SendCmd(unsigned char paddress, unsigned char pcode)
{
    PWDCmd cmd;

    cmd = SetCmd(paddress, pcode);
    return _SendCmd(&cmd);
}

PWDReply ChassisER1::SendCmd_long(unsigned char paddress, unsigned char pcode, long dwData)
{
    return SendCmd_with_arg2(paddress,pcode,HIWORD(dwData),LOWORD(dwData));
}

void ChassisER1::rcm_WheelSetPower(long wValue)
{
    SendCmd_with_arg(1,SetMotorCommand,wValue);
    SendCmd_with_arg(0,SetMotorCommand,wValue);
}

void ChassisER1::rcm_WheelUpdate()
{
    SendCmd(1,Update);
    SendCmd(0,Update);
}

// Initalize two wheels
void ChassisER1::rcm_WheelInit()
{
    SendCmd(1,Reset);
    SendCmd_with_arg(1,SetLimitSwitchMode,0x0000);
    SendCmd_with_arg(1,SetMotorCommand,0x0000);
    SendCmd_with_arg(1,SetProfileMode,0x0001);
    SendCmd_long(1,SetAcceleration,0x000000B0);
    SendCmd_long(1,SetDeceleration,0x000000B0);
    SendCmd_long(1,SetJerk,0x03290ABB);

    SendCmd(0,Reset);
    SendCmd_with_arg(0,SetLimitSwitchMode,0x0000);
    SendCmd_with_arg(0,SetMotorCommand,0x0000);
    SendCmd_with_arg(0,SetProfileMode,0x0001);
    SendCmd_long(0,SetAcceleration,0x000000B0);
    SendCmd_long(0,SetDeceleration,0x000000B0);
    SendCmd_long(0,SetJerk,0x03290ABB);

    rcm_WheelUpdate();
}

void ChassisER1::rcm_WheelSetVelocity(long dwSpeed)
{
    long dwNegSpeed = ~dwSpeed + 1;

    rcm_WheelSetPower(0x4CC0);
    SendCmd_long(1,SetVelocity,dwNegSpeed);
    SendCmd_long(0,SetVelocity,dwSpeed);
}

void ChassisER1::rcm_WheelSetVelocity_LR(long left,long right){
  rcm_WheelSetPower(0x4CC0);
//  GDOS_DBG_DETAIL("setting velocity to: %d %d\n", left, right);
//  pwdreply_print(&SendCmd_long(1, SetVelocity, right));
//  pwdreply_print(&SendCmd_long(0, SetVelocity, -left));
  SendCmd_long(1, SetVelocity, right);
  SendCmd_long(0, SetVelocity, -left);
}

// +ve for left, -ve for right
void ChassisER1::rcm_WheelSetTurn(long dwSpeed)
{
    rcm_WheelSetPower(0x4CC0);
    SendCmd_long(1,SetVelocity,dwSpeed);
    SendCmd_long(0,SetVelocity,dwSpeed);
}


long ChassisER1::pwdreply_status(PWDReply *r)
{
    return r->data[STATUS];
}

long ChassisER1::pwdreply_checksum(PWDReply *r)
{
    unsigned char sum=0;
    int i;

    for ( i=0 ; i< r->dwSize ; i++ )
    {
        sum+=r->data[i];
    }

    return sum;
}

void ChassisER1::pwdreply_print(PWDReply *r)
{
    int i;

    printf("Reply Status=%2x, Checksum=%2x, size=%ld, Data=",
           r->data[STATUS], r->data[CHECKSUM], r->dwSize);
    for ( i = DATA_START ; i < r->dwSize ; i++ ){
        printf("%2x ",r->data[i]);
    }
    printf("\n");

}

int ChassisER1::pwdreply_data(PWDReply *r)
{
    char hexString[4+r->dwSize];
    int number; 
//    const char* rdata = (char*)  r->data;
    sscanf((char*)  r->data, "0x00%s", hexString ); 
    printf("hexString: ");
    printf(hexString);
    printf("\n");
    number = atoi(hexString);
    printf("int=%d \n", number);

    return number; 
}

