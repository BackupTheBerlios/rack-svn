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
#ifndef __SERIAL_PORT_H__
#define __SERIAL_PORT_H__

#include <main/rack_time.h>
#include <rtdm/rtserial.h>

//######################################################################
//# class SerialPort
//######################################################################

class SerialPort
{
    private:

    protected:

        int fd;

    public:

        SerialPort();
        ~SerialPort();

        int open(int dev, const rtser_config *config);
        int close(void);

        int setConfig(const rtser_config *config);
        int setBaudrate(int baudrate);
        int setRxTimeout(int64_t timeout);
        int setEventTimeout(int64_t timeout);

        int send(const void* data, int dataLen);

        int recv(void *data, int dataLen);
        int recv(void *data, int dataLen, RACK_TIME *timestamp);
        int recv(void *data, int dataLen, RACK_TIME *timestamp,
                 int64_t timeout_ns);

        int recv_pending(void *data, int maxdataLen);

        int clean(void);
};

#endif // __SERIAL_PORT_H__
