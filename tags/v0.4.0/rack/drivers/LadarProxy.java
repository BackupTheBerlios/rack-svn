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

import rack.main.*;
import rack.main.tims.*;

public class LadarProxy extends RackDataProxy
{
    public LadarProxy(int id, TimsMbx replyMbx)
    {
        super(RackName.create(RackName.LADAR, id), replyMbx, 10000, 5000, 1000);
        this.id = id;
    }

    public synchronized LadarDataMsg getData(int recordingTime)
    {
        try
        {
            TimsDataMsg raw = getRawData(recordingTime);

            if (raw != null)
            {
                LadarDataMsg data = new LadarDataMsg(raw);
                return (data);
            }
            else
            {
                return (null);
            }

        }
        catch (TimsException e)
        {
            System.out.println(e.toString());
            return (null);
        }
    }

    public synchronized LadarDataMsg getData()
    {
        return (getData(0));
    }

    public int getCommandMbx()
    {
        return (RackName.create(RackName.LADAR, id));
    }
}
