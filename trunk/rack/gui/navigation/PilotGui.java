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
package rack.gui.navigation;

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

import rack.gui.main.RackModuleGui;
import rack.main.*;
import rack.navigation.PilotDataMsg;
import rack.navigation.PilotProxy;

public class PilotGui extends RackModuleGui
{
    protected PilotProxy pilot;
    protected PilotComponent pilotComponent;

    protected JPanel panel;
    protected JPanel buttonPanel;

    protected JButton onButton;
    protected JButton offButton;
    protected JButton zoomOutButton;
    protected JButton zoomInButton;

    public int maxDistance = 1200; // 1m

    public PilotGui(PilotProxy proxy)
    {
        pilot = proxy;

        panel = new JPanel(new BorderLayout(2, 2));
        panel.setBorder(BorderFactory.createEmptyBorder(4, 4, 4, 4));

        JPanel northPanel = new JPanel(new BorderLayout(2, 2));

        buttonPanel = new JPanel(new GridLayout(0, 2, 4, 2));

        onButton = new JButton("On");
        offButton = new JButton("Off");
        zoomOutButton = new JButton("Zoom out");
        zoomInButton = new JButton("Zoom in");

        pilotComponent = new PilotComponent(maxDistance);
        pilotComponent.addKeyListener(new myKeyListener());

        onButton.addActionListener(new ActionListener()
        {
            public void actionPerformed(ActionEvent e)
            {
                pilot.on();
            }
        });
        onButton.addKeyListener(new myKeyListener());

        offButton.addActionListener(new ActionListener()
        {
            public void actionPerformed(ActionEvent e)
            {
                pilot.off();
            }
        });
        offButton.addKeyListener(new myKeyListener());

        zoomOutButton.addActionListener(new ActionListener()
        {
            public void actionPerformed(ActionEvent e)
            {
                pilotComponent.setMaxDistance(2.0);
            }
        });
        zoomOutButton.addKeyListener(new myKeyListener());

        zoomInButton.addActionListener(new ActionListener()
        {
            public void actionPerformed(ActionEvent e)
            {
                pilotComponent.setMaxDistance(0.5);
            }
        });
        zoomInButton.addKeyListener(new myKeyListener());

        buttonPanel.add(onButton);
        buttonPanel.add(offButton);
        buttonPanel.add(zoomInButton);
        buttonPanel.add(zoomOutButton);
        northPanel.add(new JLabel("pilot"), BorderLayout.NORTH);
        northPanel.add(buttonPanel, BorderLayout.CENTER);

        panel.add(northPanel, BorderLayout.NORTH);
        panel.add(pilotComponent, BorderLayout.CENTER);
    }

    public JComponent getComponent()
    {
        return (panel);
    }

    public String getModuleName()
    {
        return ("Pilot");
    }

    public RackProxy getProxy()
    {
        return (pilot);
    }

    public void run()
    {
        PilotDataMsg infoData;

        while (terminate == false)
        {
            if (panel.isShowing())
            {
                infoData = pilot.getData();

                if (infoData != null)
                {
                    pilotComponent.updateData(infoData);
                }
            }
            try
            {
                Thread.sleep(1000);
            }
            catch (InterruptedException e)
            {
            }
        }
    }

    class myKeyListener implements KeyListener
    {
        public void keyPressed(KeyEvent e)
        {
            if (e.getKeyCode() == KeyEvent.VK_RIGHT)
                pilotComponent.right();
            if (e.getKeyCode() == KeyEvent.VK_DOWN)
                pilotComponent.down();
            if (e.getKeyCode() == KeyEvent.VK_LEFT)
                pilotComponent.left();
            if (e.getKeyCode() == KeyEvent.VK_UP)
                pilotComponent.up();
            if (e.getKeyCode() == KeyEvent.VK_PLUS)
                pilotComponent.setMaxDistance(0.5);
            if (e.getKeyCode() == KeyEvent.VK_MINUS)
                pilotComponent.setMaxDistance(2.0);
            if (e.getKeyCode() == KeyEvent.VK_SUBTRACT)
                pilotComponent.setMaxDistance(0.5);
            if (e.getKeyCode() == KeyEvent.VK_ADD)
                pilotComponent.setMaxDistance(2.0);
        }

        public void keyReleased(KeyEvent e)
        {
        }

        public void keyTyped(KeyEvent e)
        {
        }
    }
}
