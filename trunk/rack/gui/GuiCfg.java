/*
 * RACK - Robotics Application Construction Kit
 * Copyright (C) 2005-2007 University of Hannover
 *                         Institute for Systems Engineering - RTS
 *                         Professor Bernardo Wagner
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Authors
 *      Oliver Wulf <wulf@rts.uni-hannover.de>
 *
 */
package rack.gui;

import java.awt.Dimension;
import java.awt.Point;
import java.beans.PropertyVetoException;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.util.Vector;

import javax.swing.JOptionPane;

public class GuiCfg
{
    private Vector<String> cfgLines = new Vector<String>();

    private Gui gui;

    GuiCfg(Gui gui)
    {
        this.gui = gui;
    }
    
    void readConfig(BufferedReader configReader) throws IOException
    {
        String string;
        try
        {
            GuiGroupDescriptor currentGroup = new GuiGroupDescriptor("");
            gui.groups.add(currentGroup);
            int readMode = 1;

            GuiWorkspaceDescriptor currentWorkspace = new GuiWorkspaceDescriptor("workspace");
            gui.workspaces.add(currentWorkspace);
            
            string = configReader.readLine();

            while (string != null)
            {
                string.trim();
                cfgLines.add(string);
                System.out.println(string);

                // Debug output
                // System.out.println(string);

                if (!string.startsWith("//") && string.length() != 0)
                {
                    if (string.startsWith("GROUP "))
                    {
                        if((currentGroup.name == "") && (currentGroup.elements.size() ==0))
                        {
                            gui.groups.remove(currentGroup);
                        }
                        string = string.substring(6).trim();
                        currentGroup = new GuiGroupDescriptor(string);
                        gui.groups.add(currentGroup);
                        readMode = 1;
                    }
                    else if (string.startsWith("TIMS_PARAM "))
                    {
                        string = string.substring(11).trim();  // cut TIMS_PARAM
                        gui.timsParam = string;
                    }
                    else if (string.startsWith("TIMS "))
                    {
                        string = string.substring(5).trim();  // cut TIMS
                        gui.timsClass = string;
                    }
                    else if (string.startsWith("JAR_FILES"))
                    {
                        readMode = 2;
                    }
                    else if (string.startsWith("RACK_NAME "))
                    {
                        string = string.substring(10);  // cut RACK_NAME
                        if (string.length() > 0)
                        {
                            gui.rackName = string.trim();
                        }
                    }
                    else if (string.startsWith("MAPVIEW"))
                    {
                        gui.showMapView = true;
                        gui.mapViewWorkSpace = gui.workspaces.indexOf(currentWorkspace);
                    }
                    else if (string.startsWith("WORKSPACE "))
                    {
                        if (gui.elements.size() == 0)
                        {
                            gui.workspaces.remove(0);
                        }
                        
                        string = string.substring(9).trim();

                        GuiWorkspaceDescriptor newWorkspace = new GuiWorkspaceDescriptor(string);
                        int index = gui.workspaces.indexOf(newWorkspace);
                        
                        if (index < 0)
                        {
                            gui.workspaces.add(newWorkspace);
                            currentWorkspace = newWorkspace;
                        }
                        else
                        {
                            currentWorkspace = gui.workspaces.get(index);
                        }
                    }
                    else // line starts without keyword
                    {
                        if (readMode == 1) // GuiElement
                        {
                            GuiElementDescriptor newElement = new GuiElementDescriptor();

                            newElement.cfg = string;
                            newElement.cfgSplit = string.split(" ");
                            
                            newElement.mainGui = gui;
                            newElement.group = currentGroup;
                            newElement.workspace = currentWorkspace; 

                            getGuiProxyClass(newElement);
                            getName(newElement);

                            getLocationSize(newElement.cfg, newElement.location, newElement.size);

                            gui.elements.add(newElement);

                            currentGroup.elements.add(newElement);
                            currentWorkspace.elements.add(newElement);
                        }
                        if (readMode == 2) // jarfile
                        {
                            gui.jarfiles.add(string);
                        }
                    }
                }
                if(string.startsWith("//mainFrameLocationSize"))
                {
                    // getMainFrameLocationSize
                    getLocationSize(string, gui.mainFrameLocation, gui.mainFrameSize);
                }
                // read next line
                string = configReader.readLine();
            }
            configReader.close();
        }
        catch (IOException ioe)
        {
            JOptionPane.showMessageDialog(gui.mainFrameContent,
                            "Error reading config file",
                            "RACK GUI", JOptionPane.ERROR_MESSAGE);
            throw ioe;
        }

        System.out.println("Found " + gui.elements.size() + " element(s)");
        System.out.println("Found " + gui.groups.size() + " group(s)");
        System.out.println("Found " + gui.workspaces.size() + " workspace(s)");
        System.out.println("Found " + gui.jarfiles.size() + " jarfile(s)");
    }

