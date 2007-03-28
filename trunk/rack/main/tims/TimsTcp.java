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
 *      Oliver Wulf <wulf@rts.uni-hannover.de>
 *
 */
package rack.main.tims;

import java.net.*;
import java.io.*;
import java.util.*;

public class TimsTcp extends Tims
{
    protected Socket                socket = null;
    protected InputStream           tcpIn = null;
    protected BufferedOutputStream  tcpOut = null;
    protected TimsMbx               routerMbx;
    protected Object                dataCountSync = new Object();
    protected Vector                mbxList = new Vector();
    protected InetAddress           addr;
    protected int                   port;
    protected int                   dataCount;
    protected long                  dataCountTime;


    public TimsTcp(InetAddress addr, int port) throws TimsException
    {
      this.addr = addr;
      this.port = port;

      try {
        socket = new Socket(addr, port);  // open socket connection to TimsMsgGateway
        socket.setSoTimeout(10000);
        socket.setTcpNoDelay(true);
        tcpOut = new BufferedOutputStream(socket.getOutputStream());
        tcpIn  = socket.getInputStream();

        // init MBX 0 for communication between Tims and TimsRouter
        routerMbx = new TimsMbx(0, this);
        mbxList.addElement(routerMbx);

        start();

      } catch(IOException e) {

        tcpOut = null;
        tcpIn  = null;

        if (socket != null) {
          try {
            socket.close();
          } catch(IOException ee) {}

          socket = null;
        }
        throw(new TimsException("Can't connect to TimsRouter. " + e.toString()));
      }
    }

    synchronized void send(TimsMsg m) throws TimsException
    {
//    System.out.println(/*"TimsRouter " + */p);

      BufferedOutputStream out = tcpOut;

      if (out == null) {
        throw(new TimsException("No connection to TimsRouter"));
      }

      try {
        m.writeTimsMsg(out);
      } catch(IOException e) {
        throw(new TimsException("Can't send message to TimsRouter. " + e.toString()));
      }
    }

    TimsDataMsg receive(TimsMbx mbx, int timeout) throws TimsException
    {
        TimsDataMsg p;
        synchronized(mbx) {
        
          if (mbx.isEmpty()) {
            try {
              mbx.wait(timeout);
            } catch(InterruptedException e) {}
          }
        
          if (mbx.isEmpty()) {
            throw(new TimsTimeoutException("Receive timeout"));
          } else {
            p = (TimsDataMsg)mbx.remove(0);
            return p;
          }
        
        }
    }

    public void run()
    {
      TimsDataMsg m;
      TimsMbx     mbx;
      InputStream in   = tcpIn;
      Socket      sock = socket;

      while (terminate == false) {

        // normal operation
        try {

          while(in != null) {
            m = new TimsDataMsg(in);

            if ((m.dest == 0) && (m.src == 0) &&
                (m.type == TimsRouter.GET_STATUS)) {

              // reply to lifesign
              try {
                  TimsDataMsg rm = new TimsDataMsg();
                  rm.type     = Tims.MSG_OK;
                  rm.dest     = 0;
                  rm.src      = 0;
                  rm.priority = m.priority;
                  rm.seqNr    = m.seqNr;
                  send(rm);
              } catch (TimsException e1) {}

            } else {
              synchronized(dataCountSync) {
                dataCount += m.getDataLen();
              }

              mbx = getMbx(m.dest);
              if (mbx != null) {
                synchronized(mbx) {
                  mbx.addElement(m);
                  mbx.notifyAll();
                }
              } else {
                System.out.println("Tims received message for unknown mbx " + m.dest);
              }
            }
            in   = tcpIn;
            sock = socket;
          }
        }  catch(IOException e) {
          System.out.println("Tims " + e.getMessage());

          try {
            if (sock != null)
              sock.close();
            socket = null;
            tcpIn  = null;
            tcpOut = null;
          } catch (IOException e1) {}

        } catch (Throwable t) {
          System.out.println("Tims " + t);
          t.printStackTrace();
        }

        // try to reconnect to TimsRouterTcp
        if (tcpIn == null) {
          synchronized(mbxList) {
            try {
              socket = new Socket(addr, port);  // open socket connection to TimsMsgGateway
              socket.setSoTimeout(10000);
              socket.setTcpNoDelay(true);
              tcpOut = new BufferedOutputStream(socket.getOutputStream());
              tcpIn  = socket.getInputStream();

              TimsRouterMbxMsg initMbxM = new TimsRouterMbxMsg();

              for(int i = 0; i < mbxList.size(); i++) {
                initMbxM.mbx = ((TimsMbx)mbxList.elementAt(i)).name;

                initMbxM.type     = TimsRouter.MBX_INIT;
                initMbxM.dest     = 0;
                initMbxM.src      = 0;

                send(initMbxM);
              }

              System.out.println("Tims reconnected to " + addr.getHostAddress() + ":" + port);

            } catch(Exception e) {

              System.out.println("Tims " + e.getMessage());

              tcpOut = null;
              tcpIn  = null;

              if (socket != null) {
                try {
                  socket.close();
                } catch(IOException ee) {}

                socket = null;
              }
            }

            in   = tcpIn;
            sock = socket;

            try {
              sleep(1000);
            } catch (InterruptedException e1) {}
          }
        }
      }
      System.out.println("TimsTcp terminated");
    }

