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

public class JoystickProxy extends RackDataProxy
{
    public JoystickProxy(int id, TimsMbx replyMbx)
    {
        super(RackName.create(RackName.JOYSTICK, id), replyMbx, 1000);
    }

    public synchronized JoystickDataMsg getData(int recordingTime)
    {
        try
        {
            TimsRawMsg raw = getRawData(recordingTime);
            if (raw != null)
            {
                JoystickDataMsg data = new JoystickDataMsg(raw);
                return data;
            }
            else
            {
                return null;
            }
        }
        catch (TimsException e)
        {
            System.out.println(e.toString());
            return null;
        }
    }

    public synchronized JoystickDataMsg getData()
    {
        return (getData(0));
    }
}