    // alle ModuleFrame Positionen in ConfigurationFile schreiben!
    void writeConfig(BufferedWriter cfgWriter) throws IOException
    {
        System.out.println("Write config");
        StringBuffer sb;
        for (int i = 0; i < gui.elements.size(); i++)
        {
            GuiElementDescriptor ge = gui.elements.get(i);
            sb = new StringBuffer(ge.cfg);
            int kAuf = sb.indexOf("(");
            int kZu = sb.indexOf(")");

            if (ge.frame != null)
            {
                if (ge.frame.isIcon())
                {
                    try
                    {
                        ge.frame.setIcon(false);
                    }
                    catch (PropertyVetoException e) {}
                }

                if (ge.frame.isMaximum())
                {
                    try
                    {
                        ge.frame.setMaximum(false);
                    }
                    catch (PropertyVetoException e) {}
                }

                ge.location = ge.frame.getLocation();
                ge.size = ge.frame.getSize();

                if((ge.size.width != 0) && (ge.size.height != 0))
                {
                    sb = sb.replace(kAuf, kZu, "(" +
                            ge.location.x + "," + ge.location.y + ";" +
                            ge.size.width + "," + ge.size.height);
                }
            }
            else
            {
                sb = sb.replace(kAuf, kZu, "(,;,");
            }

            for (int z = 0; z < cfgLines.size(); z++)
            {
                if (ge.cfg.equals(cfgLines.get(z)))
                {
                    cfgLines.set(z, sb.toString());
                    System.out.println("moduleName:" + ge.cfg);
                    System.out.println("configZeilen:" + cfgLines.get(z));
                }
            }
        }
        // um die mainFrameLocationSize abzuspeichen.
        if(gui.mainFrame != null)
        {
            gui.mainFrameLocation = gui.mainFrame.getLocation();
            gui.mainFrameSize = gui.mainFrame.getSize();
        }

        String str = "//mainFrameLocationSize(" +
                gui.mainFrameLocation.x + "," + gui.mainFrameLocation.y + ";" +
                gui.mainFrameSize.width + "," + gui.mainFrameSize.height + ")";

        if (cfgLines.get(0).startsWith(
                "//mainFrameLocationSize"))
        {
            cfgLines.set(0, str);
        }
        else
        {
            cfgLines.insertElementAt(str, 0);
        }

        for (int z = 0; z < cfgLines.size(); z++)
        {
            System.out.println(cfgLines.get(z));
            cfgWriter.write(cfgLines.get(z));
            cfgWriter.newLine();
        }

        cfgWriter.close();
    }

    private void getName(GuiElementDescriptor ge)
    {
        // erstmal sehen, ob der parameter "-name=..." in der gui.cfg benutzt
        // wurde
        ge.name = ge.getParameter("name");

        if (ge.name == "")
        {
            //System.out.println("cfgSplit " + ge.cfgSplit[0]);
            int i = ge.cfgSplit[0].lastIndexOf(".");
            ge.name = ge.cfgSplit[0].substring(i+1);

            if(ge.name.endsWith("Gui") == true)
            {
                ge.name = ge.name.replace("Gui", "");
            }
        }

        if(ge.instance >= 0)
        {
            ge.name = ge.name + "(" + ge.instance + ")";
        }
    }

    private void getGuiProxyClass(GuiElementDescriptor ge)
    {
        ge.guiClass = ge.cfgSplit[0];
        System.out.println("GuiClass:   " + ge.guiClass);

        ge.instance = Integer.parseInt(ge.cfgSplit[1]);

        ge.proxyClass = ge.getParameter("proxy");
        if (ge.proxyClass == "")
        {
            ge.proxyClass = ge.guiClass;

            if(ge.proxyClass.endsWith("Gui") == true)
            {
                ge.proxyClass = ge.proxyClass.replaceAll("Gui", "Proxy");
                ge.proxyClass = ge.proxyClass.replaceAll("gui.", "");
            }
        }
        System.out.println("ProxyClass: " + ge.proxyClass + " Instance: " + ge.instance);
    }

    private void getLocationSize(String cfg, Point location, Dimension size)
    {
        //System.out.println("getLocationSize " + cfg);

        int open    = cfg.indexOf('(');
        int close   = cfg.indexOf(')');

        String locationSize = cfg.substring(open + 1, close);

        locationSize = locationSize.replaceAll(";", ",");

        //System.out.println("locationSize " + locationSize);

        if(locationSize.startsWith(",,,"))
        {
            location.x = 0;
            location.y = 0;
            size.width = 0;
            size.height = 0;
        }
        else
        {
            String[] coordinates = locationSize.split(",");
    
            try
            {
                // for the location
                location.x = Integer.parseInt(coordinates[0].trim());
                location.y = Integer.parseInt(coordinates[1].trim());
    
                // for the size
                size.width = Integer.parseInt(coordinates[2].trim());
                size.height = Integer.parseInt(coordinates[3].trim());
            }
            catch (ArrayIndexOutOfBoundsException e)
            {
                JOptionPane.showMessageDialog(gui.mainFrameContent,
                        "Error reading location and size (1,2,3,4).\n" + 
                        "\"" + cfg + "\"",
                        "RACK GUI", JOptionPane.ERROR_MESSAGE);
            }
            catch (NumberFormatException e)
            {
                JOptionPane.showMessageDialog(gui.mainFrameContent,
                        "Error reading location and size (1,2,3,4).\n" + 
                        "\"" + cfg + "\"",
                        "RACK GUI", JOptionPane.ERROR_MESSAGE);
            }
        }
    }
}
