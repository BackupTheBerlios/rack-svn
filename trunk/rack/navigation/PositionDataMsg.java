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
package rack.navigation;

import java.io.*;

import rack.main.RackProxy;
import rack.main.tims.*;

import rack.main.defines.Position3d;

public class PositionDataMsg extends TimsMsg
{
    public int recordingTime = 0;
    public Position3d pos = new Position3d();
    public Position3d var = new Position3d();

    public int getDataLen()
    {
        return (4 + 2 * Position3d.getDataLen());
    }

    public PositionDataMsg()
    {
        msglen = HEAD_LEN + getDataLen();
    }

    public PositionDataMsg(TimsRawMsg p) throws TimsException
    {
        readTimsRawMsg(p);
    }

    public PositionDataMsg(Position3d pos)
    {
        msglen = HEAD_LEN + getDataLen();
        
        this.pos = pos;
        this.var = new Position3d(1000, 1000, Integer.MAX_VALUE, Float.POSITIVE_INFINITY, Float.POSITIVE_INFINITY, (float)Math.toRadians(10.0));
    }

    public PositionDataMsg(Position3d pos, Position3d var)
    {
        msglen = HEAD_LEN + getDataLen();
        
        this.pos = pos;
        this.pos = var;
    }

    public boolean checkTimsMsgHead()
    {
        if (type == RackProxy.MSG_DATA &&
            msglen == HEAD_LEN + getDataLen())
        {
            return true;
        }
        else
            return false;
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
        pos.readData(dataIn);
        var.readData(dataIn);

        bodyByteorder = BIG_ENDIAN;
    }

    public void writeTimsMsgBody(OutputStream out) throws IOException
    {
        DataOutputStream dataOut = new DataOutputStream(out);

        dataOut.writeInt(recordingTime);
        pos.writeData(dataOut);
        var.writeData(dataOut);
    }
}
