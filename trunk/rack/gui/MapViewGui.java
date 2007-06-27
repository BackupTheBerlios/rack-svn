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
package rack.gui;

import java.awt.*;
import java.awt.image.*;
import java.awt.geom.AffineTransform;
import java.awt.event.*;
import java.io.File;
import java.util.*;

import javax.swing.*;
import javax.swing.event.*;
import javax.imageio.*;

import rack.gui.main.*;
import rack.main.defines.*;
import rack.main.tims.*;
import rack.drivers.*;
import rack.navigation.*;

public class MapViewGui extends Thread
{
    private Vector<GuiElementDescriptor> guiElement;
    private ModuleGuiList   moduleGuiList;

    private boolean         updateNeeded = false;
    private Position2d      viewPosition = new Position2d();
    private double          viewZoom     = 0.02;
    private int             viewGridDistance = 10000; // in mm
    private boolean         mouseEntered = false;

    private Position2d      robotPosition = new Position2d();
    private PositionDataMsg[] robotPositionList;
    private int             robotPositionListIndex = 0;
    private Position2d      worldCursorPosition = new Position2d(0, 0, 0);

    // basic Components
    private ViewPanel       viewPanel;
    private ActionCursor    actionCursor;
    private MapNavigator    mapNavigator;
    private JMenu           menuBar;
    private JPanel          panel;
    private BufferedImage   backGndImg;
    private double          backGndResX,        // in mm/pixel
                            backGndResY;        // in mm/pixel
    private Position2d      backGndOffset = new Position2d();

    // proxies
    private PositionProxy   positionProxy;
    private ChassisProxy    chassisProxy;

    // Messages
    private ChassisParamMsg chassisParam;

    private boolean         terminate = false;
    

    public MapViewGui(Vector<GuiElementDescriptor> element, TimsMbx workMbx)
    {
        this.guiElement = element;
        this.setPriority(Thread.MIN_PRIORITY);

        // create MapView proxies
        positionProxy = new PositionProxy(0, workMbx);
        chassisProxy  = new ChassisProxy(0, workMbx);

        // get chassis parameter message
        if (chassisProxy != null)
        {
            chassisParam = chassisProxy.getParam();
        }
        System.out.println("chassisParam="+chassisParam);
        if(chassisParam == null)
        {
            chassisParam = new ChassisParamMsg();
            chassisParam.boundaryFront = 400;
            chassisParam.boundaryBack  = 400;
            chassisParam.boundaryLeft  = 400;
            chassisParam.boundaryRight = 400;
        }

        // create MapView components
        menuBar = new JMenu();
        moduleGuiList = new ModuleGuiList();
        viewPanel = new ViewPanel();
        actionCursor = new ActionCursor();
        mapNavigator = new MapNavigator();

        // set MapView layout
        panel = new JPanel(new BorderLayout(2,2));
        panel.setBorder(BorderFactory.createEmptyBorder(2,2,2,2));
        panel.add(mapNavigator,BorderLayout.NORTH);
        panel.add(viewPanel,BorderLayout.CENTER);;

        // set MapView background
        File file = new File("mapViewBackground.jpg");        
/*        backGndOffset.x   = 106432;
        backGndOffset.y   = 204500;
//        backGndOffset.rho = (float)(-18.0 / 180.0 * Math.PI);
        backGndOffset.rho = 0.0f;
        backGndResX       = 220.0;
        backGndResY       = 230.0;*/
        backGndOffset.x   = 362800;
        backGndOffset.y   = 96500;
        backGndOffset.rho = (float)(-0.1/ 180.0 * Math.PI);
//        backGndOffset.rho = 0.0f;
        backGndResX       = 250.0;
        backGndResY       = 250.0;

        // read background image
        try
        {
            backGndImg = ImageIO.read(file);
        }
        catch (Exception exc)
        {
            System.out.println("Can't read background image\n" + exc);
        }

        robotPositionList = new PositionDataMsg[25];
        for(int i = 0; i < 25; i++)
        {
            robotPositionList[i] = new PositionDataMsg();
        }

        this.start();
    }

    
    public void terminate()
    {
        terminate = true;
        try
        {
            this.interrupt();
            this.join(100);
        }
        catch (Exception e) {}
    }

    
    public JComponent getComponent()
    {
        return panel;
    }


