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
 *      Oliver Wulf <oliver.wulf@web.de>
 *
 */
package rack.gui.main;

import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.GridLayout;
import java.awt.Point;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyAdapter;
import java.awt.event.KeyEvent;
import java.awt.event.KeyListener;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.awt.event.MouseMotionListener;
import java.awt.event.MouseWheelEvent;
import java.awt.event.MouseWheelListener;
import java.awt.geom.AffineTransform;
import java.awt.geom.NoninvertibleTransformException;
import java.awt.image.AffineTransformOp;
import java.awt.image.BufferedImage;
import java.awt.image.BufferedImageOp;
import java.util.Vector;

import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JPanel;

import rack.main.AngleTool;
import rack.main.defines.Position2d;
import rack.navigation.PositionDataMsg;

public class MapViewComponent extends JComponent
{
    private static final int           MAX_BACKGROUND_IMAGES = 4;
    private static final long          serialVersionUID      = 1L;

    public static double               DEFAULT_ZOOM_FACTOR   = 1.141;
    public static double               DEFAULT_VISIBLE_RANGE = 5000.0;

    protected double                   defaultZoomFactor     = DEFAULT_ZOOM_FACTOR;
    protected double                   defaultVisibleRange   = DEFAULT_VISIBLE_RANGE;

    protected Vector<MapViewInterface> mapViews              = new Vector<MapViewInterface>();

    protected Vector<PositionDataMsg>  robotPosition;
    protected double                   visibleRange;
    protected double                   centerX;
    protected double                   centerY;
    protected Position2d               worldCenter           = new Position2d();
    protected AffineTransform          world2frame           = new AffineTransform();
    
    protected boolean                  showGrid;
    protected boolean                  showCursor;
    protected int                      actionMenuIndex;
    
    protected BufferedImage            bgImg[] = new BufferedImage[MAX_BACKGROUND_IMAGES];
    protected double                   bgX[] = new double[MAX_BACKGROUND_IMAGES];
    protected double                   bgY[] = new double[MAX_BACKGROUND_IMAGES];
    protected double                   bgW[] = new double[MAX_BACKGROUND_IMAGES];
    protected double                   bgH[] = new double[MAX_BACKGROUND_IMAGES];
    protected boolean                  bgBicubic[] = new boolean[MAX_BACKGROUND_IMAGES];
    protected boolean                  bgAssigned[] = new boolean[MAX_BACKGROUND_IMAGES];

    public JPanel                      zoomPanel;
    public JButton                     zoomInButton;
    public JButton                     zoomOutButton;
    public JButton                     zoomCenterButton;
    
    protected ActionListener           zoomInAction;
    protected ActionListener           zoomOutAction;
    protected ActionListener           zoomCenterAction;

    protected Point                    mousePressed;
    protected float                    cursorRho             = 0.0f;

    public MouseListener               mouseListener         = new MapViewComponentMouseListener();
    public KeyListener                 keyListener           = new MapViewComponentKeyListener();
    
    public double                      zoomRange;            //a public copy of visibleRange
                               
   
    public MapViewComponent()
    {
        this.setDoubleBuffered(true);
        this.setBackground(Color.WHITE);
        this.setPreferredSize(new Dimension(400, 400));
        this.visibleRange = DEFAULT_VISIBLE_RANGE;
        this.zoomRange = DEFAULT_VISIBLE_RANGE;
        this.showGrid = true;
        this.showCursor = false;
        this.bgAssigned[0] = false;
        
        this.robotPosition = new Vector<PositionDataMsg>();
        robotPosition.add(new PositionDataMsg());
        
        this.addKeyListener(keyListener);
        this.addMouseListener(mouseListener);
        this.addMouseMotionListener((MouseMotionListener)mouseListener);
        this.addMouseWheelListener((MouseWheelListener)mouseListener);
        
        zoomInButton = new JButton("zoom in");
        zoomInButton.addKeyListener(keyListener);
        zoomInAction = new ActionListener() {
            public void actionPerformed(ActionEvent e)
            {
                zoomIn(defaultZoomFactor);
            }
        };
        zoomInButton.addActionListener(zoomInAction);

        zoomOutButton = new JButton("zoom out");
        zoomOutButton.addKeyListener(keyListener);
        zoomOutAction = new ActionListener() {
            public void actionPerformed(ActionEvent e)
            {
                zoomOut(defaultZoomFactor);
            }
        };
        zoomOutButton.addActionListener(zoomOutAction);
        
        zoomCenterButton = new JButton("center");
        zoomCenterButton.addKeyListener(keyListener);
        zoomCenterAction = new ActionListener() {
            public void actionPerformed(ActionEvent e)
            {
                
                zoomCenter();
            }
        };
        zoomCenterButton.addActionListener(zoomCenterAction);

        zoomPanel = new JPanel(new GridLayout(1, 0, 4, 2));
        zoomPanel.add(zoomInButton);
        zoomPanel.add(zoomOutButton);
        zoomPanel.add(zoomCenterButton);
    }

