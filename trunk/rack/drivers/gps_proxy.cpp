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
 *      Matthias Hentschel      <hentschel@rts.uni-hannover.de>
 *
 */
 #include <drivers/gps_proxy.h>

 int GpsProxy::getData(gps_data *recv_data, ssize_t recv_datalen,
                        rack_time_t timeStamp, uint64_t reply_timeout_ns)
{
    message_info msgInfo;

    int ret = RackDataProxy::getData((void *)recv_data, recv_datalen,
                                     timeStamp, reply_timeout_ns, &msgInfo);
    if (ret)
    {
        return ret;
    }

    recv_data = GpsData::parse(&msgInfo);
    return 0;
}
