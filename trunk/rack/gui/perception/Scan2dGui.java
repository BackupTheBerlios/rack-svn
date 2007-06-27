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
 *      Joerg Langenberg <joerg.langenberg@gmx.net>
 *
 */
package rack.gui.perception;

import java.awt.*;
import java.awt.event.*;

import javax.swing.*;

import rack.gui.GuiElementDescriptor;
import rack.gui.main.*;
import rack.main.defines.*;
import rack.perception.Scan2dDataMsg;
import rack.perception.Scan2dProxy;

public class Scan2dGui extends RackModuleGui
{
    protected boolean         mapViewIsShowing = false;
    public int                maxDistance      = 10000; // 10m
    public Scan2dDataMsg      scan2dData;
    protected Scan2dProxy     scan2d;
    protected Scan2dComponent scan2dComponent;

    protected JButton         onButton;
    protected JButton         offButton;
    protected JButton         zoomOutButton;
    protected JButton         zoomInButton;
    protected JButton         storeContOnButton;
    protected JButton         storeContOffButton;
    public JLabel             contStoringLabel;
    protected int             contStoring      = 0;

    protected JPanel          panel;
    protected JPanel          northPanel;
    protected JPanel          wButtonPanel;
    protected JPanel          eButtonPanel;
    protected JPanel          sButtonPanel;

    protected Point           aktuellPoint;
    protected Point           mousePressedPoint;
    protected Point           mouseReleasedPoint;
    protected Point           mouseClickedPoint;

    public Scan2dGui(GuiElementDescriptor guiElement)
    {
        super(guiElement);

        scan2d = (Scan2dProxy) proxy;

        panel = new JPanel(new BorderLayout(2, 2));
        panel.setBorder(BorderFactory.createEmptyBorder(4, 4, 4, 4));

        northPanel = new JPanel(new BorderLayout(2, 2));
        wButtonPanel = new JPanel(new GridLayout(1, 0, 4, 2));
        eButtonPanel = new JPanel(new GridLayout(1, 0, 4, 2));
        sButtonPanel = new JPanel(new GridLayout(1, 0, 4, 2));

        onButton = new JButton("On");
        offButton = new JButton("Off");
        zoomOutButton = new JButton("Zoom out");
        zoomInButton = new JButton("Zoom in");
        storeContOnButton = new JButton("StoreOn");
        storeContOffButton = new JButton("StoreOff");
        contStoringLabel = new JLabel();
        contStoringLabel.setText("Cont.storing off.");

        scan2dComponent = new Scan2dComponent(maxDistance);

        scan2dComponent.addMouseListener(new MouseListener() {
            public void mouseClicked(MouseEvent e)
            {
                mouseClickedPoint = e.getPoint();
                scan2dComponent.setCenter(mouseClickedPoint);
            }

            public void mousePressed(MouseEvent e)
            {
                mousePressedPoint = e.getPoint();
            }

            public void mouseReleased(MouseEvent e)
            {
                mouseReleasedPoint = e.getPoint();
                // scan2dComponent.select(mousePressedPoint , mouseReleasedPoint);
            }

            public void mouseEntered(MouseEvent e)
            {
            }

            public void mouseExited(MouseEvent e)
            {
            }
        });

        onButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e)
            {
                scan2d.on();
            }
        });

        onButton.addKeyListener(new myKeyListener());

        offButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e)
            {
                scan2d.off();
            }
        });

        offButton.addKeyListener(new myKeyListener());

        zoomOutButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e)
            {
                maxDistance = maxDistance * 2;
                scan2dComponent.setMaxDistance(maxDistance);
            }
        });

        zoomOutButton.addKeyListener(new myKeyListener());

        zoomInButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e)
            {
                maxDistance = maxDistance / 2;
                scan2dComponent.setMaxDistance(maxDistance);
            }
        });

        zoomInButton.addKeyListener(new myKeyListener());

        storeContOnButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e)
            {
                contStoring = 1;
                contStoringLabel.setText("Cont.storing on.");

            }
        });

        storeContOffButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e)
            {
                contStoring = 0;
                contStoringLabel.setText("Cont.storing off.");

            }
        });

        wButtonPanel.add(onButton);
        wButtonPanel.add(offButton);
        eButtonPanel.add(zoomOutButton);
        eButtonPanel.add(zoomInButton);

        sButtonPanel.add(storeContOnButton, BorderLayout.WEST);
        sButtonPanel.add(storeContOffButton);
        sButtonPanel.add(contStoringLabel);

        northPanel.add(wButtonPanel, BorderLayout.WEST);
        northPanel.add(eButtonPanel, BorderLayout.EAST);
        northPanel.add(sButtonPanel, BorderLayout.SOUTH);

        panel.add(northPanel, BorderLayout.NORTH);
        panel.add(scan2dComponent, BorderLayout.CENTER);

    }

    public JComponent getComponent()
    {
        return panel;
    }

    public void run()
    {
        Scan2dDataMsg data;

        while (terminate == false)
        {
            if (panel.isShowing() | (mapViewIsShowing))
            {
                data = scan2d.getData();
                if (data != null)
                {
                    scan2dData = data;
                    scan2dComponent.updateData(data);
                    if (contStoring == 1)
                    {
                        scan2d.storeDataToFile("scan2d-" + System.currentTimeMillis() + ".txt");
                    }
                }
                mapViewIsShowing = false;
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

    public boolean hasMapView()
    {
        return true;
    }

    public void paintMapView(MapViewDrawContext drawContext)
    {
        mapViewIsShowing = true;

        if (scan2dData == null)
            return;

        Graphics2D robotGraphics = drawContext.getRobotGraphics(scan2dData.recordingTime);

        for (int i = 0; i < scan2dData.pointNum; i++)
        {
            ScanPoint point = scan2dData.point[i];
            int size = 100;
            int dist;

            if ((point.type & ScanPoint.TYPE_INVALID) != 0)
            {
                robotGraphics.setColor(Color.GRAY);
            }
            else if ((point.type & ScanPoint.TYPE_REFLECTOR) != 0)
            {
                robotGraphics.setColor(Color.YELLOW);
            }
            else if ((point.type & ScanPoint.TYPE_MASK) == ScanPoint.TYPE_LANDMARK)
            {
                robotGraphics.setColor(Color.BLUE);
            }
            else if ((point.type & ScanPoint.TYPE_MASK) == ScanPoint.TYPE_OBSTACLE)
            {
                robotGraphics.setColor(Color.RED);
            }
            else
            {
                robotGraphics.setColor(Color.BLACK);
            }

            // draw scanpoints in mm
            dist = (int) Math.sqrt(point.x * point.x + point.y * point.y);
            size += (int) (dist * 0.025);
            robotGraphics.fillArc(point.x - size / 2, point.y - size / 2, size, size, 0, 360);
        }
    }

    class myKeyListener extends KeyAdapter
    {
        public void keyPressed(KeyEvent e)
        {
            if (e.getKeyCode() == KeyEvent.VK_RIGHT)
                scan2dComponent.right();
            if (e.getKeyCode() == KeyEvent.VK_DOWN)
                scan2dComponent.down();
            if (e.getKeyCode() == KeyEvent.VK_LEFT)
                scan2dComponent.left();
            if (e.getKeyCode() == KeyEvent.VK_UP)
                scan2dComponent.up();
        }
    }
}