    public void setEnabled(boolean enabled)
    {
        zoomInButton.setEnabled(enabled);
        zoomOutButton.setEnabled(enabled);
        zoomCenterButton.setEnabled(enabled);
    }
    
    public void addMapView(MapViewInterface mapView)
    {
        mapViews.add(mapView);
    }
    
    public void removeMapView(MapViewInterface mapView)
    {
        mapViews.remove(mapView);
    }

    public synchronized void setBackgroundImage(int index,BufferedImage bgImg, int bgX, int bgY, int bgW, int bgH, boolean bgBicubic)
    {
        this.bgImg[index] = bgImg;
        this.bgX[index] = (double)bgX;
        this.bgY[index] = (double)bgY;
        this.bgW[index] = (double)bgW;
        this.bgH[index] = (double)bgH;
        this.bgBicubic[index] = bgBicubic;
        this.bgAssigned[index] = true;
    }
    
    public synchronized void setBackgroundImage(BufferedImage bgImg, int bgX, int bgY, int bgW, int bgH, boolean bgBicubic)
    {
        setBackgroundImage(0, bgImg, bgX, bgY, bgW, bgH, bgBicubic);
    }
 
    public void setWorldCenter(Position2d worldCenter)
    {
        synchronized(world2frame)
        {
            this.worldCenter   = worldCenter;
        }
    }

    public void setDefaultVisibleRange(double visibleRange)
    {
        this.defaultVisibleRange = visibleRange;
        this.visibleRange = visibleRange;
        this.zoomRange = visibleRange;
    }
    
    public Vector<PositionDataMsg> getRobotPositionVector()
    {
        return robotPosition;
    }

    public Position2d getCursorPosition()
    {
        Position2d cursorPosition = new Position2d();
        
        cursorPosition = frame2world(getMousePosition());

        cursorRho = AngleTool.normalise(cursorRho);
        cursorPosition.rho = AngleTool.normalise(cursorRho + worldCenter.rho);
        return cursorPosition;
    }
    
    public Position2d getViewPortCenter()
    {
        Position2d viewPortCenter = new Position2d();
        Point viewPortPoint = new Point(getWidth()/2, getHeight()/2);
        viewPortCenter      = frame2world(viewPortPoint);
        return viewPortCenter;
    }
    
    protected Position2d frame2world(Point framePoint)
    {
        Point worldPoint = new Point();

        synchronized(world2frame)
        {
            try
            {
                world2frame.inverseTransform(framePoint, worldPoint);
            }
            catch (NoninvertibleTransformException e)
            {
                world2frame.setToIdentity();
            }
        }
        return new Position2d(worldPoint.x, worldPoint.y, 0.0f);
    }
    
    protected Point world2frame(Position2d worldPosition)
    {
        Point worldPoint = new Point(worldPosition.x, worldPosition.y);
        Point framePoint = new Point();
        
        synchronized(world2frame)
        {
            world2frame.transform(worldPoint, framePoint);
        }
        
        return framePoint;
    }
    
