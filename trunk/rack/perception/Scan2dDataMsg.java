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

import rack.main.RackProxy;
import rack.main.defines.ScanPoint;
import rack.main.defines.Position3d;
import rack.main.tims.*;

public class Scan2dDataMsg extends TimsMsg
{
    public int recordingTime = 0;
    public int duration = 0;
    public int maxRange = 0;
    public int pointNum = 0;
    public Position3d refPos = new Position3d();  
    public ScanPoint[] point;

    public int getDataLen()
    {
        return (16 + Position3d.getDataLen() + pointNum * ScanPoint.getDataLen());
    }

    public Scan2dDataMsg(TimsRawMsg p) throws TimsException
    {
        readTimsRawMsg(p);
    }

    public boolean checkTimsMsgHead()
    {
        if (type == RackProxy.MSG_DATA)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    public void readTimsMsgBody(InputStream in) throws IOException
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

        recordingTime = dataIn.readInt();
        duration = dataIn.readInt();
        maxRange = dataIn.readInt();
        pointNum = dataIn.readInt();
        refPos.readData(dataIn);

        point = new ScanPoint[pointNum];
        
        for (int i = 0; i < pointNum; i++)
        {
            point[i] = new ScanPoint(dataIn);
        }
        bodyByteorder = BIG_ENDIAN;
    }

    public void writeTimsMsgBody(OutputStream out) throws IOException
    {
        DataOutputStream dataOut = new DataOutputStream(out);

        dataOut.writeInt(recordingTime);
        dataOut.writeInt(duration);
        dataOut.writeInt(maxRange);
        dataOut.writeInt(pointNum);
        
        refPos.writeData(dataOut);

        for (int i = 0; i < pointNum; i++)
        {
            point[i].writeDataOut(dataOut);
        }
    }
}