    public void terminate()
    {
        super.terminate();
        
        // close connection to router to terminate

        tcpIn  = null;
        tcpOut = null;

        if (socket != null)
        {
            try
            {
                socket.close();
            }
            catch (IOException e) {}
        }
        
        socket = null;
        
        try
        {
            this.interrupt();
            this.join(1000);
        }
        catch (Exception e) {}
    }

    public synchronized TimsMbx mbxInit(int mbxName) throws TimsException
    {
        TimsMbx mbx;
        TimsRouterMbxMsg m = new TimsRouterMbxMsg();
        TimsDataMsg reply;

        m.type  = TimsRouter.MBX_INIT_WITH_REPLY;
        m.dest  = 0;
        m.src   = 0;
        m.mbx   = mbxName;

        routerMbx.clear();
        routerMbx.send(m);

        reply = routerMbx.receive(1000);
        
        if ((reply != null) &&
            (reply.type == Tims.MSG_OK)) {
        
          synchronized(mbxList) {
            mbx = new TimsMbx(mbxName, this);
            mbxList.addElement(mbx);
          }
        
        } else {
        
          throw(new TimsException("Can't init mbx " +
                                  Integer.toHexString(mbxName) +
                                  ". Already initialised"));
        }
        return mbx;
    }

    public synchronized void mbxDelete(TimsMbx mbx) throws TimsException
    {
        TimsRouterMbxMsg m = new TimsRouterMbxMsg();
        TimsDataMsg reply;

        m.type  = TimsRouter.MBX_DELETE_WITH_REPLY;
        m.dest  = 0;
        m.src   = 0;
        m.mbx   = mbx.name;
        
        routerMbx.clear();
        routerMbx.send(m);

        reply = routerMbx.receive(1000);
        
        if ((reply != null) && (reply.type == Tims.MSG_OK))
        {
            synchronized (mbxList)
            {
                mbxList.removeElement(mbx);
                //System.out.println("remove from mbxList: mbx "+ mbxName);
            }
        }
        else
        {
            throw ( new TimsException("Can't delete mbx " +
                                          Integer.toHexString(mbx.name) +
                                          ". not initialised"));
        }
    }

    public synchronized void mbxClean(TimsMbx mbx)
    {
        synchronized(mbx)
        {
            mbx.clear();
            mbx.notifyAll();
        }
    }

    public int getDataRate()
    {
      int dataRate, deltaT;
      long newTime;

      synchronized(dataCountSync) {
        newTime = System.currentTimeMillis();
        deltaT = (int)(newTime - dataCountTime);
        dataRate = 1000 * dataCount / deltaT / 1024;
        dataCount = 0;
        dataCountTime = newTime;
      }
      return dataRate;
    }

    protected TimsMbx getMbx(int mbxName)
    {
      TimsMbx mbx;

      synchronized(mbxList) {
        for (int i = 0; i < mbxList.size(); i++) {
          mbx = (TimsMbx)(mbxList.elementAt(i));
          if (mbx.name == mbxName) {
            return mbx;
          }
        }
      }
      return null;
    }

}