   protected void paintCrosshairCursor(Graphics2D cg)
    {
        cg.setColor(Color.RED);
        cg.setStroke(new BasicStroke(3.0f, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
        cg.drawLine(-30, 0, 30, 0);
        cg.drawLine(0, -30, 0, 30);
        cg.drawLine(20, -5, 30, 0);
        cg.drawLine(20, 5, 30, 0);
    }

    protected void paintGrid(Graphics2D g, Color color, int num, int step)
    {
        g.setColor(color);

        for (int x = -num; x <= num; x++)
        {
            g.drawLine(x * step, -num * step, x * step, num * step);
        }

        for (int y = -num; y <= num; y++)
        {
            g.drawLine(-num * step, y * step, num * step, y * step);
        }
    }

    protected void paintComponent(Graphics g)
    {
        g.setColor(getBackground()); // paint background
        g.fillRect(0, 0, getWidth(), getHeight());

        Graphics2D frame = (Graphics2D)g.create();
        Graphics2D world = (Graphics2D)g.create();
        
        synchronized(world2frame)
        {
            world2frame.setToIdentity();
            
            world2frame.translate((double)getWidth() / 2.0, (double)getHeight() / 2.0);
            world2frame.translate(centerX, centerY);
            world2frame.rotate(-Math.PI/2.0);
            
            double scale;
            if(getHeight() > getWidth())
            {
                scale = (double)getWidth() / 2.0 / visibleRange;
            }
            else
            {
                scale = (double)getHeight() / 2.0 / visibleRange;
            }
            world2frame.scale(scale, scale);
            
            world2frame.rotate(-worldCenter.rho);
            world2frame.translate(-worldCenter.x, -worldCenter.y);

            world.transform(world2frame);
        }

        synchronized(this)
        {
            
            for (int i=0; i<MAX_BACKGROUND_IMAGES; i++)
            {
                if (bgAssigned[i]){
                    AffineTransform at = new AffineTransform();
                    at.scale(bgW[i] / (double)(bgImg[i].getWidth()), bgH[i] / (double)(bgImg[i].getHeight()) );
                    at.rotate( Math.PI / 2);
                    at.translate(( bgY[i] * (double)(bgImg[i].getWidth()) / bgW[i])  - bgImg[i].getWidth()  / 2.0, 
                            (-bgX[i] * (double)(bgImg[i].getHeight()) / bgH[i]) - bgImg[i].getHeight() / 2.0);
                    BufferedImageOp biop;

                    if(bgBicubic[i])
                    {
                        biop = new AffineTransformOp(at, AffineTransformOp.TYPE_BICUBIC);
                    }
                    else
                    {
                        biop = new AffineTransformOp(at, AffineTransformOp.TYPE_NEAREST_NEIGHBOR);
                    }
                    world.drawImage(bgImg[i], biop, 0, 0);
                }
                else
                    break;
            }
        }
        
        if(showGrid)
        {
            //paintGrid(world, Color.LIGHT_GRAY, 100, 10000);  // 100 * 10m
            paintGrid(world, Color.GRAY, 84, 100000);  // 84 * 100m
        }

        for(int i = 0; i < mapViews.size(); i++)
        {
            mapViews.get(i).paintMapView(new MapViewGraphics(frame, world, robotPosition));
        }

        if(showCursor)
        {
            Point cursorPoint = getMousePosition();
            if(cursorPoint != null)
            {
                Graphics2D cursor = (Graphics2D)g.create();

                cursor.translate(cursorPoint.x, cursorPoint.y);
                cursor.rotate(cursorRho - Math.PI/2.0);
    
                paintCrosshairCursor(cursor);

                cursor.dispose();
            }
        }
        
        world.dispose(); // clean up
        frame.dispose();
    }

    public void zoomIn(double zoomFactor)
    {
        if(visibleRange > 1000)
        {
            visibleRange = visibleRange / zoomFactor;
            zoomRange    = visibleRange;               //a public copy of visibleRange 
            centerX = centerX * zoomFactor;
            centerY = centerY * zoomFactor;
    
            repaint();
        }
    }
    
    public void zoomOut(double zoomFactor)
    {
        if(visibleRange < 1000000)
        {
            visibleRange = visibleRange * zoomFactor;
            zoomRange    = visibleRange; 
            centerX = centerX / zoomFactor;
            centerY = centerY / zoomFactor;
    
            repaint();
        }
    }
    
    public void zoomCenter()
    {
        
        visibleRange = defaultVisibleRange;
        zoomRange    = visibleRange; 
        centerX = 0;
        centerY = 0;

        repaint();
    }
    
    public void removeListener()
    {
        zoomInButton.removeActionListener(zoomInAction);
        zoomOutButton.removeActionListener(zoomOutAction);
        zoomCenterButton.removeActionListener(zoomCenterAction);
        
        zoomInAction = null;
        zoomOutAction = null;
        zoomCenterAction = null;
        
        zoomInButton.removeKeyListener(keyListener);
        zoomOutButton.removeKeyListener(keyListener);
        zoomCenterButton.removeKeyListener(keyListener);

        this.removeKeyListener(keyListener);
        this.removeMouseListener(mouseListener);
        this.removeMouseMotionListener((MouseMotionListener)mouseListener);
        this.removeMouseWheelListener((MouseWheelListener)mouseListener);
        
        keyListener = null;
        mouseListener = null;
    }
    
    protected class MapViewComponentMouseListener extends MouseAdapter
            implements MouseMotionListener, MouseWheelListener
    {
        public void mouseClicked(MouseEvent e)
        {
            //Point mouse = e.getPoint();
        }

        public void mousePressed(MouseEvent e)
        {
            mousePressed = e.getPoint();
            
            int button13 = MouseEvent.BUTTON1_DOWN_MASK | MouseEvent.BUTTON3_DOWN_MASK;
            if((e.getModifiersEx() & button13) == button13)
            {
                zoomCenter();
            }
        }

        public void mouseReleased(MouseEvent e)
        {
            mousePressed = null;
        }

        public void mouseDragged(MouseEvent e)
        {
            if(mousePressed != null)
            {
                Point mouse = e.getPoint();

                double dx = (double)(mouse.x - mousePressed.x);
                double dy = (double)(mouse.y - mousePressed.y);
                    
                if((e.getModifiersEx() & MouseEvent.BUTTON3_DOWN_MASK) == MouseEvent.BUTTON3_DOWN_MASK)
                {
                    if(dy > 0)
                    {
                        zoomIn(1.0 + dy / 100.0);
                    }
                    if(dy < 0)
                    {
                        zoomOut(1.0 - dy / 100.0);
                    }
                }
                else
                {
                    centerX += dx;
                    centerY += dy;
                }
                mousePressed = mouse;
                repaint();
            }
        }
        
        public void mouseMoved(MouseEvent e)
        {
            //showCoords();
            if(showCursor)
            {
                repaint();
            }
            
        }
        
        public void mouseWheelMoved(MouseWheelEvent e)
        {
            if(showCursor)
            {
                cursorRho += Math.toRadians(10.0) * e.getWheelRotation();
            }
            else
            {
                if(e.getWheelRotation() > 0)
                {
                    zoomIn(defaultZoomFactor);
                }
                else
                {
                    zoomOut(defaultZoomFactor);
                }
            }
            //showCoords();
            repaint();
        }
       
    };

    protected class MapViewComponentKeyListener extends KeyAdapter
    {
        public void keyPressed(KeyEvent e)
        {
            if (e.getKeyCode() == KeyEvent.VK_UP)
            {
                centerY += 10;
                repaint();
            }
            else if (e.getKeyCode() == KeyEvent.VK_DOWN)
            {
                centerY -= 10;
                repaint();
            }
            else if (e.getKeyCode() == KeyEvent.VK_LEFT)
            {
                centerX += 10;
                repaint();
            }
            else if (e.getKeyCode() == KeyEvent.VK_RIGHT)
            {
                centerX -= 10;
                repaint();
            }
            else if ((e.getKeyCode() == KeyEvent.VK_PLUS) ||
                (e.getKeyCode() == KeyEvent.VK_ADD))
            {
                zoomIn(defaultZoomFactor);
            }
            else if ((e.getKeyCode() == KeyEvent.VK_MINUS) ||
                (e.getKeyCode() == KeyEvent.VK_SUBTRACT))
            {
                zoomOut(defaultZoomFactor);
            }
            else if (e.getKeyCode() == KeyEvent.VK_HOME)
            {
                zoomCenter();
            }
            else if ((e.getKeyCode() == KeyEvent.VK_PAGE_DOWN) ||
                     (e.getKeyCode() == KeyEvent.VK_H))
            {
                if(showCursor)
                {
                    cursorRho += Math.toRadians(10.0);
                }
                else
                {
                    zoomIn(defaultZoomFactor);
                }
                repaint();
            }
            else if ((e.getKeyCode() == KeyEvent.VK_PAGE_UP) ||
                     (e.getKeyCode() == KeyEvent.VK_G))
            {
                if(showCursor)
                {
                    cursorRho -= Math.toRadians(10.0);
                }
                else
                {
                    zoomOut(defaultZoomFactor);
                }
                repaint();
            }
        }
    };
    
    public boolean imageFillsFrame()
    {
        Position2d cUp = new Position2d();
        Position2d cDn = new Position2d();
        
        cDn.x = (int)(bgX[0] - bgH[0]/2);
        cDn.y = (int)(bgY[0] + bgW[0]/2);
        cDn.rho = 0.0f;
        
        cUp.x = (int)(bgX[0] + bgH[0]/2);
        cUp.y = (int)(bgY[0] - bgW[0]/2);
        cUp.rho = 0.0f;
        
        Point pDn = world2frame(cDn);
        Point pUp = world2frame(cUp);
        return ((pUp.x<=0) && (pUp.y<=0) && (pDn.x >= getWidth()) && (pDn.y >= getHeight()));
    }
}
