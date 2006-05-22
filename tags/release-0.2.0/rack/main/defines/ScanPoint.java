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
package rack.main.defines;

import java.io.DataOutputStream;
import java.io.IOException;
import rack.main.tims.streams.EndianDataInputStream;

public class ScanPoint {
    
    public static final int TYPE_INVALID          = 0x0010;
    public static final int TYPE_MAX_RANGE        = 0x0020;
    public static final int TYPE_REDUCE           = 0x0040;
    public static final int TYPE_REFLECTOR        = 0x0080;

    public static final int TYPE_HOR_EDGE         = 0x0008;  // 3d scan
    public static final int TYPE_VER_EDGE         = 0x0004;  // 3d scan
    public static final int TYPE_EDGE             = 0x0004;  // 2d scan

    public static final int TYPE_MASK             = 0x0003;

    public static final int TYPE_UNKNOWN          = 0x0000;
    public static final int TYPE_OBJECT           = 0x0001;  // 3d scan
    public static final int TYPE_GROUND           = 0x0002;  // 3d scan
    public static final int TYPE_CEILING          = 0x0003;  // 3d scan
    public static final int TYPE_LANDMARK         = 0x0001;  // 2d scan
    public static final int TYPE_OBSTACLE         = 0x0002;  // 2d scan

    public short x              = 0;    // 2 Bytes
    public short y              = 0;    // 2 Bytes
    public short z              = 0;    // 2 Bytes
    public short type           = 0;    // 2 Bytes
    public short segment        = 0;    // 2 Bytes
    public short intensity      = 0;    // 2 Bytes --> 12 Bytes completely
    
    static public int getDataLen()
    {
        return 12;
    }

    /**
     * @param dataIn
     */
    public ScanPoint(EndianDataInputStream dataIn) throws IOException
    {
        x              = dataIn.readShort();
        y              = dataIn.readShort();
        z              = dataIn.readShort();
        type           = dataIn.readShort();
        segment        = dataIn.readShort();
        intensity      = dataIn.readShort();
    }
    
    /**
     * @param dataOut
     */
    public void writeDataOut(DataOutputStream dataOut) throws IOException
    {
        dataOut.writeShort(x);
        dataOut.writeShort(y);
        dataOut.writeShort(z);
        dataOut.writeShort(type);
        dataOut.writeShort(segment);
        dataOut.writeShort(intensity);
    }
    
    public String toString()
    {
        return x + " " + y + " " + z + " " + type + " " + segment + " " + intensity;
    }
}