    private void changePositionAndZoom(int changeX, int changeY, int changeRho,
            int changeZoom)
    {
        Position2d newCenter = getPosOnScreen(viewPanel.getWidth() / 2
                + changeX * viewPanel.getWidth() / 5, viewPanel.getHeight() / 2
                + changeY * viewPanel.getHeight() / 5);
        viewPosition.rho += changeRho * Math.PI / 10;
        if (viewPosition.rho > Math.PI)
            viewPosition.rho -= (2 * Math.PI);
        if (viewPosition.rho < -Math.PI)
            viewPosition.rho += (2 * Math.PI);
        if (changeZoom > 0)
            viewZoom = viewZoom * 1.5;
        if (changeZoom < 0)
            viewZoom = viewZoom / 1.5;
        setCenter(newCenter);
    }


    private void setCenter(Position2d newCenter)
    {
        Position2d screenCenter;

        // sets center on screen in world coordinates
        screenCenter = (new Position2d((int)Math.round(-viewPanel.getHeight() /
                                                       (2 * viewZoom)),
                                       (int) Math.round(viewPanel.getWidth() /
                                                       (2 * viewZoom)))).
                                                  coordTrafo(viewPosition.rho);

        viewPosition.x = newCenter.x - screenCenter.x;
        viewPosition.y = newCenter.y - screenCenter.y;
    }

    private Position2d getPosOnScreen(int x, int y)
    {
        Position2d posOnScreen;

        // returns position in wold coordinates
        posOnScreen = new Position2d((int) Math.round(-y / viewZoom),
                                     (int) Math.round(x / viewZoom));
        return posOnScreen.coordTrafo(viewPosition.rho, viewPosition.x,
                                      viewPosition.y);
    }

    private void updateRobotPosition()
    {
        try
        {
        	PositionDataMsg position = new PositionDataMsg();
        	
        	if (positionProxy != null)
        	    position = positionProxy.getData();

            robotPosition = new Position2d(position.pos.x, position.pos.y,
                                           position.pos.rho);

            synchronized(robotPositionList)
            {
                robotPositionListIndex++;
                if(robotPositionListIndex >= 25)
                    robotPositionListIndex = 0;
                robotPositionList[robotPositionListIndex] = position;
            }
        }
        catch (Exception exc)
        {
            robotPosition = new Position2d();
        }

        if (mapNavigator.viewRobot())
        {
            viewPosition.rho = robotPosition.rho;
            setCenter(robotPosition);
        }

        // center mapView
        else
        {
        	Position2d upLeftBound     = getPosOnScreen(viewPanel.getWidth() / 4,
            										    viewPanel.getHeight() / 4);
            Position2d lowRightBound   = getPosOnScreen(viewPanel.getWidth() - 
            										    viewPanel.getWidth() / 4,
            										    viewPanel.getHeight() -
            										    viewPanel.getHeight() / 4);
            if ((robotPosition.x > upLeftBound.x) |
               (robotPosition.y < upLeftBound.y) |
               (robotPosition.x < lowRightBound.x) |
               (robotPosition.y > lowRightBound.y))
            {
            	setCenter(robotPosition);
            }
        }

    }

    public void updateNeeded()
    {
        updateNeeded = true;
        wakeup();
    }


    public void run()
    {
        Thread.yield();
        Thread.yield();
        Thread.yield();
        setCenter(new Position2d());

        while (terminate == false)
        {
            do
            {
                updateNeeded = false;
                if (!viewPanel.isShowing())
                    break;

                updateRobotPosition();

                // create a new draw context
                DrawContext drawContext;
                if (actionCursor.isSimRobotPosition())
                {
                    drawContext = new DrawContext(worldCursorPosition, null);
                }
                else
                {
                    drawContext = new DrawContext(robotPosition, robotPositionList);
                }

                actionCursor.drawDefaultCursor(drawContext.getRobotGraphics(), true);

                ListIterator<ModuleGuiProp> moduleGuiIterator = moduleGuiList.listIterator();
                while (moduleGuiIterator.hasNext())
                {
                    ModuleGuiProp moduleGuiProp = moduleGuiIterator.next();
                    if (!moduleGuiProp.getPaintIntoMap())
                        continue;
                    try
                    {
                        moduleGuiProp.getModuleGui().paintMapView(drawContext);
                    }
                    catch (Exception exc)
                    {
                        exc.printStackTrace();
                    }
                } // while
              viewPanel.setDrawContext(drawContext);
            }
            while ((updateNeeded == true) && (terminate == false));
            
            try {
                sleep(200);
            } catch (InterruptedException e) {}
        }
        System.out.println("MapViewGui terminated");
    } // run()

    private synchronized void wakeup()
    {
        this.notifyAll();
    }

