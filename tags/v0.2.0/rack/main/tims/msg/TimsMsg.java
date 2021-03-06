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
package rack.main.tims.msg;

import java.io.*;
import rack.main.tims.msgtypes.*;
import rack.main.tims.exceptions.*;
import rack.main.tims.streams.*;
import rack.main.naming.*;

  /* TIMS message bytes in tims.h
  __u8          flags;     // 1 Byte: flags
  __s8          type;      // 1 Byte: Message Type
  __s8          priority;  // 1 Byte: Priority
  __u8          seq_nr;    // 1 Byte: Sequence Number
  __u32         dest;      // 4 Byte: Destination ID
  __u32         src;       // 4 Byte: Source ID
  __u32         msglen;    // 4 Byte: length of complete message
  __u8          data[0];   // 0 Byte: following data
  */

public abstract class TimsMsg
{
    public static final int SP = 0; // sender priority
    public static final int RP = 1; // receiver priority

    protected static final int BIG_ENDIAN    = 0; // network byteorder
    protected static final int LITTLE_ENDIAN = 1; // Intel byteorder

    public static final int headLen = 16; // length of message-head
    public static final int MAX_BODY_LENGTH = 65536; // 64kb, max length of message-body

    protected int   headByteorder = BIG_ENDIAN;
    protected int   bodyByteorder = BIG_ENDIAN;

    public    byte  type     = RackMsgType.MSG_ERROR;
    public    byte  priority = 0;   // 0 = less important message
    public    byte  seq_nr   = 0;
    public    int   dest     = 0;
    public    int   src      = 0;
    protected int   msglen   = 0;

    public TimsMsg()
    {
      msglen = headLen;
    }

    public TimsMsg(TimsDataMsg p) throws MsgException
    {
      readTimsDataMsg(p);
    }

    protected TimsMsg(InputStream in) throws IOException
    {
      readTimsMsg(in);
    }

    public void readTimsMsg(InputStream in) throws IOException
    {
        readTimsMsgHead(in);

        if (checkTimsMsgHead() == false) {
            // head doesn't fit to TimsMsg class, save message as TimsDataMsg
            System.out.println("TimsMsg: head is not OK");
            new TimsDataMsg(this, in);
        }

        readTimsMsgBody(in);
    }

    public void readTimsDataMsg(TimsDataMsg p) throws MsgException
    {
      if (p == null) {
        throw(new MsgTypeException("TimsMsg: Message is null"));
      }

      // copy message head
      headByteorder = p.headByteorder;
      bodyByteorder = p.bodyByteorder;
      priority      = p.priority;
      seq_nr        = p.seq_nr;
      type          = p.type;
      dest          = p.dest;
      src           = p.src;
      msglen        = p.msglen;

      if (checkTimsMsgHead() == false) {
          throw(new MsgTypeException("TimsMsg: Message head doesn't fit\n   "
                                          + this.toString()));
      }

      try {
          readTimsMsgBody(new ByteArrayInputStream(p.body));
      } catch(IOException e) {
          throw(new MsgTypeException("TimsMsg: Message body doesn't fit\n    "
                                          + this.toString() + "\n" +
                                          e.toString()));
      }
    }

    public void writeTimsMsg(BufferedOutputStream out) throws IOException
    {
      msglen = headLen + getDataLen();

      writeTimsMsgHead(out);
      writeTimsMsgBody(out);
      out.flush();
    }

    protected void readTimsMsgHead(InputStream in) throws IOException
    {
      int flags = in.read();
      EndianDataInputStream dataIn;

      headByteorder = (flags) & 0x01;
      bodyByteorder = (flags >> 1) & 0x01;

      if (headByteorder == BIG_ENDIAN) {
        dataIn = new BigEndianDataInputStream(in);
      } else {
        dataIn = new LittleEndianDataInputStream(in);
      }

      type      = dataIn.readByte();
      priority  = dataIn.readByte();
      seq_nr    = dataIn.readByte();
      dest      = dataIn.readInt();
      src       = dataIn.readInt();
      msglen    = dataIn.readInt();

      headByteorder = BIG_ENDIAN;
    }

    protected void writeTimsMsgHead(OutputStream out) throws IOException
    {
      DataOutputStream dataOut = new DataOutputStream(out);

      int flags = headByteorder + bodyByteorder * 0x01;

      dataOut.writeByte(flags);
      dataOut.writeByte(type);
      dataOut.writeByte(priority);
      dataOut.writeByte(seq_nr);
      dataOut.writeInt(dest);
      dataOut.writeInt(src);
      dataOut.writeInt(msglen);
    }

    protected abstract boolean checkTimsMsgHead();

    protected abstract void readTimsMsgBody(InputStream in) throws IOException;

    protected abstract void writeTimsMsgBody(OutputStream out) throws IOException;

    public    abstract int getDataLen();

    public String toString()
    {
      return("seq_no:" + seq_nr + " type:" + RackMsgType.toString(type) + " " +
             RackName.string(src) + " -> " + RackName.string(dest) +
             " msglen:" + msglen + " priority:" + priority + " flags:" +
            (headByteorder + bodyByteorder * 0x01) );
    }
}
