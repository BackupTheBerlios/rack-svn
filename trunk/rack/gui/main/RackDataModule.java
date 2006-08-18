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
package rack.gui.main;

import java.util.Vector;

import javax.swing.JComponent;
import javax.swing.JLabel;

import rack.main.naming.*;
import rack.main.debug.*;
import rack.main.proxy.*;
import rack.main.tims.*;

/**
 *
 * Entspricht dem ContinuousDataModule unter C Periodisches Verschicken der
 * Daten noch nicht richtig implementiert. Versenden an alle eingetragegenen
 * Zuhoerer bisher ueber writeWorkPackage()
 *
 */
public abstract class RackDataModule extends RackModuleGui
{

    /** Modul hat ein ON-Kommandopaket empfangen und soll sich einschalten */
    public abstract boolean moduleOn();

    /** Modul hat ein OFF-Kommandopaket empfangen und soll sich ausschalten */
    public abstract void moduleOff();

    /** Modul soll die Daten zu kontinuierliche Daten liefern */
    public abstract boolean moduleLoop();

    /** Modul soll nach beenden aufraeumen */
    public abstract void moduleCleanup();

    /** Modul empfaengt internes Paket */
    public abstract void moduleCommand(TimsDataMsg raw);

    /** Kommandomailbox */
    public int commandMbx;
    /** Arbeitsmailbox */
    public int workMbx;
    /** Datenmailbox */
    public int dataMbx;

    /** Pause im disabled-Zustand in [ms] */
    public int disabledTime = 100;
    /** Pause im error-Zustand in [ms] */
    public int errorTime = 1000;

    /** Definiert in welchen Status sich das Modul befindet */
    public byte moduleStatus = RackProxy.MSG_DISABLED;
    /** Wunschstatus */
    public int moduleTargetStatus = RackProxy.MSG_DISABLED;
    /** mailbox, in der On-Kommandos beantwortet werden */
    protected int notifyIfOn = 0;
    /** Merker der Packet-Id zur Notify-Antwort */
    protected byte notifyIfOnId;

    /** Liste der Zuhoerer von kontinuierlichen Daten */
    protected Vector dataListener = new Vector();
    protected TimsMsg dataMsg = null;

    /** Data module period time */
    public int periodTime = 100;

    protected int gdosLevel = GDOS.WARNING;

    /** beenden des Programmablaufs */
    public void terminate()
    {
        moduleCleanup();

        try
        {
            Tims.mbxDelete(commandMbx);
            if (dataMbx > 0)
                Tims.mbxDelete(dataMbx);
            if (workMbx > 0)
                Tims.mbxDelete(workMbx);
        }
        catch (TimsException e) {}
        
        super.terminate();
    }

    /**
     * Gibt den container des GUIs zurueck Die Methode muss, wenn es ein Gui
     * gibt, ueberschrieben werden
     */
    public JComponent getComponent()
    {
        return (new JLabel("No Gui implemented yet."));
    }

    /**
     * Initialisiert die Mailboxen des Moduls und startet die Task zum
     * Paketempfang
     */
    public RackDataModule(int commandMbx)
    {
        this.commandMbx = commandMbx;
        this.dataMbx = commandMbx + 1;
        this.workMbx = commandMbx + 2;
        try
        {
            Tims.mbxInit(commandMbx);
            Tims.mbxInit(dataMbx);
            Tims.mbxInit(workMbx);

            DataThread dt = new DataThread();
            dt.setDaemon(true); // Thread beendet sich wenn Programm sich
                                // beendet
            dt.start();
        }
        catch (TimsException e)
        {
            e.printStackTrace();
        }
        this.gdosLevel = GDOS.WARNING;
    }

    /**
     * Initialisiert die Mailboxen des Moduls und startet die Task zum
     * Paketempfang
     */
    public RackDataModule(int commandMbx, int gdosLevel)
    {
        this.commandMbx = commandMbx;
        this.dataMbx = commandMbx + 1;
        this.workMbx = commandMbx + 2;
        try
        {
            Tims.mbxInit(commandMbx);
            Tims.mbxInit(dataMbx);
            Tims.mbxInit(workMbx);

            DataThread dt = new DataThread();
            dt.setDaemon(true); // Thread beendet sich wenn Programm sich
                                // beendet
            dt.start();
        }
        catch (TimsException e)
        {
            e.printStackTrace();
        }
        this.gdosLevel = gdosLevel;
    }

    /** Sendet das uebergebene Paket an alle Listener */
    public void writeWorkMsg(TimsMsg msg)
    {
        msg.src = commandMbx;
        dataMsg = msg;

        synchronized(dataListener)
        {
            for (int i = 0; i < dataListener.size(); i++)
            {
                try
                {
                    // System.out.println("Sende Karte to
                    // Listener:"+((Integer)listener.elementAt(i)).intValue());
                    msg.dest = ((Integer) dataListener.elementAt(i)).intValue();
                    Tims.send(msg);
                }
                catch (TimsException e)
                {
                    e.printStackTrace();
                }
            }
        }
    }