    // *********************************************************
    // draw context
    // **********************************************************
    private class DrawContext extends BufferedImage implements
            MapViewDrawContext
    {

        private Graphics2D worldGraph;
        private Graphics2D robotGraph;
        private Position2d robotPosition;
        private PositionDataMsg robotPositionList[];

        public DrawContext(Position2d robotPosition, PositionDataMsg[] robotPositionList)
        {
            super(viewPanel.getWidth(), viewPanel.getHeight(),
                    BufferedImage.TYPE_INT_RGB);

            this.robotPosition = robotPosition;
            this.robotPositionList = robotPositionList;

            worldGraph = this.createGraphics();
            worldGraph.setClip(0, 0, this.getWidth(), this.getHeight());

            // prepare worldGraph
            worldGraph.scale(viewZoom, viewZoom);
            worldGraph.rotate(-viewPosition.rho - Math.PI / 2);
            worldGraph.translate(-viewPosition.x, -viewPosition.y);

            // prepare robotGraph
            robotGraph = this.createGraphics();
            robotGraph.setClip(0, 0, this.getWidth(), this.getHeight());
            robotGraph.setTransform(worldGraph.getTransform());
            robotGraph.translate(robotPosition.x, robotPosition.y);
            robotGraph.rotate(robotPosition.rho);

            drawBackgndImg(backGndImg, backGndResX, backGndResY, backGndOffset);
            drawGrid();
        }

        public Graphics2D getFrameGraphics()
        {
            return (Graphics2D)this.getGraphics();
        }

        public Graphics2D getWorldGraphics()
        {
            return worldGraph;
        }

        public Graphics2D getRobotGraphics()
        {
            return robotGraph;
        }

        public Graphics2D getRobotGraphics(int time)
        {
            Position2d robotPositionTime = getRobotPosition(time);

            // prepare robotGraph
            Graphics2D robotGraphTime = this.createGraphics();

            robotGraphTime.setClip(0, 0, this.getWidth(), this.getHeight());
            robotGraphTime.setTransform(worldGraph.getTransform());
            robotGraphTime.translate(robotPositionTime.x, robotPositionTime.y);
            robotGraphTime.rotate(robotPositionTime.rho);

            return robotGraphTime;
        }

        public Position2d getRobotPosition()
        {
            return (Position2d) robotPosition.clone();
        }

        public Position2d getRobotPosition(int time)
        {
            if(robotPositionList != null)
            {
                Position2d robotPosition;
                
                synchronized(robotPositionList)
                {
                    int index = robotPositionListIndex;
                    int indexTimeDiff = Math.abs(time - robotPositionList[index].recordingTime);
                    
                    for(int i = 0; i < robotPositionList.length; i++)
                    {
                        int timeDiff = Math.abs(time - robotPositionList[i].recordingTime);
                        if(timeDiff < indexTimeDiff)
                        {
                            index = i;
                            indexTimeDiff = timeDiff;
                        }
                    }
                    robotPosition = new Position2d(robotPositionList[index].pos.x,
                                                   robotPositionList[index].pos.y,
                                                   robotPositionList[index].pos.rho);
                }
                return robotPosition;
            }
            else
                return (Position2d) robotPosition.clone();
        }

        private void drawGrid()
        {
            Rectangle viewBounds = worldGraph.getClipBounds();
            worldGraph.setColor(Color.LIGHT_GRAY);

            for (int x = (viewGridDistance * (int) (viewBounds.x / viewGridDistance));
                 x < (viewBounds.x + viewBounds.width); x += viewGridDistance)
            {
                worldGraph.drawLine(x, viewBounds.y, x, viewBounds.y +
                                                        viewBounds.height);
            }

            for (int y = (viewGridDistance * (int) (viewBounds.y / viewGridDistance));
                 y < (viewBounds.y + viewBounds.height); y += viewGridDistance)
            {
                worldGraph.drawLine(viewBounds.x, y, viewBounds.x +
                                                     viewBounds.width, y);
            }
            worldGraph.setColor(Color.ORANGE);
            worldGraph.drawArc(-75, -75, 150, 150, 0, 270);
        }


        private void drawBackgndImg(BufferedImage image,
                                       double resX, double resY, Position2d pos)
        {
            Rectangle viewBounds = worldGraph.getClipBounds();
            worldGraph.setBackground(Color.WHITE);
            worldGraph.clearRect(viewBounds.x, viewBounds.y, viewBounds.width,
                                 viewBounds.height);
            if (image != null)
            {
                AffineTransform at = new AffineTransform();
                at.scale(resX, resY);
                at.rotate(pos.rho + Math.PI/2);
                BufferedImageOp biop = new AffineTransformOp(at, AffineTransformOp.TYPE_NEAREST_NEIGHBOR);
                worldGraph.drawImage(image, biop, pos.x, pos.y);
            }
        }
    }

