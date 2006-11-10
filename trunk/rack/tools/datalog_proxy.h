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
 *      Oliver Wulf        <oliver.wulf@gmx.de>
 *      Matthias Hentschel <hentschel@rts.uni-hannover.de>
 */

#ifndef __DATALOG_PROXY_H__
#define __DATALOG_PROXY_H__

/*!
 * @ingroup tools
 * @defgroup datalog RackDatalog
 *
 * Data strcture for the rack datalogger
 *
 * @{
 */

#include <main/rack_proxy.h>
#include <main/rack_name.h>

#define DATALOG_LOGNUM_MAX 40

//######################################################################
//# Datalog Message Types
//######################################################################

#define MSG_DATALOG_SET_LOG             (RACK_PROXY_MSG_POS_OFFSET + 1)
#define MSG_DATALOG_GET_LOG_STATUS      (RACK_PROXY_MSG_POS_OFFSET + 2)
#define MSG_DATALOG_RESET               (RACK_PROXY_MSG_POS_OFFSET + 3)
#define MSG_DATALOG_LOG_STATUS          (RACK_PROXY_MSG_NEG_OFFSET - 1)


//######################################################################
//# Datalog Data (static size  - MESSAGE)
//######################################################################

typedef struct {
    rack_time_t recordingTime;  // have to be first element
    int32_t     dataLogged;
} __attribute__((packed)) datalog_data;

class DatalogData
{
    public:
        static void le_to_cpu(datalog_data *data)
        {
            data->recordingTime = __le32_to_cpu(data->recordingTime);
            data->dataLogged    = __le32_to_cpu(data->dataLogged);
        }

        static void be_to_cpu(datalog_data *data)
        {
            data->recordingTime = __be32_to_cpu(data->recordingTime);
            data->dataLogged    = __be32_to_cpu(data->dataLogged);
        }

        static datalog_data* parse(message_info *msgInfo)
        {
            if (!msgInfo->p_data)
                return NULL;

            datalog_data *p_data = (datalog_data *)msgInfo->p_data;

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
//# Datalog Log Info (static size - MESSAGE)
//######################################################################
typedef struct {
    int32_t         logEnable;
    uint32_t        moduleMbx;
    rack_time_t     periodTime;
    uint8_t         filename[40];
} __attribute__((packed)) datalog_info;

class DatalogInfo
{
    public:
        static void le_to_cpu(datalog_info *data)
        {
            data->logEnable  = __le32_to_cpu(data->logEnable);
            data->moduleMbx  = __le32_to_cpu(data->moduleMbx);
            data->periodTime = __le32_to_cpu(data->periodTime);
        }

        static void be_to_cpu(datalog_info *data)
        {
            data->logEnable  = __be32_to_cpu(data->logEnable);
            data->moduleMbx  = __be32_to_cpu(data->moduleMbx);
            data->periodTime = __be32_to_cpu(data->periodTime);
        }
};


//######################################################################
//# DatalogInfoData (!!! VARIABLE SIZE !!! MESSAGE !!!)
//######################################################################

/* CREATING A MESSAGE :

typedef struct {
    datalog_info_data   data;
    datalog_info        logInfo[ ... ];
} __attribute__((packed)) datalog_info_data_msg;

datalog_info_data_msg msg;

ACCESS: msg.data.logInfo[...] OR msg.logInfo[...];
*/

typedef struct {
    int32_t       logNum;
    datalog_info  logInfo[0];
} __attribute__((packed)) datalog_info_data;

class DatalogInfoData
{
    public:
        static void le_to_cpu(datalog_info_data *data)
        {
            int i;
            data->logNum     = __le32_to_cpu(data->logNum);
            for (i = 0; i < data->logNum; i++)
            {
                DatalogInfo::le_to_cpu(&data->logInfo[i]);
            }
        }

        static void be_to_cpu(datalog_info_data *data)
        {
            int i;
            data->logNum     = __be32_to_cpu(data->logNum);
            for (i = 0; i < data->logNum; i++)
            {
                DatalogInfo::be_to_cpu(&data->logInfo[i]);
            }
        }

        static datalog_info_data* parse(message_info *msgInfo)
        {
            if (!msgInfo->p_data)
                return NULL;

            datalog_info_data *p_data = (datalog_info_data *)msgInfo->p_data;

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
//# Datalog Proxy Functions
//######################################################################

class DatalogProxy : public RackDataProxy {

    public:

//
// constructor / destructor
// WARNING -> look at module class id in constuctor
//
    DatalogProxy(RackMailbox *workMbx, uint32_t sys_id, uint32_t instance)
            : RackDataProxy(workMbx, sys_id, SCAN2D, instance)
    {
    };

    ~DatalogProxy()
    {
    };


//
// overwriting getData proxy function
// (includes parsing and type conversion)
//
    int getData(datalog_data *recv_data, ssize_t recv_datalen,
                rack_time_t timeStamp)
    {
        return getData(recv_data, recv_datalen, timeStamp, dataTimeout);
    }

    int getData(datalog_data *recv_data, ssize_t recv_datalen,
                rack_time_t timeStamp, uint64_t reply_timeout_ns);



// getLogStatus
    int getLogStatus(datalog_info_data *recv_data, ssize_t recv_datalen)
    {
        return getLogStatus(recv_data, recv_datalen, dataTimeout);
    }

    int getLogStatus(datalog_info_data *recv_data, ssize_t recv_datalen,
                     uint64_t reply_timeout_ns);


// setLog
    int setLog(datalog_info_data *recv_data, ssize_t recv_datalen)
    {
        return setLog(recv_data, recv_datalen, dataTimeout);
    }

    int setLog(datalog_info_data *recv_data, ssize_t recv_datalen,
               uint64_t reply_timeout_ns);


// reset
    int reset()
    {
        return reset(dataTimeout);
    }

    int reset(uint64_t reply_timeout_ns);
};

/*@}*/

#endif // __DATALOG_PROXY_H__
