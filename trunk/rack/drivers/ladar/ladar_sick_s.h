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
 *      Daniel Lecking   <lecking@rts.uni-hannover.de>
 *      Oliver Wulf      <wulf@rts.uni-hannover.de>
 *
 */
#ifndef __LADAR_SICK_S_H__
#define __LADAR_SICK_S_H__

#include <main/rack_data_module.h>
#include <main/serial_port.h>

#include <drivers/ladar_proxy.h>

#define MODULE_CLASS_ID         LADAR


//######################################################################
//# class LadarSick
//######################################################################

class LadarSickS : public RackDataModule {
    private:

        SerialPort  serialPort;
        int devNumber;
        int EFIdev;
        int serialDev;
        int baudrate;
        double scanningAngle;

        unsigned short crc_check(unsigned char* data, int len);

    protected:
        // -> realtime context
        int      moduleOn(void);
        void     moduleOff(void);
        int      moduleLoop(void);
        int      moduleCommand(message_info *p_msginfo);

        // -> non realtime context
        void     moduleCleanup(void);

    public:
        // constructor und destructor
        LadarSickS();
        ~LadarSickS() {};

        // -> non realtime context
        int  moduleInit(void);
};
#endif
