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
 *      Oliver Wulf <wulf@rts.uni-hannover.de>
 *
 */
#ifndef __SCAN_2D_SIM_H__
#define __SCAN_2D_SIM_H__

#include <main/rack_data_module.h>
#include <main/dxf_map.h>
#include <perception/scan2d_proxy.h>
#include <drivers/odometry_proxy.h>

#define MODULE_CLASS_ID             SCAN2D

//######################################################################
//# class Scan2DSim
//######################################################################

class Scan2dSim : public RackDataModule {
    private:
        int          odometryInst;
        int          maxRange;
        int          mapOffsetX;
        int          mapOffsetY;
        DxfMap       dxfMap;
        char         *dxfMapFile;
        int          angleRes;

        // additional mailboxes
        RackMailbox workMbx;
        RackMailbox odometryMbx;

        // proxies
        OdometryProxy  *odometry;

        // buffer
        odometry_data  odometryData;

    protected:
        // -> realtime context
        int  moduleOn(void);
        void moduleOff(void);
        int  moduleLoop(void);
        int  moduleCommand(message_info *msgInfo);

        // -> non realtime context
        void moduleCleanup(void);

      public:
        // constructor und destructor
        Scan2dSim();
        ~Scan2dSim() {};

        // -> non realtime context
        int  moduleInit(void);
};

#endif // __SCAN_2D_SIM_H__
