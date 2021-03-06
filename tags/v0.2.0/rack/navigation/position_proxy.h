/*
 * RACK - Robotics Application Construction Kit
 * Copyright (C) 2005-2006 University of Hannover
 *                         Institute for Systems Engineering - RTS
 *                         Professor Bernardo Wagner
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Authors
 *      Joerg Langenberg <joerg.langenberg@gmx.net>
 *
 */
#ifndef __POSITION_PROXY_H__
#define __POSITION_PROXY_H__

#include <main/rack_proxy.h>
#include <navigation/position_proxy.h>
#include <main/defines/position3d.h>

//######################################################################
//# Position Message Types
//######################################################################

#define MSG_POSITION_UPDATE              (RACK_PROXY_MSG_POS_OFFSET + 1)

//######################################################################
//# Position Data (static size - MESSAGE)
//######################################################################

typedef struct{
    RACK_TIME    recordingTime; // have to be first element
    position_3d  pos;
} __attribute__((packed)) position_data;

class PositionData
{
    public:
        static void le_to_cpu(position_data *data)
        {
            data->recordingTime = __le32_to_cpu(data->recordingTime);
            Position3D::le_to_cpu(&data->pos);
        }

        static void be_to_cpu(position_data *data)
        {
            data->recordingTime = __be32_to_cpu(data->recordingTime);
            Position3D::be_to_cpu(&data->pos);
        }

        static position_data* parse(MessageInfo *msgInfo)
        {
            if (!msgInfo->p_data)
                return NULL;

            position_data *p_data = (position_data *)msgInfo->p_data;

            if (msgInfo->flags & MSGINFO_DATA_LE) // data in little endian
            {
                le_to_cpu(p_data);
            }
            else // data in big endian
            {
                be_to_cpu(p_data);
            }
            msgInfo->usedMbx->setDataByteorder(msgInfo);
            return p_data;
        }
};

//######################################################################
//# Position Proxy Functions
//######################################################################

class PositionProxy : public RackDataProxy {

    public:

        PositionProxy(RackMailbox *workMbx, uint32_t sys_id, uint32_t instance)
              : RackDataProxy(workMbx, sys_id, POSITION, instance)
        {
        };

        ~PositionProxy()
        {
        };

        //
        // overwriting getData (includes parsing and type conversion)
        //

        int getData(position_data *recv_data, ssize_t recv_datalen,
                    RACK_TIME timeStamp, uint64_t reply_timeout_ns,
                    MessageInfo *msgInfo);

        int getData(position_data *recv_data, ssize_t recv_datalen,
                    RACK_TIME timeStamp, MessageInfo *msgInfo)
        {
            return getData(recv_data, recv_datalen, timeStamp, dataTimeout, msgInfo);
        }

        int update(position_3d *pos, RACK_TIME recordingTime, uint64_t reply_timeout_ns);

        int update(position_3d *pos, RACK_TIME recordingTime)
        {
            return update(pos, recordingTime, dataTimeout);
        }
};

#endif // __POSITION_PROXY_H__
