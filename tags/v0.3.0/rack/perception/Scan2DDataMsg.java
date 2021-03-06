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
package rack.perception;

import java.io.*;

import rack.main.defines.ScanPoint;
import rack.main.tims.exceptions.MsgException;
import rack.main.tims.msg.*;
import rack.main.tims.msgtypes.*;
import rack.main.tims.streams.*;

public class Scan2DDataMsg extends TimsMsg
{
    public int recordingtime = 0;
    public int duration = 0;
    public int maxRange = 0;
    public int pointNum = 0;
    public ScanPoint[] point;

    public int getDataLen()
    {
        return (16 + pointNum * ScanPoint.getDataLen());
    }

    public Scan2DDataMsg(TimsDataMsg p) throws MsgException
    {
        readTimsDataMsg(p);
    }

    protected boolean checkTimsMsgHead()
    {
        if (type == RackMsgType.MSG_DATA)
        {
            return true;
        }
        else
        {
            return false;
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
        pointNum = dataIn.readInt();

        point = new ScanPoint[pointNum];

        for (int i = 0; i < pointNum; i++)
        {
            point[i] = new ScanPoint(dataIn);
        }
        bodyByteorder = BIG_ENDIAN;
    }

    protected void writeTimsMsgBody(OutputStream out) throws IOException
    {
        DataOutputStream dataOut = new DataOutputStream(out);

        dataOut.writeInt(recordingtime);
        dataOut.writeInt(duration);
        dataOut.writeInt(maxRange);
        dataOut.writeInt(pointNum);

        for (int i = 0; i < pointNum; i++)
        {
            point[i].writeDataOut(dataOut);
        }
    }
}