    // **********************************************************
    // view panel
    // **********************************************************
    private class ViewPanel extends JPanel implements ComponentListener
    {
        private DrawContext drawContext = null;

        private static final long serialVersionUID = 1L;

        public ViewPanel()
        {
            this.setDoubleBuffered(false);
            this.addComponentListener(this);
        }

        public synchronized void setDrawContext(DrawContext newDrawContext)
        {
            drawContext = newDrawContext;
            this.repaint();
        }

        public synchronized void paint(Graphics onGraph)
        {
            if (drawContext == null)
            {
                return;
            }
            
            onGraph.drawImage(drawContext, 0, 0, this);
            
            if (mouseEntered)
            {
                actionCursor.drawCursor((Graphics2D) onGraph, drawContext);
            }
        }

        public void componentResized(ComponentEvent evnt)
        {
            updateNeeded();
            grabFocus();
        }

        public void componentHidden(ComponentEvent evnt)
        {
        }

        public void componentMoved(ComponentEvent evnt)
        {
        }

        public void componentShown(ComponentEvent evnt)
        {
            updateNeeded();
            grabFocus();
        }
    }

    // **********************************************************
    // Map Navigator
    // **********************************************************
    private class MapNavigator extends JPanel implements ActionListener,
            MouseWheelListener, MouseListener, MouseMotionListener, KeyListener
    {
        protected JPanel    eastPanel;
        protected JPanel    centerPanel;
        protected JPanel    westPanel;
        private JButton     viewRobotButton;
        private JButton     viewOriginButton;
        private JLabel      coordinateLabel;

        private static final long serialVersionUID = 1L;

        public MapNavigator()
        {
            this.setLayout(new BorderLayout(0,0));
//            this.setBorder(BorderFactory.createEmptyBorder(2,2,2,2));

         //   viewPanel.addMouseWheelListener(this);
            viewPanel.addMouseListener(this);
            viewPanel.addKeyListener(this);
            viewPanel.addMouseMotionListener(this);

           // command
            westPanel = new JPanel();
            westPanel.setLayout(new BorderLayout(2,2));
            westPanel.setBorder(BorderFactory.createEmptyBorder(2,2,2,2));

            JMenuBar menu = new JMenuBar();
            menu.add(new CommandMenu());
            westPanel.add(menu, BorderLayout.WEST);

            // coordinates
            centerPanel = new JPanel();
            centerPanel.setLayout(new BorderLayout(2, 2));
            centerPanel.setBorder(BorderFactory.createEmptyBorder(2,2,2,2));

            coordinateLabel = new JLabel("X: 0 mm , Y: 0 mm");
            centerPanel.add(coordinateLabel, BorderLayout.CENTER);

            // view
            eastPanel = new JPanel();
            eastPanel.setLayout(new BorderLayout(2, 2));
            eastPanel.setBorder(BorderFactory.createEmptyBorder(2,2,2,2));

            viewOriginButton = new JButton("Origin");
            viewOriginButton.setActionCommand("origin");
            viewOriginButton.addActionListener(this);
            viewOriginButton.setToolTipText("global view");
            viewRobotButton = new JButton("Robot");
            viewRobotButton.setActionCommand("robot");
            viewRobotButton.addActionListener(this);
            viewRobotButton.setToolTipText("robot centered view");

            eastPanel.add(viewOriginButton, BorderLayout.WEST);
            eastPanel.add(viewRobotButton, BorderLayout.EAST);

            add(westPanel, BorderLayout.WEST);
            add(centerPanel, BorderLayout.CENTER);
            add(eastPanel, BorderLayout.EAST);
        }

        public void actionPerformed(ActionEvent event)
        {
            if (event.getActionCommand().equals("north"))
            {
                changePositionAndZoom(0, -1, 0, 0);
                viewRobotButton.setSelected(false);
            }
            if (event.getActionCommand().equals("south"))
            {
                changePositionAndZoom(0, 1, 0, 0);
                viewRobotButton.setSelected(false);
            }
            if (event.getActionCommand().equals("west"))
            {
                changePositionAndZoom(-1, 0, 0, 0);
                viewRobotButton.setSelected(false);
            }
            if (event.getActionCommand().equals("east"))
            {
                changePositionAndZoom(1, 0, 0, 0);
                viewRobotButton.setSelected(false);
            }
            if (event.getActionCommand().equals("in"))
            {
                changePositionAndZoom(0, 0, 0, 1);
            }
            if (event.getActionCommand().equals("out"))
            {
                changePositionAndZoom(0, 0, 0, -1);
            }
            if (event.getActionCommand().equals("left"))
            {
                changePositionAndZoom(0, 0, -1, 0);
                viewRobotButton.setSelected(false);
            }
            if (event.getActionCommand().equals("right"))
            {
                changePositionAndZoom(0, 0, 1, 0);
                viewRobotButton.setSelected(false);
            }
            if (event.getActionCommand().equals("origin"))
            {
                viewPosition.rho = 0;
                setCenter(robotPosition);
                viewRobotButton.setSelected(false);
            }
            if (event.getActionCommand().equals("robot"))
            {
                viewRobotButton.setSelected(true);
            }

            updateNeeded();
            viewPanel.grabFocus();
        }

        public void mouseMoved(MouseEvent event)
        {
            Position2d tempPosition = getPosOnScreen(event.getX(),
                                                     event.getY());
            worldCursorPosition.x   = tempPosition.x;
            worldCursorPosition.y   = tempPosition.y;
            coordinateLabel.setText("X: "+worldCursorPosition.x+" mm, " +
                                    "Y: "+worldCursorPosition.y+" mm");
        }

        public void mouseDragged(MouseEvent event)
        {
        }

        public void mouseClicked(MouseEvent event)
        {
            if (event.getButton() == MouseEvent.BUTTON2)
            {
                setCenter(getPosOnScreen(event.getX(), event.getY()));
                viewRobotButton.setSelected(false);
                updateNeeded();
            }
        }

        public void mousePressed(MouseEvent event)
        {
            viewPanel.grabFocus();
        }

        public void mouseReleased(MouseEvent event)
        {
        }

        public void mouseEntered(MouseEvent e)
        {
            mouseEntered = true;
        }

        public void mouseExited(MouseEvent e)
        {
            mouseEntered = false;
        }

        public void mouseWheelMoved(MouseWheelEvent event)
        {
            changePositionAndZoom(0, 0, 0, -event.getWheelRotation());
            viewPanel.grabFocus();
        }

        public void keyPressed(KeyEvent event)
        {
            switch (event.getKeyCode())
            {
                case KeyEvent.VK_RIGHT:
                    actionPerformed(new ActionEvent(this, 0, "east"));
                    break;
                case KeyEvent.VK_LEFT:
                    actionPerformed(new ActionEvent(this, 0, "west"));
                    break;
                case KeyEvent.VK_UP:
                    actionPerformed(new ActionEvent(this, 0, "north"));
                    break;
                case KeyEvent.VK_DOWN:
                    actionPerformed(new ActionEvent(this, 0, "south"));
                    break;

                case KeyEvent.VK_PLUS:
                    actionPerformed(new ActionEvent(this, 0, "in"));
                    break;
                case KeyEvent.VK_MINUS:
                    actionPerformed(new ActionEvent(this, 0, "out"));
                    break;

                case KeyEvent.VK_PAGE_UP:
                    actionPerformed(new ActionEvent(this, 0, "left"));
                    break;
                case KeyEvent.VK_PAGE_DOWN:
                    actionPerformed(new ActionEvent(this, 0, "right"));
                    break;
            }
        }

        public void keyReleased(KeyEvent e)
        {
        }

        public void keyTyped(KeyEvent e)
        {
        }

        public boolean viewRobot()
        {
            return viewRobotButton.isSelected();
        }
        

        private class CommandMenu extends JMenu implements MenuListener
        {
            private static final long serialVersionUID = 1L;

            public CommandMenu()
            {
                super("   Command   ");
                addMenuListener(this);
                setToolTipText("Choose a command");
            }

            public void menuCanceled(MenuEvent arg0)
            {
            }

            public void menuDeselected(MenuEvent arg0)
            {
            }

            public void menuSelected(MenuEvent arg0)
            {
                this.removeAll();

                ListIterator<ModuleGuiProp> moduleGuiIterator = moduleGuiList.listIterator();
                while (moduleGuiIterator.hasNext())
                {
                    ModuleGuiProp moduleGuiProp = moduleGuiIterator.next();
                    if (!moduleGuiProp.isOn())
                        continue;

                    MapViewActionList actionList = moduleGuiProp.getModuleGui()
                            .getMapViewActionList();
                    if (actionList == null)
                        continue;

                    JMenu newSubmenu = new JMenu(actionList.title);
                    this.add(newSubmenu);

                    ListIterator<MapViewActionListItem> actionListIterator = actionList.listIterator();
                    while (actionListIterator.hasNext())
                    {
                        newSubmenu.add(new ModuleActionEvent(moduleGuiProp,
                                      actionListIterator.next()));
                    }
                }

                if (this.getComponentCount() > 0)
                    this.addSeparator();

                JMenuItem newMenuItem = new JMenuItem("Cursor");
                newMenuItem.setActionCommand("selectCursor");
                newMenuItem.addActionListener(actionCursor);
                this.add(newMenuItem);

                this.validate();
            }
        } // commandMenu
    }