    /** Paketempfangstask */
    public void run()
    {
        TimsDataMsg cmdMsg;

        GDOS.dbgInfo("Run command thread", commandMbx, gdosLevel);

        while (!terminate)
        {
            // Paket der Kommandomailbox empfangen
            try
            {
                cmdMsg = Tims.receive(commandMbx, 0);
            }
            catch (TimsException e1)
            {
                e1.printStackTrace();
                terminate();
                break;
            }

            // Empfangenes Paket analysieren
            switch (cmdMsg.type)
            {

                case RackProxy.MSG_ON:
                    switch (moduleTargetStatus)
                    {
                        case RackProxy.MSG_ENABLED:
                            if (moduleStatus == RackProxy.MSG_ENABLED)
                            {
                                try
                                {
                                    Tims.sendReply0(
                                            RackProxy.MSG_OK, cmdMsg);
                                }
                                catch (TimsException e)
                                {
                                    e.printStackTrace();
                                }
                            }
                            else
                            {
                                try
                                {
                                    Tims.sendReply0(
                                            RackProxy.MSG_ERROR, cmdMsg);
                                }
                                catch (TimsException e)
                                {
                                    e.printStackTrace();
                                }
                            }
                            break;

                        case RackProxy.MSG_DISABLED:
                            moduleTargetStatus = RackProxy.MSG_ENABLED;
                            notifyIfOn = cmdMsg.src;
                            notifyIfOnId = cmdMsg.seqNr;
                            break;

                        default:
                            try
                            {
                                Tims.sendReply0(RackProxy.MSG_ERROR,
                                        cmdMsg);
                            }
                            catch (TimsException e2)
                            {
                                e2.printStackTrace();
                            }
                    }
                    break;

                case RackProxy.MSG_OFF:
                    moduleTargetStatus = RackProxy.MSG_DISABLED;
                    try
                    {
                        Tims.sendReply0(RackProxy.MSG_OK, cmdMsg);
                    }
                    catch (TimsException e3)
                    {
                        e3.printStackTrace();
                    }
                    if (notifyIfOn > 0)
                    {
                        try
                        {
                            Tims.send0(RackProxy.MSG_ERROR,
                                    notifyIfOn, commandMbx, (byte) 0,
                                    (byte) notifyIfOnId);
                        }
                        catch (TimsException e)
                        {
                            e.printStackTrace();
                        }
                        notifyIfOn = 0;
                    }
                    break;

                case RackProxy.MSG_GET_STATUS:
                    try
                    {
                        Tims.sendReply0(moduleStatus, cmdMsg);
                    }
                    catch (TimsException e)
                    {
                        e.printStackTrace();
                    }
                    break;

                case RackProxy.MSG_GET_DATA:
                    try
                    {
                        if (dataMsg != null)
                        {
                            Tims.sendReply(RackProxy.MSG_DATA,
                                    cmdMsg, dataMsg);
                        }
                        else
                        {
                            System.out
                                    .println(RackName.nameString(commandMbx)
                                            + ": Im Java-Modul ist das Datenpacket nicht angelegt! (dataPackage == null)");
                            Tims.sendReply0(RackProxy.MSG_ERROR,
                                    cmdMsg);
                        }
                    }
                    catch (TimsException e)
                    {
                        e.printStackTrace();
                    }
                    break;

                case RackProxy.MSG_GET_CONT_DATA:
                    GetContDataMsg contPack;
                    try
                    {
                        contPack = new GetContDataMsg(cmdMsg);
                        
                        synchronized(dataListener)
                        {
                            int i;
                            boolean existent = false;
    
                            // check listener first
                            for (i = 0; i < dataListener.size(); i++)
                            {
                                int name = ((Integer) dataListener.elementAt(i)).intValue();
                                if (name == contPack.dataMbx)
                                {
                                    existent = true;
                                    break;
                                }
                            }
                            if (!existent)
                            {
                                dataListener.addElement(new Integer(contPack.dataMbx));
                            }

                            ContDataMsg contData = new ContDataMsg();
                            contData.periodTime = periodTime;

                            Tims.sendReply(RackProxy.MSG_CONT_DATA,
                                                    cmdMsg,
                                                    contData);
                        }
                    }
                    catch (TimsException e4)
                    {
                        e4.printStackTrace();
                    }
                    break;

                case RackProxy.MSG_STOP_CONT_DATA:
                    StopContDataMsg stopPack;
                    try
                    {
                        stopPack = new StopContDataMsg(cmdMsg);
                        synchronized(dataListener)
                        {
                            for (int i = 0; i < dataListener.size(); i++)
                            {
                                int name = ((Integer) dataListener.elementAt(i)).intValue();
                                if (name == stopPack.dataMbx)
                                {
                                    dataListener.removeElementAt(i);
                                }
                            }
                            Tims.sendReply0(RackProxy.MSG_OK, cmdMsg);
                        }
                    }
                    catch (TimsException e5)
                    {
                        e5.printStackTrace();
                    }
                    break;

                default: // Paket konnte nicht interpretiert werden...
                    moduleCommand(cmdMsg); // ... dies wird nun dem
                                            // entsprechenden Modul �berlassen
            }
        }
    }

