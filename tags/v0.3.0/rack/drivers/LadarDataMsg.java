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
package rack.drivers;

import java.io.*;
import rack.main.tims.msg.*;
import rack.main.tims.msgtypes.*;
import rack.main.tims.exceptions.*;
import rack.main.tims.streams.*;

public class LadarDataMsg extends TimsMsg
{
    public int recordingtime = 0;
    public int duration = 0;
    public int maxRange = 0;
    public float startAngle = 0.0f;
    public float angleResolution = 0.0f;
    public int distanceNum = 0;
    public int[] distance = new int[0];

    public int getDataLen()
    {
        return (24 + distanceNum * 4);
    }

    public LadarDataMsg()
    {
        msglen = headLen + getDataLen();
    }

    public LadarDataMsg(TimsDataMsg p) throws MsgException
    {
        readTimsDataMsg(p);
    }

    protected boolean checkTimsMsgHead()
    {
        if (type == RackMsgType.MSG_DATA)
        {
            return (true);
        }
        else
        {
            return (false);
        }
    }

    protected void readTimsMsgBody(InputStream in) throws IOException
    {
        EndianDataInputStream dataIn;

        if (bodyByteorder == BIG_ENDIAN)
        {
            dataIn = new BigEndianDataInputStream(in);
        }
        else
        {
            dataIn = new LittleEndianDataInputStream(in);
        }

        recordingtime = dataIn.readInt();
        duration = dataIn.readInt();
        maxRange = dataIn.readInt();
        startAngle = dataIn.readFloat();
        angleResolution = dataIn.readFloat();
        distanceNum = dataIn.readInt();
        distance = new int[distanceNum];
        for (int i = 0; i < distanceNum; i++)
        {
            distance[i] = dataIn.readInt();
        }

        bodyByteorder = BIG_ENDIAN;
    }

    protected void writeTimsMsgBody(OutputStream out) throws IOException
    {
        DataOutputStream dataOut = new DataOutputStream(out);

        dataOut.writeInt(recordingtime);
        dataOut.writeInt(duration);
        dataOut.writeInt(maxRange);
        dataOut.writeFloat(startAngle);
        dataOut.writeFloat(angleResolution);
        dataOut.writeInt(distanceNum);

        for (int i = 0; i < distanceNum; i++)
        {
            dataOut.writeInt(distance[i]);
        }
    }
}