    // **********************************************************
    // Action Cursor
    // **********************************************************
    private class ActionCursor extends JPanel implements MouseListener,
            MouseMotionListener, MouseWheelListener, ActionListener
    {
        public boolean active = false;
        private ModuleActionEvent actionEvent = null;
        private float dRhoPerClick = (float)Math.toRadians(10.0);

        private static final long serialVersionUID = 1L;

        public ActionCursor()
        {
            viewPanel.addMouseListener(this);
            viewPanel.addMouseMotionListener(this);
            viewPanel.addMouseWheelListener(this);
        }

        public boolean isSimRobotPosition()
        {
            return false;
//            return simRobotPositionButton.isSelected();
        }

        public void actionPerformed(ActionEvent event)
        {
            if (event.getActionCommand().equals("selectAction"))
            {
                actionEvent = (ModuleActionEvent) event.getSource();
                active = true;
            }
            if (event.getActionCommand().equals("repeat"))
            {
                if (actionEvent != null && !actionEvent.moduleGuiProp.isOn())
                    actionEvent = null;
                active = true;
            }
            if (event.getActionCommand().equals("cancel"))
            {
                actionEvent = null;
                active = false;
            }
            if (event.getActionCommand().equals("execute"))
            {
                if (actionEvent != null && actionEvent.moduleGuiProp.isOn())
                {
                    updateRobotPosition();
                    actionEvent.updatePositions(robotPosition,
                            worldCursorPosition);
                    actionEvent.moduleGuiProp.getModuleGui()
                            .mapViewActionPerformed(actionEvent);
                }
                active = true;
            }
        }

        public synchronized void drawCursor(Graphics2D screenGraph,
                DrawContext drawContext)
        {

            if (active)
            {
                CursorDrawContext cursorDrawContext = new CursorDrawContext(
                        screenGraph, drawContext, actionEvent,
                        worldCursorPosition);
                if (actionEvent != null
                        && actionEvent.moduleGuiProp.isOn()
                        && actionEvent.moduleGuiProp.getModuleGui()
                                .hasMapViewCursor())
                {
                    actionEvent.moduleGuiProp.getModuleGui()
                            .paintMapViewCursor(cursorDrawContext);
                }
                else
                {
                    drawDefaultCursor(cursorDrawContext.getCursorGraphics(), false);
                }
            }
        }

        private void drawDefaultCursor(Graphics2D cursorGraphics, boolean filled)
        {
            int chassisWidth = chassisParam.boundaryLeft +
               chassisParam.boundaryRight;
            int chassisLength = chassisParam.boundaryBack +
                chassisParam.boundaryFront;

            if(filled)
            {
                cursorGraphics.setColor(Color.LIGHT_GRAY);
                cursorGraphics.fillRect(-chassisParam.boundaryBack -
                                         chassisParam.safetyMargin,
                                        -chassisParam.boundaryLeft -
                                         chassisParam.safetyMargin,
                                         chassisLength + 2 * chassisParam.safetyMargin +
                                         chassisParam.safetyMarginMove,
                                         chassisWidth + 2 * chassisParam.safetyMargin);
    
                cursorGraphics.setColor(Color.GRAY);
                cursorGraphics.fillRect(-chassisParam.boundaryBack,
                                        -chassisParam.boundaryLeft,
                                         chassisLength, chassisWidth);
            }

            cursorGraphics.setColor(Color.BLACK);
            cursorGraphics.drawRect(-chassisParam.boundaryBack,
                                    -chassisParam.boundaryLeft,
                                     chassisLength, chassisWidth);
            cursorGraphics.drawLine(-chassisParam.boundaryBack +
                                     (int)(chassisLength * 0.5),
                                       -chassisParam.boundaryLeft,
                                        chassisParam.boundaryFront,
                                       -chassisParam.boundaryLeft +
                                        (int)(chassisWidth * 0.5));
            cursorGraphics.drawLine( chassisParam.boundaryFront,
                                    -chassisParam.boundaryLeft +
                                     (int)(chassisWidth * 0.5),
                                    -chassisParam.boundaryBack +
                                     (int)(chassisLength * 0.5),
                                     chassisParam.boundaryRight);
        }

        public Position2d translateRobotCursorPosition(
                Position2d worldCursorPosition, Position2d robotPosition)
        {
            Position2d robotCursorPosition = new Position2d();
            double sinRho = Math.sin(robotPosition.rho);
            double cosRho = Math.cos(robotPosition.rho);
            double x      = (double)(worldCursorPosition.x - 
                                      robotPosition.x);
            double y      = (double)(worldCursorPosition.y - 
                                      robotPosition.y);            

            robotCursorPosition.x   = (int)(  x * cosRho + y * sinRho);
            robotCursorPosition.y   = (int)(- x * sinRho + y * cosRho);
            robotCursorPosition.rho = normalizeAngle(worldCursorPosition.rho - 
                                      robotPosition.rho);
            return robotCursorPosition;
        }

        private float normalizeAngle(float angle)
        {
            if (angle > Math.PI)
                angle -= 2 * Math.PI;
            if (angle < -Math.PI)
                angle += 2 * Math.PI;
            return angle;
        }


        public void mouseDragged(MouseEvent event)
        {
            if ((event.getModifiersEx() & MouseEvent.BUTTON1_DOWN_MASK) > 0)
            {
                Position2d tempPosition = getPosOnScreen(event.getX(), event
                        .getY());
                worldCursorPosition.x = tempPosition.x;
                worldCursorPosition.y = tempPosition.y;
                actionPerformed(new ActionEvent(this, 0, "execute"));
            }
/*            if ((event.getModifiersEx() & MouseEvent.BUTTON3_DOWN_MASK) > 0)
            {
                Position2D mousePsition = getPosOnScreen(event.getX(), event
                        .getY());
                worldCursorPosition.rho = normalizeAngle((float) Math.atan2(
                            mousePsition.y - worldCursorPosition.y,
                            mousePsition.x - worldCursorPosition.x));
            }*/

            if (isSimRobotPosition())
                updateNeeded();
            else
                viewPanel.repaint();
        }

        public void mouseWheelMoved(MouseWheelEvent event)
        {
            worldCursorPosition.rho = normalizeAngle(
                                      worldCursorPosition.rho +
                                      event.getWheelRotation() * dRhoPerClick);
            updateNeeded();
        }

        public void mouseClicked(MouseEvent event)
        {
        }

        public void mouseEntered(MouseEvent arg0)
        {
        }

        public void mouseExited(MouseEvent arg0)
        {
        }

        public void mousePressed(MouseEvent event)
        {
            mouseDragged(event);
        }

        public void mouseReleased(MouseEvent arg0)
        {
        }

        public void mouseMoved(MouseEvent arg0)
        {
        }
    }