    class DataThread extends Thread
    {

        public void run()
        {
            GDOS.dbgInfo("Run data thread", commandMbx, gdosLevel);

            while (!terminate)
            {
                switch (moduleStatus)
                {
                    case RackProxy.MSG_ENABLED:

                        if (moduleTargetStatus == RackProxy.MSG_ENABLED)
                        {
                            if (!moduleLoop())
                            {
                                GDOS.error("Error", commandMbx, gdosLevel);
                                moduleStatus = RackProxy.MSG_ERROR;
                                moduleOff();
                            }
                            if (notifyIfOn > 0)
                            {
                                try
                                {
                                    Tims.send0(RackProxy.MSG_OK,
                                            notifyIfOn, commandMbx, (byte) 0,
                                            notifyIfOnId);
                                }
                                catch (TimsException e)
                                {
                                    e.printStackTrace();
                                }
                                notifyIfOn = 0;
                            }
                        }
                        else
                        {
                            moduleStatus = RackProxy.MSG_DISABLED;
                            GDOS.dbgInfo("Turn off", commandMbx, gdosLevel);
                            moduleOff();
                            GDOS.print("Module off", commandMbx, gdosLevel);
                        }
                        break;

                    case RackProxy.MSG_DISABLED:
                        if (moduleTargetStatus == RackProxy.MSG_ENABLED)
                        {
                            synchronized(dataListener)
                            {
                                dataListener.removeAllElements();
                            }
                            GDOS.dbgInfo("Turn on", commandMbx, gdosLevel);
                            if (moduleOn() == true)
                            {
                                GDOS.print("Module on", commandMbx, gdosLevel);
                                moduleStatus = RackProxy.MSG_ENABLED;
                                if (notifyIfOn > 0)
                                {
                                    try
                                    {
                                        Tims.send0(RackProxy.MSG_OK,
                                                notifyIfOn, commandMbx,
                                                (byte) 0, notifyIfOnId);
                                    }
                                    catch (TimsException e)
                                    {
                                        e.printStackTrace();
                                    }
                                    notifyIfOn = 0;
                                }
                            }
                            else
                            {
                                GDOS.error("Error", commandMbx, gdosLevel);
                                moduleTargetStatus = RackProxy.MSG_DISABLED;
                                moduleStatus = RackProxy.MSG_DISABLED;
                                moduleOff();
                                if (notifyIfOn > 0)
                                {
                                    try
                                    {
                                        Tims.send0(
                                                RackProxy.MSG_ERROR,
                                                notifyIfOn, commandMbx,
                                                (byte) 0, notifyIfOnId);
                                    }
                                    catch (TimsException e)
                                    {
                                        e.printStackTrace();
                                    }
                                    notifyIfOn = 0;
                                }
                            }

                        }
                        else
                        {
                            try
                            {
                                Thread.sleep(disabledTime);
                            }
                            catch (InterruptedException e)
                            {
                            }

                        }
                        break;

                    case RackProxy.MSG_ERROR:
                        try
                        {
                            Thread.sleep(errorTime);
                        }
                        catch (InterruptedException e)
                        {
                        }

                        if (moduleTargetStatus == RackProxy.MSG_ENABLED)
                        {
                            synchronized(dataListener)
                            {
                                dataListener.removeAllElements();
                            }
                            GDOS.dbgInfo("Turn on", commandMbx, gdosLevel);
                            if (moduleOn())
                            {
                                GDOS.print("Module on", commandMbx, gdosLevel);
                                moduleStatus = RackProxy.MSG_ENABLED;

                                if (notifyIfOn > 0)
                                {
                                    try
                                    {
                                        Tims.send0(RackProxy.MSG_OK,
                                                notifyIfOn, commandMbx,
                                                (byte) 0, notifyIfOnId);
                                    }
                                    catch (TimsException e)
                                    {
                                        e.printStackTrace();
                                    }
                                    notifyIfOn = 0;
                                }
                            }
                            else
                            {
                                GDOS.dbgInfo("Turn off", commandMbx, gdosLevel);
                                moduleOff();

                                if (notifyIfOn > 0)
                                {
                                    try
                                    {
                                        Tims.send0(
                                                RackProxy.MSG_ERROR,
                                                notifyIfOn, commandMbx,
                                                (byte) 0, notifyIfOnId);
                                    }
                                    catch (TimsException e)
                                    {
                                        e.printStackTrace();
                                    }
                                    notifyIfOn = 0;
                                }
                            }
                        }
                        else
                        {
                            GDOS.print("Module off", commandMbx, gdosLevel);
                            moduleStatus = RackProxy.MSG_DISABLED;
                        }
                        break;

                    default:

                }
            }
        }
    }

}
