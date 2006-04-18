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
 *      Marko Reimer     <reimer@l3s.de>
 *      Joerg Langenberg <joerg.langenberg@gmx.net>
 *
 */
package rack.drivers;

/**
 *
 */
import java.io.IOException;
import java.io.File;
import java.awt.image.BufferedImage;
import javax.imageio.*;

import rack.main.naming.*;
import rack.main.proxy.*;
import rack.main.tims.msg.*;
import rack.main.tims.msgtypes.*;
import rack.main.tims.router.*;
import rack.main.tims.exceptions.*;

public class CameraProxy extends RackDataProxy {

	public static final int  MAX = 1;
    
    public static final byte MSG_CAMERA_GET_PARAMETER =
        RackMsgType.RACK_PROXY_MSG_POS_OFFSET + 1;

    public static final byte MSG_CAMERA_SET_FORMAT =
        RackMsgType.RACK_PROXY_MSG_POS_OFFSET + 2;
    
    public static final byte MSG_CAMERA_PARAMETER =
        RackMsgType.RACK_PROXY_MSG_NEG_OFFSET - 1;
    
    public static final byte MSG_CAMERA_FORMAT =
        RackMsgType.RACK_PROXY_MSG_NEG_OFFSET - 2;
    

	public CameraProxy(int id, int replyMbx)
    {
    	super(RackName.create(RackName.CAMERA, id), replyMbx, 5000, 1000, 5000);
    	this.id = id;
    }
    public void storeDataToFile(String filename)
	{
    	CameraDataMsg data = getData();

		if(data != null)
		{
			try{
				System.out.println("Store image data filename=" + filename);
			    BufferedImage image = new BufferedImage(data.width, data.height, BufferedImage.TYPE_INT_RGB);// the image to be stored //";
			    image.setRGB(0, 0, data.width, data.height, data.imageRawData, 0, data.width);
			    File file = new File(filename);
		        ImageIO.write(image, "png", file);
			} catch(IOException e) {
				System.out.println("Error storing image filename=" + filename + e.toString());
			}
		}
	}
    public synchronized CameraDataMsg getData(int recordingtime)
    {
        try
        {
            TimsDataMsg raw = getRawData(recordingtime);

        	if(raw != null)
        	{
                CameraDataMsg data = new CameraDataMsg(raw);
				return data;
        	}
        	else
        	{
        		return null;
        	}
        }
        catch(MsgException e)
        {
            System.out.println(e.toString());
            return null;
        }
    }

    public synchronized CameraDataMsg getData()
    {
        return(getData(0));
    }

	public synchronized void setFormat(CameraFormatMsg format)
	{
		currentSequenceNo++;
		System.out.println(format);

		try {
			TimsMsgRouter.send(
				MSG_CAMERA_SET_FORMAT,
				commandMbx,
				replyMbx,
				(byte)0,
				(byte)currentSequenceNo,
				format);

			TimsDataMsg reply;

			do {
				reply = TimsMsgRouter.receive(replyMbx, dataTimeout);
			} while (reply.seq_nr != currentSequenceNo);

			if (reply.type == RackMsgType.MSG_OK) {
				System.out.println(
                        RackName.nameString(replyMbx) + ": cameraProxy setFormat");
			} else {
				System.out.println(
					RackName.nameString(replyMbx)
						+ ": "
						+ RackName.nameString(commandMbx)
						+ ".setFormat replied error");
			}
		} catch (MsgException e) {
			System.out.println(
				RackName.nameString(replyMbx) + ": cameraProxy setFormat " + e);
		}
	}

	public int getCommandMbx()
	{
		return(RackName.create(RackName.CAMERA, id));
	}
}