    // **********************************************************
    // ModuleActionEvent
    // **********************************************************
    private class ModuleActionEvent extends JMenuItem implements
            MapViewActionEvent
    {

        public ModuleGuiProp moduleGuiProp = null;
        public String actionCommand = "";

        public Position2d robotPosition;
        public Position2d worldCursorPosition;
        public Position2d robotCursorPosition;

        private static final long serialVersionUID = 1L;

        public ModuleActionEvent(ModuleGuiProp n_moduleGuiProp,
                MapViewActionListItem listItem)
        {
            super(listItem.title);
            setActionCommand("selectAction");
            addActionListener(actionCursor);

            moduleGuiProp = n_moduleGuiProp;
            actionCommand = listItem.actionCommand;
        }

        public void updatePositions(Position2d robotPosition,
                Position2d worldCursorPosition)
        {
            this.robotPosition = robotPosition;
            this.worldCursorPosition = worldCursorPosition;
            robotCursorPosition = actionCursor.translateRobotCursorPosition(
                    worldCursorPosition, robotPosition);
        }

        public String getActionCommand()
        {
            return actionCommand;
        }

        public Position2d getWorldCursorPos()
        {
            return (Position2d) worldCursorPosition;
        }

        public Position2d getRobotCursorPos()
        {
            return (Position2d) robotCursorPosition;
        }

        public Position2d getRobotPosition()
        {
            return (Position2d) robotPosition;
        }
    }





    // **********************************************************
    // cursor draw context
    // **********************************************************
    private class CursorDrawContext implements MapViewCursorDrawContext
    {

        private Graphics2D worldGraph;
        private Graphics2D robotGraph;
        private Graphics2D cursorGraph;

        private Position2d robotPosition;
        private Position2d worldCursorPosition;
        private Position2d robotCursorPosition;

        private ModuleActionEvent actionEvent;

        public CursorDrawContext(Graphics2D screenGraph,
                DrawContext drawContext, ModuleActionEvent actionEvent,
                Position2d worldCursorPosition)
        {

            // prepare worldGraph
            worldGraph = (Graphics2D) screenGraph.create();
            worldGraph.setClip(0, 0, drawContext.getWidth(), drawContext
                    .getHeight());
            worldGraph.setTransform(drawContext.getWorldGraphics()
                    .getTransform());

            // prepare robotGraph
            robotGraph = (Graphics2D) screenGraph.create();
            robotGraph.setClip(0, 0, drawContext.getWidth(), drawContext
                    .getHeight());
            robotGraph.setTransform(drawContext.getRobotGraphics()
                    .getTransform());

            // prepare cursorGraph
            cursorGraph = (Graphics2D) screenGraph.create();
            cursorGraph.setClip(0, 0, drawContext.getWidth(), drawContext
                    .getHeight());
            cursorGraph.setTransform(worldGraph.getTransform());
            cursorGraph.translate(worldCursorPosition.x,
                                  worldCursorPosition.y);
            cursorGraph.rotate(worldCursorPosition.rho);

            robotPosition = drawContext.getRobotPosition();
            this.worldCursorPosition = worldCursorPosition;
            this.actionEvent = actionEvent;

            robotCursorPosition = actionCursor.translateRobotCursorPosition(
                    worldCursorPosition, robotPosition);
        }

        public Graphics2D getFrameGraphics()
        {
            return null;
        }

        public Graphics2D getWorldGraphics()
        {
            return worldGraph;
        }

        public Graphics2D getRobotGraphics()
        {
            return robotGraph;
        }

        public Graphics2D getRobotGraphics(int time)
        {
            return robotGraph;
        }

        public Graphics2D getCursorGraphics()
        {
            return cursorGraph;
        }

        public Position2d getRobotPosition()
        {
            return robotPosition;
        }

        public Position2d getRobotPosition(int time)
        {
            return robotPosition;
        }

        public Position2d getRobotCursorPos()
        {
            return robotCursorPosition;
        }

        public Position2d getWorldCursorPos()
        {
            return worldCursorPosition;
        }

        public String getActionCommand()
        {
            if (actionEvent != null)
                return actionEvent.getActionCommand();
            else
                return "";
        }
    }

    // **********************************************************
    // ModuleGuiProp
    // **********************************************************
    private class ModuleGuiProp extends JMenu implements ActionListener
    {

        private JMenuItem paintItem;
        private int moduleGuiIndex;

        private static final long serialVersionUID = 1L;

        public ModuleGuiProp(int n_moduleGuiIndex)
        {
            super();

            moduleGuiIndex = n_moduleGuiIndex;

            paintItem = new JCheckBoxMenuItem("Paint", true);
            paintItem.setActionCommand("paint");
            paintItem.addActionListener(this);
            add(paintItem);
        }

        public GuiElement getModuleGui()
        {
            return guiElement.get(moduleGuiIndex).gui;
        }

        public boolean isOn()
        {
            return (getModuleGui() != null) && getModuleGui().hasMapView();
        }

        public boolean getPaintIntoMap()
        {
            return (getModuleGui() != null) && paintItem.isSelected();
        }

        public String toString()
        {
            if (isOn())
                return guiElement.get(moduleGuiIndex).name;
            else
                return "";
        }

        public void actionPerformed(ActionEvent event)
        {
            updateNeeded();

        }

    } // ModuleGuiProp


    // **********************************************************
    // ModuleGuiList
    // **********************************************************
    private class ModuleGuiList extends ArrayList<ModuleGuiProp>
    {

        private GuiListMenu guiListMenu;

        private static final long serialVersionUID = 1L;

        public ModuleGuiList()
        {
            super(guiElement.size());

            for (int g = 0; g < guiElement.size(); g++)
            {
                this.add(new ModuleGuiProp(g));
            }

            guiListMenu = new GuiListMenu();

            menuBar.add(guiListMenu);
        }

        private class GuiListMenu extends JMenu implements MenuListener
        {
            private static final long serialVersionUID = 1L;

            public GuiListMenu()
            {
                super("Options");
                addMenuListener(this);
                setToolTipText("Change MapView options");
            }

            public void menuCanceled(MenuEvent arg0)
            {
            }

            public void menuDeselected(MenuEvent arg0)
            {
            }

            public void menuSelected(MenuEvent arg0)
            {
                this.removeAll();

                ListIterator<ModuleGuiProp> moduleGuiIterator = moduleGuiList.listIterator();
                while (moduleGuiIterator.hasNext())
                {
                    ModuleGuiProp moduleGuiProp = moduleGuiIterator.next();
                    if (!moduleGuiProp.isOn())
                        continue;

                    moduleGuiProp.setText(moduleGuiProp.toString());
                    add(moduleGuiProp);
                }
                this.validate();
            }
        }
    } // ModuleGuiList

}