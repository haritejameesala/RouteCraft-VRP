import javax.swing.*;
import javax.swing.border.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.geom.*;
import java.util.*;
import java.util.List;
import javax.xml.parsers.*;
import org.w3c.dom.*;
import java.io.*;
import java.net.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
import java.util.concurrent.*;

/**
 * ROUTECRAFT — Premium Multi-Depot VRP Studio
 * Pure Java (standard JDK Swing/AWT only). No HTML/CSS/JS/Python/dependencies.
 */
public class RouteCraftVRP {
    // Visual system: parchment, evergreen, and antique brass.
    static final Color BG = new Color(242, 238, 228);
    static final Color SURFACE = new Color(255, 252, 245);
    static final Color SURFACE2 = new Color(246, 241, 230);
    static final Color LINE = new Color(218, 207, 187);
    static final Color TEXT = new Color(40, 53, 44);
    static final Color MUTED = new Color(116, 109, 94);
    static final Color JADE = new Color(49, 91, 68);
    static final Color BRASS = new Color(165, 122, 59);
    static final Color BLUE = new Color(74, 109, 129);
    static final Color ROSE = new Color(154, 83, 68);
    static final Color VIOLET = new Color(112, 96, 123);
    static final Color[] ROUTE_COLORS = {JADE, BLUE, BRASS, VIOLET, ROSE, new Color(73,130,115), new Color(151,139,79), new Color(174,112,75)};

    static class Node {
        int id, demand; double x,y; String name; boolean depot;
        Node(int id,String name,double x,double y,int demand,boolean depot){this.id=id;this.name=name;this.x=x;this.y=y;this.demand=demand;this.depot=depot;}
        String label(){return depot?name:"C"+id;}
    }
    static class Route {
        Node depot; List<Node> stops=new ArrayList<>();
        int load(){int n=0;for(Node x:stops)n+=x.demand;return n;}

    }
    // OpenStreetMap road graph. Coordinates are projected into the map's 0..100 canvas.
    static class RoadNode {
        long id; double lat,lon,x,y;
        List<RoadEdge> edges=new ArrayList<>();
        RoadNode(long id,double lat,double lon){this.id=id;this.lat=lat;this.lon=lon;}
    }
    static class RoadEdge {
        RoadNode to; double km,minutes,cost;
        RoadEdge(RoadNode to,double km,double minutes,double cost){this.to=to;this.km=km;this.minutes=minutes;this.cost=cost;}
    }
    static class Leg {
        List<RoadNode> path=new ArrayList<>(); double km,minutes,cost,score;
    }

    private final List<Node> customers=new ArrayList<>(), depots=new ArrayList<>();
    private final Map<Long,RoadNode> roadNodes=new LinkedHashMap<>();
    // Public OpenStreetMap raster tiles are fetched only for the currently imported map extent.
    private final Map<String,BufferedImage> tileCache=new java.util.concurrent.ConcurrentHashMap<>();
    private final Set<String> tileRequests=java.util.concurrent.ConcurrentHashMap.newKeySet();
    private final ExecutorService tileLoader=Executors.newFixedThreadPool(3,r->{Thread t=new Thread(r,"osm-tile-loader");t.setDaemon(true);return t;});
    private volatile boolean tileLoading=false;
    private boolean roadNetworkLoaded=false;
    private String roadNetworkName="No OSM road network loaded",roadNetworkPath="";
    private double minLat,maxLat,minLon,maxLon;
    private double weightDistance=0.40,weightTime=0.35,weightCost=0.25;
    private final JSlider weightDistanceSlider=new JSlider(0,100,40),weightTimeSlider=new JSlider(0,100,35),weightCostSlider=new JSlider(0,100,25);
    private final Map<String,Leg> legCache=new HashMap<>();
    private final JLabel roadStatus=value("ROAD GRAPH: NOT LOADED",10,ROSE);
    private List<Route> routes=new ArrayList<>();
    private int nextCustomerId=1,nextDepotId=2,vehicleCount=4,capacity=25;
    private final MapCanvas map=new MapCanvas();
    private final DefaultListModel<String> customerModel=new DefaultListModel<>(), depotModel=new DefaultListModel<>();
    private final JList<String> customersList=new JList<>(customerModel), depotsList=new JList<>(depotModel);
    private final JSpinner customerX=spin(50.0,0.0,100.0,1.0), customerY=spin(50.0,0.0,100.0,1.0), demand=spin(5,1,100,1);
    private final JSpinner depotX=spin(50.0,0.0,100.0,1.0), depotY=spin(50.0,0.0,100.0,1.0);
    private final JSpinner fleet=spin(4,1,50,1), cap=spin(25,1,10000,1);
    private final JComboBox<String> algorithm=new JComboBox<>(new String[]{"Clarke–Wright savings + 2-opt","Nearest neighbor + 2-opt"});
    private final JComboBox<String> mapMode=new JComboBox<>(new String[]{"Inspect nodes","Place customer","Place depot"});
    private final JLabel totalDistance=value("—",26,JADE), vehiclesUsed=value("—",24,BRASS), served=value("0",24,BLUE), totalDemand=value("0",24,VIOLET);
    private final JLabel distanceUnit=value("map units",10,MUTED);
    private final JLabel status=value("READY TO OPTIMIZE",10,JADE);
    private final JLabel cursor=value("X  —     Y  —",11,BRASS);
    private final JTextArea routeOutput=new JTextArea();
    private boolean placeCustomer,placeDepot;
    private double beforeLocalSearch=0, afterLocalSearch=0;
    private final java.util.Deque<String> undoStack=new java.util.ArrayDeque<>(), redoStack=new java.util.ArrayDeque<>();
    // Animated vehicle simulation state. Each route has its own phase, advanced by a Swing Timer.
    private final javax.swing.Timer animationTimer = new javax.swing.Timer(35, e -> tickAnimation());
    private boolean animationRunning = false;
    private double animationSpeed = 1.0;
    private double[] routeProgress = new double[0];
    private long lastTickNanos = System.nanoTime();
    private final JLabel simulationState = value("SIMULATION PAUSED", 10, MUTED);
    private final JComboBox<String> vehicleFocus = new JComboBox<>(new String[]{"All vehicles"});


    public static void main(String[] args){SwingUtilities.invokeLater(()->{try{UIManager.setLookAndFeel(UIManager.getCrossPlatformLookAndFeelClassName());}catch(Exception ignored){}new RouteCraftVRP().launch();});}
    RouteCraftVRP(){seed();refreshLists();configureUI();}
    private static JSpinner spin(Number v,Comparable min,Comparable max,Number step){return new JSpinner(new SpinnerNumberModel(v,min,max,step));}
    private void configureUI(){
        UIManager.put("ToolTip.background",SURFACE2);UIManager.put("ToolTip.foreground",TEXT);
        UIManager.put("ToolTip.font",new Font("Segoe UI",Font.PLAIN,12));
        UIManager.put("ScrollBar.thumb",LINE);UIManager.put("ScrollBar.track",SURFACE);
        UIManager.put("List.background",SURFACE);UIManager.put("List.foreground",TEXT);
        UIManager.put("OptionPane.background",SURFACE);UIManager.put("Panel.background",SURFACE);
        UIManager.put("OptionPane.messageForeground",TEXT);
    }
    private void launch(){
        JFrame f=new JFrame("ROUTECRAFT  /  Routing Intelligence");
        f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);f.setMinimumSize(new Dimension(1180,720));f.setSize(1600,1000);f.setExtendedState(JFrame.MAXIMIZED_BOTH);f.setLocationRelativeTo(null);
        f.setContentPane(buildUI());f.setVisible(true);refreshMetrics();
    }
    private JPanel buildUI(){
        JPanel root=new JPanel(new BorderLayout());root.setBackground(BG);root.add(header(),BorderLayout.NORTH);
        JPanel body=new JPanel(new BorderLayout(16,0));body.setBackground(BG);body.setBorder(new EmptyBorder(16,18,18,18));
        class SideHost extends JPanel implements Scrollable{SideHost(){super(new BorderLayout());setOpaque(false);}
            public Dimension getPreferredScrollableViewportSize(){return getPreferredSize();}
            public int getScrollableUnitIncrement(Rectangle r,int o,int d){return 24;}
            public int getScrollableBlockIncrement(Rectangle r,int o,int d){return 120;}
            public boolean getScrollableTracksViewportWidth(){return true;}
            public boolean getScrollableTracksViewportHeight(){return false;}}
        SideHost sideHost=new SideHost();sideHost.add(sidebar(),BorderLayout.CENTER);
        JScrollPane sideScroll=new JScrollPane(sideHost);sideScroll.getVerticalScrollBar().setUnitIncrement(24);sideScroll.setBorder(null);sideScroll.setHorizontalScrollBarPolicy(ScrollPaneConstants.HORIZONTAL_SCROLLBAR_NEVER);sideScroll.getViewport().setBackground(BG);sideScroll.setPreferredSize(new Dimension(326,700));body.add(sideScroll,BorderLayout.WEST);
        JPanel content=new JPanel(new BorderLayout(0,14));content.setOpaque(false);
        content.add(intro(),BorderLayout.NORTH);
        JPanel workspace=new JPanel(new BorderLayout(0,14));workspace.setOpaque(false);
        workspace.add(metrics(),BorderLayout.NORTH);
        JPanel mapCard=card(12);mapCard.setLayout(new BorderLayout(0,7));mapCard.setMinimumSize(new Dimension(600,470));
        JPanel mapHead=new JPanel();mapHead.setOpaque(false);mapHead.setLayout(new BoxLayout(mapHead,BoxLayout.Y_AXIS));
        JPanel mh=new JPanel();mh.setOpaque(false);mh.setLayout(new BoxLayout(mh,BoxLayout.Y_AXIS));
        mh.add(value("NETWORK OVERVIEW",12,TEXT));mh.add(Box.createVerticalStrut(4));mh.add(value(roadNetworkLoaded?"OpenStreetMap road network  /  shortest-path routing":"Road-aware routing workspace  /  import OSM to begin",10,MUTED));
        JPanel mapActions=new JPanel(new BorderLayout(8,0));mapActions.setOpaque(false);
        JPanel mc=new JPanel(new FlowLayout(FlowLayout.RIGHT,7,0));mc.setOpaque(false);
        styleCombo(vehicleFocus); vehicleFocus.setPreferredSize(new Dimension(125,30));
        vehicleFocus.addActionListener(e -> map.repaint());
        mc.add(vehicleFocus);
        JButton play=button("▶  DISPATCH",true); play.addActionListener(e -> startAnimation()); mc.add(play);
        JButton pause=button("Ⅱ  PAUSE",false); pause.addActionListener(e -> pauseAnimation()); mc.add(pause);
        JButton fit=button("FIT MAP",false);fit.setToolTipText("Fit the full road network into the enlarged map viewport");fit.addActionListener(e->{map.fit();map.repaint();});mc.add(fit);
        mapActions.add(mh,BorderLayout.WEST);mapActions.add(mc,BorderLayout.EAST);
        JPanel tools=new JPanel(new FlowLayout(FlowLayout.RIGHT,7,3));tools.setOpaque(false);
        JButton optimizeTop=button("✦  OPTIMIZE ROUTES",true);optimizeTop.setPreferredSize(new Dimension(178,34));optimizeTop.setToolTipText("Compute the multi-objective vehicle routes");optimizeTop.addActionListener(e->solve());tools.add(optimizeTop);
        JButton importRoads=button("LOAD REAL MAP + OSM ROADS",false);importRoads.setToolTipText("Import an OSM XML extract; the app then loads matching map tiles");importRoads.addActionListener(e->importOsmRoads());tools.add(importRoads);
        styleCombo(mapMode);mapMode.setPreferredSize(new Dimension(145,30));tools.add(mapMode);
        mapHead.add(mapActions);mapHead.add(tools);mapCard.add(mapHead,BorderLayout.NORTH);mapCard.add(map,BorderLayout.CENTER);
        JPanel foot=new JPanel(new BorderLayout());foot.setOpaque(false);
        JPanel footLeft=new JPanel(new FlowLayout(FlowLayout.LEFT,10,0));footLeft.setOpaque(false);
        footLeft.add(value("● CUSTOMER   ◆ DEPOT   ━ ROUTE",10,MUTED));
        footLeft.add(simulationState);
        JLabel speedLabel=value("SPEED",9,MUTED); footLeft.add(speedLabel);
        JSlider speed=new JSlider(1,5,2); speed.setOpaque(false); speed.setPreferredSize(new Dimension(90,24));
        speed.setToolTipText("Vehicle animation speed"); speed.addChangeListener(e -> { animationSpeed = speed.getValue() * 0.65; });
        footLeft.add(speed);
        foot.add(footLeft,BorderLayout.WEST);foot.add(cursor,BorderLayout.EAST);mapCard.add(foot,BorderLayout.SOUTH);
        workspace.add(mapCard,BorderLayout.CENTER);
        workspace.add(routeCard(),BorderLayout.SOUTH);
                class WsPanel extends JPanel implements Scrollable{WsPanel(){super(new BorderLayout());setOpaque(false);}
            public Dimension getPreferredScrollableViewportSize(){return getPreferredSize();}
            public int getScrollableUnitIncrement(Rectangle r,int o,int d){return 24;}
            public int getScrollableBlockIncrement(Rectangle r,int o,int d){return 120;}
            public boolean getScrollableTracksViewportWidth(){return true;}
            public boolean getScrollableTracksViewportHeight(){return getParent() instanceof JViewport&&getParent().getHeight()>getPreferredSize().height;}}
        WsPanel wsPanel=new WsPanel();wsPanel.add(workspace,BorderLayout.CENTER);
        JScrollPane wsScroll=new JScrollPane(wsPanel);wsScroll.setBorder(null);wsScroll.setOpaque(false);wsScroll.getViewport().setOpaque(false);wsScroll.getVerticalScrollBar().setUnitIncrement(24);wsScroll.setHorizontalScrollBarPolicy(ScrollPaneConstants.HORIZONTAL_SCROLLBAR_NEVER);content.add(wsScroll,BorderLayout.CENTER);body.add(content,BorderLayout.CENTER);root.add(body,BorderLayout.CENTER);
        mapMode.addActionListener(e->{String m=(String)mapMode.getSelectedItem();placeCustomer="Place customer".equals(m);placeDepot="Place depot".equals(m);});
        return root;
    }
    private JPanel header(){
        JPanel p=new JPanel(new BorderLayout());p.setBackground(new Color(35,53,43));p.setBorder(new CompoundBorder(new MatteBorder(0,0,2,0,BRASS),new EmptyBorder(15,22,15,22)));
        JPanel brand=new JPanel(new FlowLayout(FlowLayout.LEFT,13,0));brand.setOpaque(false);
        JPanel mark=new JPanel(new GridBagLayout());mark.setBackground(BRASS);mark.setPreferredSize(new Dimension(42,42));
        JLabel r=value("R",24,SURFACE);r.setFont(new Font("Georgia",Font.BOLD,24));mark.add(r);
        JPanel word=new JPanel();word.setOpaque(false);word.setLayout(new BoxLayout(word,BoxLayout.Y_AXIS));
        JLabel title=value("ROUTECRAFT",20,SURFACE);title.setFont(new Font("Georgia",Font.BOLD,20));
        word.add(title);word.add(Box.createVerticalStrut(4));word.add(value("ROUTING INTELLIGENCE  /  MULTI-DEPOT VRP",9,new Color(216,207,185)));
        brand.add(mark);brand.add(word);p.add(brand,BorderLayout.WEST);
        JPanel right=new JPanel(new FlowLayout(FlowLayout.RIGHT,10,7));right.setOpaque(false);
        right.add(pill("JAVA EDITION",new Color(201,214,194)));right.add(pill("DSA PROJECT",new Color(220,187,126)));p.add(right,BorderLayout.EAST);return p;
    }
    private JPanel intro(){
        JPanel p=new JPanel(new BorderLayout());p.setOpaque(false);
        JPanel left=new JPanel();left.setOpaque(false);left.setLayout(new BoxLayout(left,BoxLayout.Y_AXIS));
        JLabel title=value("Vehicle Routing Studio",29,TEXT);title.setFont(new Font("Georgia",Font.BOLD,24));
        left.add(title);left.add(Box.createVerticalStrut(5));
        left.add(value("Design a fleet. Place your network. Optimize every journey.",12,MUTED));p.add(left,BorderLayout.WEST);
        JPanel right=new JPanel(new FlowLayout(FlowLayout.RIGHT,0,14));right.setOpaque(false);right.add(pill("CAPACITATED ROUTING",JADE));p.add(right,BorderLayout.EAST);return p;
    }
    private JPanel sidebar(){
        JPanel s=card(14);s.setLayout(new BoxLayout(s,BoxLayout.Y_AXIS));
        s.add(section("01  /  SOLVER"));s.add(Box.createVerticalStrut(11));s.add(fieldLabel("Optimization method"));styleCombo(algorithm);algorithm.setMaximumSize(new Dimension(Integer.MAX_VALUE,34));s.add(algorithm);s.add(Box.createVerticalStrut(12));
        JPanel f=new JPanel(new GridLayout(1,2,9,0));f.setOpaque(false);f.add(field("Vehicles",fleet));f.add(field("Capacity / vehicle",cap));f.setMaximumSize(new Dimension(Integer.MAX_VALUE,57));s.add(f);
        s.add(Box.createVerticalStrut(12));s.add(section("OBJECTIVE WEIGHTS  /  DISTANCE · TIME · COST"));s.add(Box.createVerticalStrut(6));
        JPanel weights=new JPanel(new GridLayout(3,2,6,3));weights.setOpaque(false);
        JSlider wd=weightDistanceSlider,wt=weightTimeSlider,wc=weightCostSlider;
        for(JSlider sl:new JSlider[]{wd,wt,wc}){sl.setOpaque(false);sl.setPreferredSize(new Dimension(115,22));}
        weights.add(fieldLabel("Distance"));weights.add(wd);weights.add(fieldLabel("Travel time"));weights.add(wt);weights.add(fieldLabel("Operating cost"));weights.add(wc);weights.setMaximumSize(new Dimension(Integer.MAX_VALUE,78));s.add(weights);
        javax.swing.event.ChangeListener wl=e->{weightDistance=wd.getValue()/100.0;weightTime=wt.getValue()/100.0;weightCost=wc.getValue()/100.0;double sum=weightDistance+weightTime+weightCost;if(sum==0){weightDistance=1;weightTime=weightCost=0;}else{weightDistance/=sum;weightTime/=sum;weightCost/=sum;}legCache.clear();};wd.addChangeListener(wl);wt.addChangeListener(wl);wc.addChangeListener(wl);
        s.add(roadStatus);roadStatus.setAlignmentX(Component.LEFT_ALIGNMENT);roadStatus.setMaximumSize(new Dimension(Integer.MAX_VALUE,22));
        s.add(Box.createVerticalStrut(12));s.add(section("02  /  NEW CUSTOMER"));s.add(Box.createVerticalStrut(10));
        JPanel xy=new JPanel(new GridLayout(1,2,9,0));xy.setOpaque(false);xy.add(field("X · 0—100",customerX));xy.add(field("Y · 0—100",customerY));xy.setMaximumSize(new Dimension(Integer.MAX_VALUE,57));s.add(xy);s.add(Box.createVerticalStrut(9));
        s.add(field("Demand units",demand));s.add(Box.createVerticalStrut(9));
        JButton addC=button("+  ADD CUSTOMER",true);addC.setMaximumSize(new Dimension(Integer.MAX_VALUE,37));s.add(addC);
        addC.addActionListener(e->addCustomer((Double)customerX.getValue(),(Double)customerY.getValue(),(Integer)demand.getValue()));
        s.add(Box.createVerticalStrut(18));s.add(section("03  /  DEPOT NETWORK"));s.add(Box.createVerticalStrut(10));
        JPanel dxy=new JPanel(new GridLayout(1,2,9,0));dxy.setOpaque(false);dxy.add(field("Depot X",depotX));dxy.add(field("Depot Y",depotY));dxy.setMaximumSize(new Dimension(Integer.MAX_VALUE,57));s.add(dxy);s.add(Box.createVerticalStrut(9));
        JButton addD=button("+  ADD DEPOT",false);addD.setMaximumSize(new Dimension(Integer.MAX_VALUE,35));s.add(addD);addD.addActionListener(e->addDepot((Double)depotX.getValue(),(Double)depotY.getValue()));
        s.add(Box.createVerticalStrut(8));s.add(section("ACTIVE DEPOTS"));s.add(Box.createVerticalStrut(5));
        styleList(depotsList);s.add(scroll(depotsList,74));s.add(Box.createVerticalStrut(7));
        JPanel db=new JPanel(new GridLayout(1,2,8,0));db.setOpaque(false);JButton remD=button("REMOVE DEPOT",false),remC=button("REMOVE CUSTOMER",false);db.add(remD);db.add(remC);db.setMaximumSize(new Dimension(Integer.MAX_VALUE,32));s.add(db);
        remD.addActionListener(e->removeSelectedDepot());
        remC.addActionListener(e->removeSelectedCustomer(true));
        // Delete key removes the selected customer from the register or map selection.
        customersList.getInputMap(JComponent.WHEN_FOCUSED).put(KeyStroke.getKeyStroke(KeyEvent.VK_DELETE,0),"deleteCustomer");
        customersList.getActionMap().put("deleteCustomer",new AbstractAction(){public void actionPerformed(java.awt.event.ActionEvent e){removeSelectedCustomer(true);}});
        depotsList.getInputMap(JComponent.WHEN_FOCUSED).put(KeyStroke.getKeyStroke(KeyEvent.VK_DELETE,0),"deleteDepot");
        depotsList.getActionMap().put("deleteDepot",new AbstractAction(){public void actionPerformed(java.awt.event.ActionEvent e){removeSelectedDepot();}});
        s.add(Box.createVerticalStrut(14));s.add(section("CUSTOMER REGISTER"));s.add(Box.createVerticalStrut(5));styleList(customersList);s.add(scroll(customersList,110));
        s.add(Box.createVerticalStrut(12));JButton solve=button("✦  OPTIMIZE ROUTES",true);solve.setMaximumSize(new Dimension(Integer.MAX_VALUE,46));s.add(solve);solve.addActionListener(e->solve());
        s.add(Box.createVerticalStrut(7));JPanel low=new JPanel(new GridLayout(1,2,8,0));low.setOpaque(false);JButton sample=button("LOAD SAMPLE",false),clear=button("CLEAR ALL",false);low.add(sample);low.add(clear);low.setMaximumSize(new Dimension(Integer.MAX_VALUE,32));s.add(low);
        sample.addActionListener(e->{seed();refreshLists();pauseAnimation();routes.clear();routeProgress=new double[0];map.repaint();refreshMetrics();setStatus("Sample loaded");});
        clear.addActionListener(e->{if(customers.isEmpty()&&depots.size()==1)return;if(JOptionPane.showConfirmDialog(null,"Remove every customer and reset the depot network?","Clear network",JOptionPane.YES_NO_OPTION)!=JOptionPane.YES_OPTION)return;pushUndo();customers.clear();depots.clear();depots.add(new Node(1,"D1",50,50,0,true));nextCustomerId=1;nextDepotId=2;invalidateRoutes();refreshLists();map.repaint();refreshMetrics();setStatus("Network cleared · UNDO AVAILABLE");});
        s.add(Box.createVerticalStrut(8));
        JPanel fileActions=new JPanel(new GridLayout(3,2,7,7));fileActions.setOpaque(false);
        JButton edit=button("EDIT SELECTED",false),duplicate=button("DUPLICATE CUSTOMER",false),save=button("SAVE SCENARIO",false),load=button("OPEN SCENARIO",false),csv=button("EXPORT ROUTES",false),undo=button("UNDO",false),redo=button("REDO",false);
        fileActions.setLayout(new GridLayout(4,2,7,7));
        fileActions.add(edit);fileActions.add(duplicate);fileActions.add(save);fileActions.add(load);fileActions.add(csv);fileActions.add(undo);fileActions.add(redo);
        fileActions.setMaximumSize(new Dimension(Integer.MAX_VALUE,145));s.add(fileActions);
        edit.addActionListener(e->editSelectedCustomer());duplicate.addActionListener(e->duplicateSelectedCustomer());save.addActionListener(e->saveScenario());load.addActionListener(e->loadScenario());csv.addActionListener(e->exportRoutes());
        undo.addActionListener(e->{if(!undoStack.isEmpty()){redoStack.push(serializeScenario());restoreScenario(undoStack.pop());setStatus("UNDO APPLIED");}});
        redo.addActionListener(e->{if(!redoStack.isEmpty()){undoStack.push(serializeScenario());restoreScenario(redoStack.pop());setStatus("REDO APPLIED");}});
        s.add(Box.createVerticalGlue());return s;
    }
    private JPanel metrics(){
        JPanel p=new JPanel(new GridLayout(1,4,11,0));p.setOpaque(false);
        JPanel dm=card(13);dm.setLayout(new BorderLayout(0,7));dm.add(value("TOTAL DISTANCE",9,MUTED),BorderLayout.NORTH);dm.add(totalDistance,BorderLayout.CENTER);distanceUnit.setText(roadNetworkLoaded?"road kilometres":"map units");dm.add(distanceUnit,BorderLayout.SOUTH);dm.setPreferredSize(new Dimension(100,87));p.add(dm);p.add(metric("FLEET UTILIZATION","vehicles dispatched",vehiclesUsed,BRASS));p.add(metric("CUSTOMERS SERVED","delivery points",served,BLUE));p.add(metric("TOTAL DEMAND","units to deliver",totalDemand,VIOLET));return p;
    }
    private JPanel metric(String t,String sub,JLabel v,Color c){JPanel p=card(13);p.setLayout(new BorderLayout(0,7));p.add(value(t,9,MUTED),BorderLayout.NORTH);v.setForeground(c);p.add(v,BorderLayout.CENTER);p.add(value(sub,10,MUTED),BorderLayout.SOUTH);p.setPreferredSize(new Dimension(100,87));return p;}
    private JPanel routeCard(){
        JPanel p=card(10);p.setLayout(new BorderLayout(0,6));p.setPreferredSize(new Dimension(100,112));p.setMinimumSize(new Dimension(100,88));
        JPanel h=new JPanel(new BorderLayout());h.setOpaque(false);h.add(value("ROUTE MANIFEST",11,TEXT),BorderLayout.WEST);h.add(status,BorderLayout.EAST);p.add(h,BorderLayout.NORTH);
        routeOutput.setEditable(false);routeOutput.setFont(new Font("Consolas",Font.PLAIN,11));routeOutput.setBackground(SURFACE);routeOutput.setForeground(TEXT);routeOutput.setCaretColor(TEXT);
        JScrollPane sc=new JScrollPane(routeOutput);sc.setBorder(new LineBorder(LINE));sc.getViewport().setBackground(SURFACE);p.add(sc,BorderLayout.CENTER);return p;
    }
    private JPanel card(int pad){
        JPanel p=new JPanel(){
            protected void paintComponent(Graphics gg){
                Graphics2D g=(Graphics2D)gg.create();
                g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,RenderingHints.VALUE_ANTIALIAS_ON);
                g.setColor(SURFACE);g.fillRoundRect(0,0,getWidth()-1,getHeight()-1,16,16);
                g.setColor(LINE);g.drawRoundRect(0,0,getWidth()-1,getHeight()-1,16,16);
                g.dispose();
                super.paintComponent(gg);
            }
        };
        p.setOpaque(false);p.setBorder(new EmptyBorder(pad,pad,pad,pad));return p;
    }
    private JLabel value(String t,int size,Color c){JLabel l=new JLabel(t);l.setForeground(c);l.setFont(new Font("Segoe UI",Font.PLAIN,size));if(size>=22)l.setFont(new Font("Georgia",Font.BOLD,size));return l;}
    private JLabel section(String t){JLabel l=value(t,10,BRASS);l.setFont(new Font("Segoe UI",Font.BOLD,10));return l;}
    private JLabel fieldLabel(String t){return value(t,10,MUTED);}
    private JPanel field(String t,JSpinner sp){JPanel p=new JPanel(new BorderLayout(0,5));p.setOpaque(false);p.add(fieldLabel(t),BorderLayout.NORTH);sp.setFont(new Font("Segoe UI",Font.PLAIN,12));sp.setBorder(new CompoundBorder(new RoundedBorder(LINE,10),new EmptyBorder(1,2,1,2)));sp.setOpaque(false);sp.setPreferredSize(new Dimension(80,29));JComponent ed=sp.getEditor();if(ed instanceof JSpinner.DefaultEditor){JFormattedTextField tf=((JSpinner.DefaultEditor)ed).getTextField();tf.setBackground(SURFACE2);tf.setForeground(TEXT);tf.setCaretColor(TEXT);tf.setBorder(new EmptyBorder(4,6,4,6));}p.add(sp,BorderLayout.CENTER);return p;}
    private JPanel pill(String t,Color c){JPanel p=new JPanel();p.setOpaque(false);p.setBorder(new CompoundBorder(new RoundedBorder(new Color(c.getRed(),c.getGreen(),c.getBlue(),150),14),new EmptyBorder(6,10,6,10)));p.add(value(t,9,c));return p;}
    private JButton button(String t,boolean primary){
        JButton b=new JButton(t){
            protected void paintComponent(Graphics gg){
                Graphics2D g=(Graphics2D)gg.create();
                g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,RenderingHints.VALUE_ANTIALIAS_ON);
                Color fill=getBackground();
                if(getModel().isPressed())fill=fill.darker();
                else if(getModel().isRollover())fill=fill.brighter();
                g.setColor(fill);g.fillRoundRect(0,0,getWidth()-1,getHeight()-1,14,14);g.dispose();
                super.paintComponent(gg);
            }
        };
        b.setFont(new Font("Segoe UI",Font.BOLD,10));b.setFocusPainted(false);b.setCursor(Cursor.getPredefinedCursor(Cursor.HAND_CURSOR));
        b.setBackground(primary?JADE:SURFACE2);b.setForeground(primary?SURFACE:TEXT);
        b.setBorder(new CompoundBorder(new RoundedBorder(primary?JADE:LINE,14),new EmptyBorder(7,11,7,11)));
        b.setRolloverEnabled(true);b.setContentAreaFilled(false);b.setOpaque(false);return b;
    }
    private void styleCombo(JComboBox<?> c){c.setBackground(SURFACE2);c.setForeground(TEXT);c.setFont(new Font("Segoe UI",Font.PLAIN,11));c.setBorder(new CompoundBorder(new RoundedBorder(LINE,10),new EmptyBorder(1,4,1,4)));c.setOpaque(false);}
    private void styleList(JList<String> l){l.setBackground(SURFACE);l.setForeground(TEXT);l.setSelectionBackground(new Color(230,220,198));l.setSelectionForeground(TEXT);l.setFont(new Font("Segoe UI",Font.PLAIN,11));l.setFixedCellHeight(23);}
    private JScrollPane scroll(JList<String> l,int h){JScrollPane p=new JScrollPane(l);p.setPreferredSize(new Dimension(100,h));p.setBorder(new RoundedBorder(LINE,10));p.getViewport().setBackground(SURFACE);return p;}

    private static class RoundedBorder extends AbstractBorder{
        private final Color color;
        private final int radius;
        RoundedBorder(Color color,int radius){this.color=color;this.radius=radius;}
        public void paintBorder(Component c,Graphics gg,int x,int y,int width,int height){
            Graphics2D g=(Graphics2D)gg.create();
            g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,RenderingHints.VALUE_ANTIALIAS_ON);
            g.setColor(color);g.drawRoundRect(x,y,width-1,height-1,radius,radius);g.dispose();
        }
        public Insets getBorderInsets(Component c){return new Insets(1,1,1,1);}
    }

    private void seed(){customers.clear();depots.clear();pauseAnimation();routes.clear();routeProgress=new double[0];nextCustomerId=1;nextDepotId=2;depots.add(new Node(1,"D1",50,50,0,true));depots.add(new Node(2,"D2",10,15,0,true));
        double[][] xy={{20,25},{30,18},{42,25},{62,18},{77,27},{84,42},{72,48},{88,63},{72,77},{58,84},{42,76},{25,83},{15,64},{29,54},{42,43},{55,59},{66,32},{35,64},{52,44}};
        int[] ds={4,5,3,7,4,6,5,3,8,4,6,2,5,3,7,4,5,6,5};for(int i=0;i<xy.length;i++)customers.add(new Node(nextCustomerId++,"Customer",xy[i][0],xy[i][1],ds[i],false));
    }
    private void addCustomer(double x,double y,int d){pushUndo();customers.add(new Node(nextCustomerId++,"Customer",x,y,d,false));invalidateRoutes();refreshLists();map.repaint();refreshMetrics();setStatus("Customer added at ("+fmt(x)+", "+fmt(y)+") · UNDO AVAILABLE");}
    private void addDepot(double x,double y){pushUndo();depots.add(new Node(nextDepotId,"D"+nextDepotId,x,y,0,true));nextDepotId++;pauseAnimation();routes.clear();routeProgress=new double[0];refreshLists();map.repaint();refreshMetrics();setStatus("Depot added at ("+fmt(x)+", "+fmt(y)+")");}
    private void refreshLists(){customerModel.clear();for(Node n:customers)customerModel.addElement(String.format(Locale.US,"C%-3d  (%5s, %5s)  d=%d",n.id,fmt(n.x),fmt(n.y),n.demand));depotModel.clear();for(Node n:depots)depotModel.addElement(String.format(Locale.US,"%-3s  (%5s, %5s)",n.name,fmt(n.x),fmt(n.y)));}
    private void solve(){vehicleCount=(Integer)fleet.getValue();capacity=(Integer)cap.getValue();if(customers.isEmpty()||depots.isEmpty()){message("Add at least one customer and one depot.");return;}int td=customers.stream().mapToInt(n->n.demand).sum();if(td>vehicleCount*capacity){message("Total demand "+td+" exceeds total fleet capacity "+vehicleCount*capacity+".");return;}
        legCache.clear();routes=algorithm.getSelectedIndex()==0?savingsMultiDepot():nearestMultiDepot();for(Route r:routes){twoOpt(r);threeOptLite(r);}routes.removeIf(r->r.stops.isEmpty());
        routeProgress = new double[routes.size()];
        vehicleFocus.removeAllItems(); vehicleFocus.addItem("All vehicles");
        for(int i=0;i<routes.size();i++) vehicleFocus.addItem("Vehicle V"+(i+1));
        pauseAnimation();
        refreshMetrics();map.repaint();renderRoutes();int n=routes.stream().mapToInt(r->r.stops.size()).sum();
        setStatus(n==customers.size()&&routes.size()<=vehicleCount?"OPTIMIZATION COMPLETE":routes.size()>vehicleCount?"FLEET LIMIT EXCEEDED — increase vehicles":"INCOMPLETE — increase fleet/capacity");
    }
    private Map<Node,List<Node>> assign(){Map<Node,List<Node>> g=new LinkedHashMap<>();for(Node d:depots)g.put(d,new ArrayList<>());for(Node c:customers){Node best=depots.get(0);for(Node d:depots)if(dist(c,d)<dist(c,best))best=d;g.get(best).add(c);}return g;}
    private List<Route> nearestMultiDepot(){Map<Node,List<Node>> groups=assign();List<Route> out=new ArrayList<>();for(Node d:depots){List<Node> rem=new ArrayList<>(groups.get(d));while(!rem.isEmpty()&&out.size()<vehicleCount){Route r=new Route();r.depot=d;Node cur=d;while(true){Node best=null;double bd=Double.MAX_VALUE;for(Node n:rem)if(r.load()+n.demand<=capacity&&dist(cur,n)<bd){best=n;bd=dist(cur,n);}if(best==null)break;r.stops.add(best);rem.remove(best);cur=best;}if(!r.stops.isEmpty())out.add(r);}}return out;}
    private List<Route> savingsMultiDepot(){Map<Node,List<Node>> groups=assign();List<Route> out=new ArrayList<>();for(Node d:depots){List<Node> g=groups.get(d);List<Route> rs=new ArrayList<>();Map<Integer,Route> owner=new HashMap<>();for(Node n:g){Route r=new Route();r.depot=d;r.stops.add(n);rs.add(r);owner.put(n.id,r);}
            class S{Node a,b;double v;S(Node a,Node b,double v){this.a=a;this.b=b;this.v=v;}}List<S> sv=new ArrayList<>();for(int i=0;i<g.size();i++)for(int j=i+1;j<g.size();j++){Node a=g.get(i),b=g.get(j);sv.add(new S(a,b,dist(d,a)+dist(d,b)-dist(a,b)));}sv.sort((a,b)->Double.compare(b.v,a.v));
            for(S s:sv){Route a=owner.get(s.a.id),b=owner.get(s.b.id);if(a==b||a==null||b==null||a.load()+b.load()>capacity)continue;boolean ae=a.stops.get(a.stops.size()-1)==s.a,as=a.stops.get(0)==s.a,be=b.stops.get(b.stops.size()-1)==s.b,bs=b.stops.get(0)==s.b;List<Node> m=null;
                if(ae&&bs){m=new ArrayList<>(a.stops);m.addAll(b.stops);}else if(as&&be){m=new ArrayList<>(b.stops);m.addAll(a.stops);}else if(as&&bs){m=new ArrayList<>(a.stops);Collections.reverse(m);m.addAll(b.stops);}else if(ae&&be){m=new ArrayList<>(a.stops);List<Node> rev=new ArrayList<>(b.stops);Collections.reverse(rev);m.addAll(rev);}
                if(m!=null){a.stops=m;for(Node n:b.stops)owner.put(n.id,a);rs.remove(b);}}
            out.addAll(rs);}
        while(out.size()>vehicleCount){
            double best=Double.MAX_VALUE;int bi=-1,bj=-1;List<Node> bestStops=null;Node bestDepot=null;
            for(int i=0;i<out.size();i++)for(int j=i+1;j<out.size();j++){
                Route a=out.get(i),b=out.get(j);
                if(a.load()+b.load()>capacity)continue;
                for(int rev=0;rev<4;rev++){
                    List<Node> aa=new ArrayList<>(a.stops),bb=new ArrayList<>(b.stops);
                    if((rev&1)>0)Collections.reverse(aa);
                    if((rev&2)>0)Collections.reverse(bb);
                    List<Node> merged=new ArrayList<>(aa);merged.addAll(bb);
                    for(Node depot:depots){
                        double score=routeDistance(depot,merged);
                        if(score<best){best=score;bi=i;bj=j;bestStops=merged;bestDepot=depot;}
                    }
                }
            }
            if(bi<0)break;
            Route merged=out.get(bi);merged.stops=bestStops;merged.depot=bestDepot;out.remove(bj);
        }
        return out;
    }
    private void twoOpt(Route r){boolean changed=true;int loops=0;while(changed&&loops++<100){changed=false;for(int i=0;i<r.stops.size()-1;i++)for(int k=i+1;k<r.stops.size();k++){Node a=i==0?r.depot:r.stops.get(i-1),b=r.stops.get(i),c=r.stops.get(k),d=k==r.stops.size()-1?r.depot:r.stops.get(k+1);if(dist(a,c)+dist(b,d)+1e-9<dist(a,b)+dist(c,d)){Collections.reverse(r.stops.subList(i,k+1));changed=true;}}}}
    private void threeOptLite(Route r){
        int n=r.stops.size();if(n<5)return;boolean changed=true;int rounds=0;
        while(changed&&rounds++<2){changed=false;outer:for(int i=0;i<n-3;i++)for(int j=i+1;j<n-2;j++)for(int k=j+1;k<n-1;k++){
            List<Node> original=new ArrayList<>(r.stops),bestList=original;double best=routeDistance(r.depot,original);
            List<Node> cand=new ArrayList<>(original);Collections.reverse(cand.subList(i,j));double d=routeDistance(r.depot,cand);if(d+1e-8<best){best=d;bestList=cand;}
            cand=new ArrayList<>(original);Collections.reverse(cand.subList(j,k));d=routeDistance(r.depot,cand);if(d+1e-8<best){best=d;bestList=cand;}
            cand=new ArrayList<>(original);Collections.reverse(cand.subList(i,k));d=routeDistance(r.depot,cand);if(d+1e-8<best){best=d;bestList=cand;}
            if(bestList!=original){r.stops=bestList;changed=true;break outer;}
        }}
    }
    private double dist(Node a,Node b){return leg(a,b).score;}
    private double routeDistance(Node depot,List<Node> ns){if(depot==null||ns.isEmpty())return 0;double d=dist(depot,ns.get(0));for(int i=1;i<ns.size();i++)d+=dist(ns.get(i-1),ns.get(i));return d+dist(ns.get(ns.size()-1),depot);}
    private double routeKm(Node depot,List<Node> ns){if(depot==null||ns.isEmpty())return 0;double d=leg(depot,ns.get(0)).km;for(int i=1;i<ns.size();i++)d+=leg(ns.get(i-1),ns.get(i)).km;return d+leg(ns.get(ns.size()-1),depot).km;}
    private double routeMinutes(Node depot,List<Node> ns){if(depot==null||ns.isEmpty())return 0;double d=leg(depot,ns.get(0)).minutes;for(int i=1;i<ns.size();i++)d+=leg(ns.get(i-1),ns.get(i)).minutes;return d+leg(ns.get(ns.size()-1),depot).minutes;}
    private double routeCost(Node depot,List<Node> ns){if(depot==null||ns.isEmpty())return 0;double d=leg(depot,ns.get(0)).cost;for(int i=1;i<ns.size();i++)d+=leg(ns.get(i-1),ns.get(i)).cost;return d+leg(ns.get(ns.size()-1),depot).cost;}
    private Leg leg(Node a,Node b){
        String key=a.id+":"+(a.depot?"D":"C")+":"+b.id+":"+(b.depot?"D":"C");Leg cached=legCache.get(key);if(cached!=null)return cached;
        Leg out=new Leg();
        if(roadNetworkLoaded&&!roadNodes.isEmpty()){
            RoadNode start=nearestRoadNode(a.x,a.y),end=nearestRoadNode(b.x,b.y);
            if(start!=null&&end!=null){out=shortestLeg(start,end);if(out!=null){legCache.put(key,out);return out;}}
        }
        double eu=Math.hypot(a.x-b.x,a.y-b.y);out.km=eu;out.minutes=eu*2.0;out.cost=eu*12.0;out.score=objectiveScore(out.km,out.minutes,out.cost);legCache.put(key,out);return out;
    }
    private double objectiveScore(double km,double minutes,double cost){return weightDistance*km+weightTime*(minutes/60.0*30.0)+weightCost*(cost/12.0);}
    private RoadNode nearestRoadNode(double x,double y){RoadNode best=null;double bd=Double.MAX_VALUE;for(RoadNode n:roadNodes.values()){double d=Math.hypot(n.x-x,n.y-y);if(d<bd){bd=d;best=n;}}return best;}
    private Leg shortestLeg(RoadNode start,RoadNode target){
        Leg result=new Leg();if(start==target){result.path.add(start);return result;}
        Map<RoadNode,Double> ds=new HashMap<>();Map<RoadNode,RoadNode> prev=new HashMap<>();PriorityQueue<RoadNode> q=new PriorityQueue<>(Comparator.comparingDouble(n->ds.getOrDefault(n,Double.POSITIVE_INFINITY)));
        ds.put(start,0.0);q.add(start);Set<RoadNode> done=new HashSet<>();
        while(!q.isEmpty()){RoadNode u=q.poll();if(!done.add(u))continue;if(u==target)break;double du=ds.get(u);for(RoadEdge e:u.edges){double nd=du+objectiveScore(e.km,e.minutes,e.cost);if(nd<ds.getOrDefault(e.to,Double.POSITIVE_INFINITY)){ds.put(e.to,nd);prev.put(e.to,u);q.remove(e.to);q.add(e.to);}}}
        if(!ds.containsKey(target)){// Disconnected components: preserve a usable route but flag fallback via straight connector.
            result.km=geoKm(start.lat,start.lon,target.lat,target.lon);result.minutes=result.km*2;result.cost=result.km*12;result.score=objectiveScore(result.km,result.minutes,result.cost);result.path.add(start);result.path.add(target);return result;
        }
        List<RoadNode> rev=new ArrayList<>();RoadNode n=target;while(n!=null){rev.add(n);if(n==start)break;n=prev.get(n);}Collections.reverse(rev);result.path.addAll(rev);
        for(int i=0;i<rev.size()-1;i++){RoadNode u=rev.get(i),v=rev.get(i+1);RoadEdge edge=null;for(RoadEdge e:u.edges)if(e.to==v){edge=e;break;}if(edge!=null){result.km+=edge.km;result.minutes+=edge.minutes;result.cost+=edge.cost;}}
        result.score=objectiveScore(result.km,result.minutes,result.cost);return result;
    }
    private static double geoKm(double lat1,double lon1,double lat2,double lon2){double R=6371.0,dLat=Math.toRadians(lat2-lat1),dLon=Math.toRadians(lon2-lon1);double a=Math.sin(dLat/2)*Math.sin(dLat/2)+Math.cos(Math.toRadians(lat1))*Math.cos(Math.toRadians(lat2))*Math.sin(dLon/2)*Math.sin(dLon/2);return 2*R*Math.asin(Math.sqrt(a));}
    private void importOsmRoads(){JFileChooser fc=new JFileChooser();fc.setDialogTitle("Import OpenStreetMap XML (.osm)");fc.setFileFilter(new javax.swing.filechooser.FileNameExtensionFilter("OpenStreetMap XML (*.osm)","osm","xml"));if(fc.showOpenDialog(null)!=JFileChooser.APPROVE_OPTION)return;try{pushUndo();loadOsm(fc.getSelectedFile());setStatus("OSM ROAD GRAPH LOADED · OPTIMIZE AGAIN");}catch(Exception ex){message("Could not load OSM file: "+ex.getMessage());}}
    private void loadOsm(File file)throws Exception{
        DocumentBuilderFactory f=DocumentBuilderFactory.newInstance();f.setFeature("http://apache.org/xml/features/disallow-doctype-decl",true);f.setFeature("http://xml.org/sax/features/external-general-entities",false);f.setFeature("http://xml.org/sax/features/external-parameter-entities",false);f.setExpandEntityReferences(false);
        Document doc=f.newDocumentBuilder().parse(file);doc.getDocumentElement().normalize();Map<Long,RoadNode> nodes=new LinkedHashMap<>();NodeList nl=doc.getElementsByTagName("node");
        for(int i=0;i<nl.getLength();i++){Element e=(Element)nl.item(i);long id=Long.parseLong(e.getAttribute("id"));nodes.put(id,new RoadNode(id,Double.parseDouble(e.getAttribute("lat")),Double.parseDouble(e.getAttribute("lon"))));}
        NodeList ways=doc.getElementsByTagName("way");int edges=0;double loLat=Double.POSITIVE_INFINITY,hiLat=Double.NEGATIVE_INFINITY,loLon=Double.POSITIVE_INFINITY,hiLon=Double.NEGATIVE_INFINITY;
        for(RoadNode n:nodes.values()){loLat=Math.min(loLat,n.lat);hiLat=Math.max(hiLat,n.lat);loLon=Math.min(loLon,n.lon);hiLon=Math.max(hiLon,n.lon);}
        if(nodes.size()<2||ways.getLength()==0)throw new IOException("No usable OSM nodes/ways found. Export road data as OSM XML, not PBF.");
        minLat=loLat;maxLat=hiLat;minLon=loLon;maxLon=hiLon;double latSpan=Math.max(1e-9,maxLat-minLat),lonSpan=Math.max(1e-9,maxLon-minLon);
        double mercMin=mercatorY(minLat),mercMax=mercatorY(maxLat),mercSpan=Math.max(1e-9,mercMax-mercMin);
        for(RoadNode n:nodes.values()){n.x=100*(n.lon-minLon)/lonSpan;n.y=100*(mercatorY(n.lat)-mercMin)/mercSpan;}
        Set<String> roadTypes=new HashSet<>(Arrays.asList("motorway","motorway_link","trunk","trunk_link","primary","primary_link","secondary","secondary_link","tertiary","tertiary_link","unclassified","residential","living_street","service"));
        for(int i=0;i<ways.getLength();i++){Element way=(Element)ways.item(i);Map<String,String> tags=new HashMap<>();NodeList children=way.getChildNodes();List<Long> refs=new ArrayList<>();for(int j=0;j<children.getLength();j++)if(children.item(j) instanceof Element){Element c=(Element)children.item(j);if(c.getTagName().equals("tag"))tags.put(c.getAttribute("k"),c.getAttribute("v"));else if(c.getTagName().equals("nd"))refs.add(Long.parseLong(c.getAttribute("ref")));}
            if(!roadTypes.contains(tags.get("highway"))||refs.size()<2||"no".equals(tags.get("access"))||"no".equals(tags.get("motor_vehicle")))continue;
            double speed=defaultSpeed(tags.get("highway"));try{String ms=tags.get("maxspeed");if(ms!=null){String digits=ms.replaceAll("[^0-9.]","");if(!digits.isEmpty())speed=Double.parseDouble(digits);if(ms.toLowerCase(Locale.ROOT).contains("mph"))speed*=1.60934;}}catch(Exception ignored){}speed=Math.max(5,Math.min(120,speed));boolean oneway="yes".equals(tags.get("oneway"))||"1".equals(tags.get("oneway"))||"-1".equals(tags.get("oneway"));boolean reverse="-1".equals(tags.get("oneway"));double toll=("yes".equals(tags.get("toll"))?20.0:0.0);
            for(int j=0;j<refs.size()-1;j++){RoadNode a=nodes.get(refs.get(j)),b=nodes.get(refs.get(j+1));if(a==null||b==null)continue;double km=geoKm(a.lat,a.lon,b.lat,b.lon);if(km<=0)continue;double minutes=km/speed*60;double cost=km*12+toll;if(reverse)b.edges.add(new RoadEdge(a,km,minutes,cost));else{a.edges.add(new RoadEdge(b,km,minutes,cost));if(!oneway)b.edges.add(new RoadEdge(a,km,minutes,cost));}edges++;}}
        if(edges==0)throw new IOException("No routable highway edges found in this OSM extract.");roadNodes.clear();Set<RoadNode> usedRoadNodes=new HashSet<>();for(RoadNode rn:nodes.values())if(!rn.edges.isEmpty()){usedRoadNodes.add(rn);for(RoadEdge edge:rn.edges)usedRoadNodes.add(edge.to);}keepLargestRoadComponent(usedRoadNodes);for(RoadNode rn:usedRoadNodes)roadNodes.put(rn.id,rn);roadNetworkLoaded=true;roadNetworkName=file.getName();roadNetworkPath=file.getAbsolutePath();distanceUnit.setText("road kilometres");
        for(Node customer:customers){RoadNode snap=nearestRoadNode(customer.x,customer.y);if(snap!=null){customer.x=snap.x;customer.y=snap.y;}}
        for(Node depot:depots){RoadNode snap=nearestRoadNode(depot.x,depot.y);if(snap!=null){depot.x=snap.x;depot.y=snap.y;}}
        legCache.clear();routes.clear();routeProgress=new double[0];pauseAnimation();roadStatus.setText("OSM: "+roadNodes.size()+" nodes · "+edges+" road links");roadStatus.setForeground(JADE);map.fit();map.repaint();refreshMetrics();renderRoutes();loadMapTilesAsync();
    }
    /** Keeps only the largest strongly-connected road component (so every snapped stop can reach every other),
     *  prunes dangling edges, then re-fits the map extent and 0-100 coordinates to the roads that remain. */
    private void keepLargestRoadComponent(Set<RoadNode> used){
        List<RoadNode> all=new ArrayList<>(used);Map<RoadNode,List<RoadNode>> rev=new HashMap<>();
        for(RoadNode u:all)for(RoadEdge e:u.edges)rev.computeIfAbsent(e.to,k->new ArrayList<>()).add(u);
        // Kosaraju pass 1 (iterative): finishing order on the forward graph
        Set<RoadNode> seen=new HashSet<>();Deque<RoadNode> order=new ArrayDeque<>();
        for(RoadNode r:all){if(seen.contains(r))continue;Deque<Object[]> st=new ArrayDeque<>();seen.add(r);st.push(new Object[]{r,0});
            while(!st.isEmpty()){Object[] top=st.peek();RoadNode u=(RoadNode)top[0];int i=(Integer)top[1];if(i<u.edges.size()){top[1]=i+1;RoadNode v=u.edges.get(i).to;if(used.contains(v)&&seen.add(v))st.push(new Object[]{v,0});}else{st.pop();order.push(u);}}}
        // pass 2 on the reversed graph, in reverse finishing order
        Set<RoadNode> assigned=new HashSet<>();Set<RoadNode> best=new HashSet<>();
        for(RoadNode r:order){if(assigned.contains(r))continue;Set<RoadNode> comp=new HashSet<>();Deque<RoadNode> st=new ArrayDeque<>();st.push(r);assigned.add(r);
            while(!st.isEmpty()){RoadNode u=st.pop();comp.add(u);for(RoadNode v:rev.getOrDefault(u,Collections.emptyList()))if(assigned.add(v))st.push(v);}
            if(comp.size()>best.size())best=comp;}
        used.retainAll(best);
        for(RoadNode u:used)u.edges.removeIf(e->!used.contains(e.to));
        double loLat=Double.POSITIVE_INFINITY,hiLat=Double.NEGATIVE_INFINITY,loLon=Double.POSITIVE_INFINITY,hiLon=Double.NEGATIVE_INFINITY;
        for(RoadNode n:used){loLat=Math.min(loLat,n.lat);hiLat=Math.max(hiLat,n.lat);loLon=Math.min(loLon,n.lon);hiLon=Math.max(hiLon,n.lon);}
        minLat=loLat;maxLat=hiLat;minLon=loLon;maxLon=hiLon;
        double lonSpan=Math.max(1e-9,maxLon-minLon),mMin=mercatorY(minLat),mSpan=Math.max(1e-9,mercatorY(maxLat)-mMin);
        for(RoadNode n:used){n.x=100*(n.lon-minLon)/lonSpan;n.y=100*(mercatorY(n.lat)-mMin)/mSpan;}
    }
    private static double mercatorY(double lat){
        double clipped=Math.max(-85.05112878,Math.min(85.05112878,lat));
        double rad=Math.toRadians(clipped);
        return Math.log(Math.tan(Math.PI/4.0+rad/2.0));
    }
    private static int tileX(double lon,int z){return (int)Math.floor((lon+180.0)/360.0*(1<<z));}
    private static int tileY(double lat,int z){
        double r=Math.toRadians(Math.max(-85.05112878,Math.min(85.05112878,lat)));
        return (int)Math.floor((1.0-Math.log(Math.tan(r)+1.0/Math.cos(r))/Math.PI)/2.0*(1<<z));
    }
    private final Set<String> tileFailed=java.util.concurrent.ConcurrentHashMap.newKeySet();
    private void loadMapTilesAsync(){
        if(!roadNetworkLoaded)return;
        tileFailed.clear();
        roadStatus.setText("OSM: loading real map tiles…");roadStatus.setForeground(BRASS);
        map.repaint();
    }
    private static double lonFromTileX(double x,int z){return x/(double)(1<<z)*360.0-180.0;}
    private static double latFromTileY(double y,int z){double n=Math.PI-2.0*Math.PI*y/(1<<z);return Math.toDegrees(Math.atan(Math.sinh(n)));}
    private void requestTile(int z,int tx,int ty){
        final String key=z+"/"+tx+"/"+ty;
        if(tileCache.containsKey(key)||tileFailed.contains(key)||!tileRequests.add(key))return;
        tileLoader.submit(()->{
            try{
                URL url=new URL("https://tile.openstreetmap.org/"+key+".png");
                URLConnection conn=url.openConnection();
                conn.setRequestProperty("User-Agent","RouteCraftVRP/1.0 (educational DSA project)");
                conn.setConnectTimeout(4500);conn.setReadTimeout(6000);
                try(InputStream in=conn.getInputStream()){BufferedImage img=ImageIO.read(in);if(img!=null)tileCache.put(key,img);else tileFailed.add(key);}
            }catch(Exception ex){tileFailed.add(key);}finally{
                tileRequests.remove(key);
                SwingUtilities.invokeLater(()->{
                    map.repaint();
                    if(tileRequests.isEmpty()){roadStatus.setText("OSM: "+roadNodes.size()+" nodes · map tiles "+(tileCache.isEmpty()?"unavailable":"loaded"));roadStatus.setForeground(tileCache.isEmpty()?ROSE:JADE);}
                });
            }
        });
    }
    /** Draws OSM tiles for exactly the visible viewport, choosing the tile zoom from the current on-screen scale. */
    private void drawMapTiles(MapCanvas canvas,Graphics2D g){
        if(!roadNetworkLoaded)return;
        double scale=canvas.mapScale();                       // pixels per radian of longitude
        int z=(int)Math.round(Math.log(2*Math.PI*scale/256.0)/Math.log(2));
        z=Math.max(2,Math.min(19,z));
        double centerLon=(minLon+maxLon)/2.0,centerMerc=(mercatorY(minLat)+mercatorY(maxLat))/2.0;
        double cx=(canvas.getWidth()+54)/2.0+canvas.panX,cy=(canvas.getHeight()+42)/2.0+canvas.panY;
        int n=1<<z;
        // visible lon/lat -> tile index range
        double lonL=centerLon+Math.toDegrees((0-cx)/scale),lonR=centerLon+Math.toDegrees((canvas.getWidth()-cx)/scale);
        double mercT=centerMerc-(0-cy)/scale,mercB=centerMerc-(canvas.getHeight()-cy)/scale;
        int x0=(int)Math.floor((lonL+180.0)/360.0*n),x1=(int)Math.floor((lonR+180.0)/360.0*n);
        int y0=(int)Math.floor((1.0-mercT/Math.PI)/2.0*n),y1=(int)Math.floor((1.0-mercB/Math.PI)/2.0*n);
        x0=Math.max(0,x0);y0=Math.max(0,y0);x1=Math.min(n-1,x1);y1=Math.min(n-1,y1);
        if((long)(x1-x0+1)*(y1-y0+1)>80)return;               // safety: never flood the tile server
        if(tileCache.size()>600)tileCache.clear();
        g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BILINEAR);
        for(int tx=x0;tx<=x1;tx++)for(int ty=y0;ty<=y1;ty++){
            BufferedImage img=tileCache.get(z+"/"+tx+"/"+ty);
            int sx1=(int)Math.round(cx+Math.toRadians(lonFromTileX(tx,z)-centerLon)*scale);
            int sx2=(int)Math.round(cx+Math.toRadians(lonFromTileX(tx+1,z)-centerLon)*scale);
            int sy1=(int)Math.round(cy-(mercatorY(latFromTileY(ty,z))-centerMerc)*scale);
            int sy2=(int)Math.round(cy-(mercatorY(latFromTileY(ty+1,z))-centerMerc)*scale);
            if(img!=null){g.drawImage(img,sx1,sy1,sx2-sx1+1,sy2-sy1+1,null);continue;}
            requestTile(z,tx,ty);
            // fallback: stretch a coarser cached parent tile so the map never goes blank while loading
            for(int up=1;up<=4&&z-up>=0;up++){
                BufferedImage par=tileCache.get((z-up)+"/"+(tx>>up)+"/"+(ty>>up));
                if(par==null)continue;
                int span=256>>up,px=((tx&((1<<up)-1))*span),py=((ty&((1<<up)-1))*span);
                g.drawImage(par,sx1,sy1,sx2+1,sy2+1,px,py,px+span,py+span,null);break;
            }
        }
    }
    private static double defaultSpeed(String highway){if(highway==null)return 30;switch(highway){case "motorway":return 90;case "trunk":return 70;case "primary":return 55;case "secondary":return 45;case "tertiary":return 35;case "residential":case "living_street":return 20;default:return 30;}}
    private void refreshMetrics(){double d=0;for(Route r:routes)d+=routeKm(r.depot,r.stops);totalDistance.setText(routes.isEmpty()?"—":fmt(d));vehiclesUsed.setText(routes.isEmpty()?"—":routes.size()+" / "+vehicleCount);vehiclesUsed.setForeground(routes.size()>vehicleCount?ROSE:BRASS);served.setText(routes.isEmpty()?"0 / "+customers.size():routes.stream().mapToInt(r->r.stops.size()).sum()+" / "+customers.size());totalDemand.setText(String.valueOf(customers.stream().mapToInt(n->n.demand).sum()));}
    private void renderRoutes(){StringBuilder b=new StringBuilder();for(int i=0;i<routes.size();i++){Route r=routes.get(i);b.append(String.format(Locale.US,"V%-2d   %-3s",i+1,r.depot.name));for(Node n:r.stops)b.append("  →  C").append(n.id);b.append("  →  ").append(r.depot.name).append(String.format(Locale.US,"     LOAD %d/%d     %.2f km · %.1f min · ₹%.0f · score %.2f%n",r.load(),capacity,routeKm(r.depot,r.stops),routeMinutes(r.depot,r.stops),routeCost(r.depot,r.stops),routeDistance(r.depot,r.stops)));}if(routes.isEmpty())b.append("No routes computed yet.");routeOutput.setText(b.toString());routeOutput.setCaretPosition(0);}
    private void startAnimation(){
        if(routes.isEmpty()){message("Optimize routes before dispatching vehicles.");return;}
        if(routeProgress.length!=routes.size())routeProgress=new double[routes.size()];
        animationRunning=true;lastTickNanos=System.nanoTime();simulationState.setText("● LIVE DISPATCH");simulationState.setForeground(JADE);
        animationTimer.start();map.repaint();
    }
    private void pauseAnimation(){
        animationRunning=false;animationTimer.stop();simulationState.setText("SIMULATION PAUSED");simulationState.setForeground(MUTED);map.repaint();
    }
    private void tickAnimation(){
        long now=System.nanoTime();double dt=Math.min(0.12,(now-lastTickNanos)/1_000_000_000.0);lastTickNanos=now;
        for(int i=0;i<routes.size();i++){
            Route r=routes.get(i);double length=Math.max(1.0,routeDistance(r.depot,r.stops));
            // Progress is normalized over the whole closed route, so the marker travels depot -> stops -> depot.
            routeProgress[i]=(routeProgress[i]+dt*animationSpeed*7.5/length)%1.0;
        }
        map.repaint();
    }
    private Point2D.Double vehiclePosition(Route r,double progress){
        List<Point2D.Double> path=new ArrayList<>();List<Node> stops=new ArrayList<>();stops.add(r.depot);stops.addAll(r.stops);stops.add(r.depot);
        for(int i=0;i<stops.size()-1;i++){Leg l=leg(stops.get(i),stops.get(i+1));if(l.path.size()>1){for(int j=0;j<l.path.size();j++){RoadNode rn=l.path.get(j);if(path.isEmpty()||j>0)path.add(new Point2D.Double(rn.x,rn.y));}}else{Node a=stops.get(i),b=stops.get(i+1);if(path.isEmpty())path.add(new Point2D.Double(a.x,a.y));path.add(new Point2D.Double(b.x,b.y));}}
        if(path.size()<2)return new Point2D.Double(r.depot.x,r.depot.y);double total=0;double[] lens=new double[path.size()-1];for(int i=0;i<lens.length;i++){lens[i]=Math.hypot(path.get(i+1).x-path.get(i).x,path.get(i+1).y-path.get(i).y);total+=lens[i];}double target=progress*total;
        for(int i=0;i<lens.length;i++){if(target<=lens[i]||i==lens.length-1){double t=lens[i]<1e-9?0:target/lens[i];Point2D.Double a=path.get(i),b=path.get(i+1);return new Point2D.Double(a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t);}target-=lens[i];}return path.get(path.size()-1);
    }
    private void invalidateRoutes(){pauseAnimation();routes.clear();routeProgress=new double[0];renderRoutes();}
    private void removeSelectedCustomer(boolean confirm){
        int i=customersList.getSelectedIndex();
        if(i<0){message("Select a customer in the Customer Register first.");return;}
        Node n=customers.get(i);
        if(confirm&&JOptionPane.showConfirmDialog(null,"Delete "+n.label()+" (demand "+n.demand+")? This can be undone.","Delete customer",JOptionPane.YES_NO_OPTION)!=JOptionPane.YES_OPTION)return;
        pushUndo();customers.remove(i);invalidateRoutes();refreshLists();map.repaint();refreshMetrics();setStatus(n.label()+" DELETED · UNDO AVAILABLE");
    }
    private void removeSelectedDepot(){
        int i=depotsList.getSelectedIndex();
        if(i<0){message("Select a depot first.");return;}
        if(depots.size()<=1){message("At least one depot must remain.");return;}
        Node n=depots.get(i);
        if(JOptionPane.showConfirmDialog(null,"Remove depot "+n.name+"? Routes will need to be optimized again.","Remove depot",JOptionPane.YES_NO_OPTION)!=JOptionPane.YES_OPTION)return;
        pushUndo();depots.remove(i);invalidateRoutes();refreshLists();map.repaint();refreshMetrics();setStatus("DEPOT REMOVED · UNDO AVAILABLE");
    }
    private void duplicateSelectedCustomer(){
        int i=customersList.getSelectedIndex();
        if(i<0){message("Select a customer to duplicate.");return;}
        Node n=customers.get(i);pushUndo();
        double x=Math.min(100,n.x+2.0),y=Math.min(100,n.y+2.0);
        customers.add(new Node(nextCustomerId++,"Customer",x,y,n.demand,false));
        invalidateRoutes();refreshLists();customersList.setSelectedIndex(customers.size()-1);map.repaint();refreshMetrics();setStatus("CUSTOMER DUPLICATED · UNDO AVAILABLE");
    }
    private void pushUndo(){undoStack.push(serializeScenario());redoStack.clear();if(undoStack.size()>30)undoStack.removeLast();}
    private String serializeScenario(){
        StringBuilder b=new StringBuilder("ROUTECRAFT,1\n");
        for(Node d:depots)b.append("D,").append(d.id).append(',').append(d.name).append(',').append(d.x).append(',').append(d.y).append(",0\n");
        for(Node c:customers)b.append("C,").append(c.id).append(',').append(c.name).append(',').append(c.x).append(',').append(c.y).append(',').append(c.demand).append('\n');
        String encodedPath=java.util.Base64.getEncoder().encodeToString(roadNetworkPath.getBytes(java.nio.charset.StandardCharsets.UTF_8));
        b.append("P,").append(weightDistance).append(',').append(weightTime).append(',').append(weightCost).append(',').append(encodedPath).append('\n');
        return b.toString();
    }
    private void restoreScenario(String data){
        customers.clear();depots.clear();routes.clear();nextCustomerId=1;nextDepotId=1;String restoredPath="";double wd=0.40,wt=0.35,wc=0.25;
        for(String line:data.split("\\R")){String[] p=line.split(",",-1);try{
            if(p[0].equals("P")&&p.length>=5){wd=Double.parseDouble(p[1]);wt=Double.parseDouble(p[2]);wc=Double.parseDouble(p[3]);restoredPath=new String(java.util.Base64.getDecoder().decode(p[4]),java.nio.charset.StandardCharsets.UTF_8);continue;}
            if(p.length<6)continue;
            if(p[0].equals("D")){int id=Integer.parseInt(p[1]);depots.add(new Node(id,p[2],Double.parseDouble(p[3]),Double.parseDouble(p[4]),0,true));nextDepotId=Math.max(nextDepotId,id+1);}
            else if(p[0].equals("C")){int id=Integer.parseInt(p[1]);customers.add(new Node(id,p[2],Double.parseDouble(p[3]),Double.parseDouble(p[4]),Integer.parseInt(p[5]),false));nextCustomerId=Math.max(nextCustomerId,id+1);}
        }catch(Exception ignored){}}
        weightDistance=wd;weightTime=wt;weightCost=wc;weightDistanceSlider.setValue((int)Math.round(wd*100));weightTimeSlider.setValue((int)Math.round(wt*100));weightCostSlider.setValue((int)Math.round(wc*100));legCache.clear();
        if(depots.isEmpty())depots.add(new Node(1,"D1",50,50,0,true));
        if(!restoredPath.isEmpty()&&new File(restoredPath).isFile()){try{loadOsm(new File(restoredPath));}catch(Exception ex){roadNetworkLoaded=false;roadNodes.clear();roadStatus.setText("OSM FILE UNAVAILABLE");roadStatus.setForeground(ROSE);}}
        else{roadNetworkLoaded=false;roadNodes.clear();roadNetworkPath="";distanceUnit.setText("map units");roadStatus.setText("ROAD GRAPH: NOT LOADED");roadStatus.setForeground(ROSE);}
        pauseAnimation();routeProgress=new double[0];refreshLists();refreshMetrics();map.repaint();renderRoutes();
    }
    private void saveScenario(){JFileChooser c=new JFileChooser();c.setSelectedFile(new java.io.File("routecraft-scenario.csv"));if(c.showSaveDialog(null)==JFileChooser.APPROVE_OPTION)try{java.nio.file.Files.writeString(c.getSelectedFile().toPath(),serializeScenario(),java.nio.charset.StandardCharsets.UTF_8);setStatus("SCENARIO SAVED");}catch(Exception ex){message("Save failed: "+ex.getMessage());}}
    private void loadScenario(){JFileChooser c=new JFileChooser();if(c.showOpenDialog(null)==JFileChooser.APPROVE_OPTION)try{pushUndo();restoreScenario(java.nio.file.Files.readString(c.getSelectedFile().toPath(),java.nio.charset.StandardCharsets.UTF_8));setStatus("SCENARIO LOADED");}catch(Exception ex){message("Load failed: "+ex.getMessage());}}
    private void exportRoutes(){if(routes.isEmpty()){message("Optimize routes before exporting.");return;}JFileChooser c=new JFileChooser();c.setSelectedFile(new java.io.File("route-plan.csv"));if(c.showSaveDialog(null)==JFileChooser.APPROVE_OPTION)try{StringBuilder b=new StringBuilder("vehicle,depot,stop_order,customer_id,x,y,demand,route_load,route_km,route_minutes,route_cost,objective_score\n");for(int i=0;i<routes.size();i++){Route r=routes.get(i);int k=1;for(Node n:r.stops)b.append("V").append(i+1).append(',').append(r.depot.name).append(',').append(k++).append(',').append(n.id).append(',').append(fmt(n.x)).append(',').append(fmt(n.y)).append(',').append(n.demand).append(',').append(r.load()).append(',').append(fmt(routeKm(r.depot,r.stops))).append(',').append(fmt(routeMinutes(r.depot,r.stops))).append(',').append(fmt(routeCost(r.depot,r.stops))).append(',').append(fmt(routeDistance(r.depot,r.stops))).append('\n');}java.nio.file.Files.writeString(c.getSelectedFile().toPath(),b.toString(),java.nio.charset.StandardCharsets.UTF_8);setStatus("ROUTES EXPORTED");}catch(Exception ex){message("Export failed: "+ex.getMessage());}}
    private void editSelectedCustomer(){int i=customersList.getSelectedIndex();if(i<0){message("Select a customer first.");return;}Node n=customers.get(i);JSpinner x=spin(n.x,0.0,100.0,1.0),y=spin(n.y,0.0,100.0,1.0),d=spin(n.demand,1,100,1);JPanel p=new JPanel(new GridLayout(3,2,8,8));p.add(new JLabel("X coordinate"));p.add(x);p.add(new JLabel("Y coordinate"));p.add(y);p.add(new JLabel("Demand"));p.add(d);if(JOptionPane.showConfirmDialog(null,p,"Edit customer C"+n.id,JOptionPane.OK_CANCEL_OPTION)==JOptionPane.OK_OPTION){pushUndo();n.x=(Double)x.getValue();n.y=(Double)y.getValue();n.demand=(Integer)d.getValue();invalidateRoutes();refreshLists();refreshMetrics();map.repaint();setStatus("CUSTOMER UPDATED · UNDO AVAILABLE");}}
    private void setStatus(String s){status.setText(s);}
    private void message(String s){JOptionPane.showMessageDialog(null,s,"RouteCraft",JOptionPane.INFORMATION_MESSAGE);}
    private static String fmt(double n){return String.format(Locale.US,"%.1f",n);}

    class MapCanvas extends JPanel{
        double zoom=1,panX=0,panY=0;Point drag;double oldX,oldY;Node hoveredNode=null;
        MapCanvas(){setBackground(new Color(239,234,220));setPreferredSize(new Dimension(1100,650));setMinimumSize(new Dimension(600,360));addMouseMotionListener(new MouseMotionAdapter(){public void mouseMoved(MouseEvent e){Point2D.Double p=world(e.getX(),e.getY());cursor.setText(String.format(Locale.US,roadNetworkLoaded?"LON/LAT AREA  %5.1f / %5.1f":"X  %5.1f     Y  %5.1f",p.x,p.y));Node oldHover=hoveredNode;hoveredNode=nearest(p.x,p.y);if(oldHover!=hoveredNode)repaint();}public void mouseDragged(MouseEvent e){if(drag!=null){panX=oldX+e.getX()-drag.x;panY=oldY+e.getY()-drag.y;repaint();}}});
            addMouseListener(new MouseAdapter(){public void mousePressed(MouseEvent e){if(SwingUtilities.isRightMouseButton(e)||SwingUtilities.isMiddleMouseButton(e)){drag=e.getPoint();oldX=panX;oldY=panY;}}public void mouseReleased(MouseEvent e){drag=null;}public void mouseClicked(MouseEvent e){if(SwingUtilities.isRightMouseButton(e)||SwingUtilities.isMiddleMouseButton(e))return;Point2D.Double p=world(e.getX(),e.getY());p.x=clamp(p.x,0,100);p.y=clamp(p.y,0,100);if(placeCustomer){addCustomer(p.x,p.y,(Integer)demand.getValue());mapMode.setSelectedIndex(0);}else if(placeDepot){addDepot(p.x,p.y);mapMode.setSelectedIndex(0);}else{Node n=nearest(p.x,p.y);if(n!=null){cursor.setText(n.label()+"   X "+fmt(n.x)+"   Y "+fmt(n.y)+"   D "+n.demand);if(n.depot){for(int i=0;i<depots.size();i++)if(depots.get(i)==n)depotsList.setSelectedIndex(i);}else{for(int i=0;i<customers.size();i++)if(customers.get(i)==n)customersList.setSelectedIndex(i);}}}}});
            addMouseWheelListener(e->{double old=zoom;zoom=clamp(zoom*(e.getWheelRotation()<0?1.25:0.8),.5,roadNetworkLoaded?200:6);double k=zoom/old;double cx0=(getWidth()+54)/2.0,cy0=(getHeight()+42)/2.0;if(roadNetworkLoaded){panX=(e.getX()-cx0)-(e.getX()-cx0-panX)*k;panY=(e.getY()-cy0)-(e.getY()-cy0-panY)*k;}repaint();});}
        void fit(){zoom=1;panX=0;panY=0;}
        double mapScale(){
            double ww=Math.max(1,getWidth()-54),hh=Math.max(1,getHeight()-42);
            if(!roadNetworkLoaded)return Math.min(ww/100.0,hh/100.0)*zoom;
            double xSpan=Math.max(1e-9,Math.toRadians(maxLon-minLon));
            double ySpan=Math.max(1e-9,mercatorY(maxLat)-mercatorY(minLat));
            return Math.min(ww/xSpan,hh/ySpan)*zoom;
        }
        double mapLeft(){double ww=Math.max(1,getWidth()-54);if(!roadNetworkLoaded){double side=ww;return (getWidth()-side)/2+panX;}double sc=mapScale(),xSpan=Math.toRadians(maxLon-minLon);return (getWidth()+54)/2-(xSpan*sc)/2+panX;}
        double mapTop(){double hh=Math.max(1,getHeight()-42);if(!roadNetworkLoaded){double side=hh;return (getHeight()-side)/2+panY;}double sc=mapScale(),ySpan=mercatorY(maxLat)-mercatorY(minLat);return (getHeight()+42)/2-(ySpan*sc)/2+panY;}
        Point2D.Double world(int x,int y){
            if(roadNetworkLoaded){double sc=mapScale();double centerLon=(minLon+maxLon)/2.0,centerMerc=(mercatorY(minLat)+mercatorY(maxLat))/2.0;double lon=centerLon+((x-(getWidth()+54)/2.0-panX)/sc)*180.0/Math.PI;double merc=centerMerc-((y-(getHeight()+42)/2.0-panY)/sc);double lat=Math.toDegrees(2*Math.atan(Math.exp(merc))-Math.PI/2);return new Point2D.Double(100*(lon-minLon)/Math.max(1e-9,maxLon-minLon),100*(mercatorY(lat)-mercatorY(minLat))/Math.max(1e-9,mercatorY(maxLat)-mercatorY(minLat)));}
            double ww=Math.max(1,getWidth()-54)*zoom,hh=Math.max(1,getHeight()-42)*zoom,ox=(getWidth()-ww)/2+panX,oy=(getHeight()-hh)/2+panY;return new Point2D.Double((x-ox)/ww*100,100-(y-oy)/hh*100);
        }
        int sx(double x){
            if(roadNetworkLoaded){double lon=minLon+(x/100.0)*(maxLon-minLon);double centerLon=(minLon+maxLon)/2.0;return (int)((getWidth()+54)/2.0+panX+(Math.toRadians(lon-centerLon))*mapScale());}
            double ww=Math.max(1,getWidth()-54)*zoom;return (int)((getWidth()-ww)/2+panX+x/100*ww);
        }
        int sy(double y){
            if(roadNetworkLoaded){double merc=mercatorY(minLat)+(y/100.0)*(mercatorY(maxLat)-mercatorY(minLat));double centerMerc=(mercatorY(minLat)+mercatorY(maxLat))/2.0;return (int)((getHeight()+42)/2.0+panY-(merc-centerMerc)*mapScale());}
            double hh=Math.max(1,getHeight()-42)*zoom;return (int)((getHeight()-hh)/2+panY+(100-y)/100*hh);
        }
        protected void paintComponent(Graphics gg){super.paintComponent(gg);Graphics2D g=(Graphics2D)gg.create();g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,RenderingHints.VALUE_ANTIALIAS_ON);int w=getWidth(),h=getHeight();double side=Math.min(w-86,h-86)*zoom,ox=(w-side)/2+panX,oy=(h-side)/2+panY;
            double ww=Math.max(1,w-54)*zoom,hh=Math.max(1,h-42)*zoom;ox=(w-ww)/2+panX;oy=(h-hh)/2+panY;
            if(roadNetworkLoaded){double sc=mapScale(),xSpan=Math.toRadians(maxLon-minLon),ySpan=mercatorY(maxLat)-mercatorY(minLat);ww=xSpan*sc;hh=ySpan*sc;ox=(w+54)/2.0-ww/2+panX;oy=(h+42)/2.0-hh/2+panY;}
            if(roadNetworkLoaded){Graphics2D tg=(Graphics2D)g.create();tg.clipRect(0,0,w,h);drawMapTiles(this,tg);tg.dispose();}
            if(!roadNetworkLoaded){
                // Deliberately quiet coordinate grid until real road data is imported.
                for(int i=0;i<=10;i++){int x=(int)(ox+ww*i/10),y=(int)(oy+hh*i/10);g.setColor(new Color(218,207,187));g.drawLine(x,(int)oy,x,(int)(oy+hh));g.drawLine((int)ox,y,(int)(ox+ww),y);}
                g.setColor(new Color(190,177,151));g.drawRoundRect((int)ox,(int)oy,(int)ww,(int)hh,10,10);
                g.setColor(MUTED);g.setFont(new Font("Georgia",Font.PLAIN,13));g.drawString("Import an OpenStreetMap .osm extract to display real roads",(int)ox+18,(int)oy+24);
            } else {
                g.setColor(new Color(45,76,98,105));g.setStroke(new BasicStroke(1.25f,BasicStroke.CAP_ROUND,BasicStroke.JOIN_ROUND));
                for(RoadNode rn:roadNodes.values())for(RoadEdge re:rn.edges)if(rn.id<re.to.id){g.drawLine(sx(rn.x),sy(rn.y),sx(re.to.x),sy(re.to.y));}
            }
            Node selectedNode=null;
            int selectedCustomer=customersList.getSelectedIndex(),selectedDepot=depotsList.getSelectedIndex();
            if(selectedCustomer>=0&&selectedCustomer<customers.size())selectedNode=customers.get(selectedCustomer);
            else if(selectedDepot>=0&&selectedDepot<depots.size())selectedNode=depots.get(selectedDepot);
            int focusedVehicle=vehicleFocus.getSelectedIndex()-1;
            for(int i=0;i<routes.size();i++){
                if(focusedVehicle>=0&&focusedVehicle!=i)continue;
                Route route=routes.get(i);
                Color routeColor=ROUTE_COLORS[i%ROUTE_COLORS.length];
                g.setStroke(new BasicStroke(7.0f,BasicStroke.CAP_ROUND,BasicStroke.JOIN_ROUND));
                g.setColor(new Color(SURFACE.getRed(),SURFACE.getGreen(),SURFACE.getBlue(),210));
                drawRoute(g,route);
                g.setStroke(new BasicStroke(3.5f,BasicStroke.CAP_ROUND,BasicStroke.JOIN_ROUND));
                g.setColor(new Color(routeColor.getRed(),routeColor.getGreen(),routeColor.getBlue(),225));
                drawRoute(g,route);
            }
            // Small, low-contrast coordinate ticks are only useful in synthetic mode.
            for(Node d:depots){int x=sx(d.x),y=sy(d.y);g.setColor(new Color(JADE.getRed(),JADE.getGreen(),JADE.getBlue(),38));g.fillOval(x-18,y-18,36,36);g.setColor(JADE);g.setStroke(new BasicStroke(2));g.fill(new Polygon(new int[]{x,x+9,x,x-9},new int[]{y-9,y,y+9,y},4));if(hoveredNode==d||selectedNode==d){g.setFont(new Font("Segoe UI",Font.BOLD,11));g.drawString(d.name,x+12,y-9);g.setColor(TEXT);g.setFont(new Font("Segoe UI",Font.PLAIN,10));g.drawString("Depot · "+fmt(d.x)+", "+fmt(d.y),x+12,y+5);}}
            for(Node n:customers){int x=sx(n.x),y=sy(n.y);g.setColor(new Color(BLUE.getRed(),BLUE.getGreen(),BLUE.getBlue(),35));g.fillOval(x-12,y-12,24,24);g.setColor(BLUE);g.setStroke(new BasicStroke(1.5f));g.drawOval(x-7,y-7,14,14);g.fillOval(x-2,y-2,4,4);if(hoveredNode==n||selectedNode==n){g.setFont(new Font("Segoe UI",Font.BOLD,10));g.setColor(TEXT);g.drawString("C"+n.id+" · d="+n.demand,x+11,y-7);}}
            if(hoveredNode!=null){int hx=sx(hoveredNode.x),hy=sy(hoveredNode.y);g.setColor(new Color(245,190,95,220));g.setStroke(new BasicStroke(1.5f));g.drawOval(hx-16,hy-16,32,32);}
            // Premium animated vehicle markers follow each computed route.
            if(animationRunning && routeProgress.length==routes.size()){
                int focus=vehicleFocus.getSelectedIndex()-1;
                for(int i=0;i<routes.size();i++){
                    if(focus>=0&&focus!=i)continue;
                    Route r=routes.get(i);Point2D.Double pos=vehiclePosition(r,routeProgress[i]);int x=sx(pos.x),y=sy(pos.y);Color c=ROUTE_COLORS[i%ROUTE_COLORS.length];
                    // Soft glow and vehicle capsule
                    g.setColor(new Color(c.getRed(),c.getGreen(),c.getBlue(),42));g.fillOval(x-17,y-17,34,34);
                    g.setColor(new Color(10,15,23,235));g.fillRoundRect(x-13,y-9,26,18,7,7);
                    g.setColor(c);g.setStroke(new BasicStroke(1.8f));g.drawRoundRect(x-13,y-9,26,18,7,7);
                    g.fillRoundRect(x-7,y-6,11,7,3,3);g.fillRoundRect(x+5,y-6,3,7,2,2);
                    g.setColor(TEXT);g.setFont(new Font("Segoe UI",Font.BOLD,9));g.drawString("V"+(i+1),x+15,y-12);
                }
            }
            if(roadNetworkLoaded){g.setColor(new Color(255,255,255,230));g.fillRoundRect((int)ox+6,(int)(oy+hh-22),205,16,6,6);g.setColor(new Color(45,64,82));g.setFont(new Font("Segoe UI",Font.PLAIN,9));g.drawString("© OpenStreetMap contributors",(int)ox+12,(int)(oy+hh-10));}
            if(routes.isEmpty()&&roadNetworkLoaded){g.setColor(new Color(45,64,82));g.setFont(new Font("Segoe UI",Font.PLAIN,12));g.drawString("Road network loaded · click OPTIMIZE ROUTES to calculate fleet routes",(int)ox+15,(int)oy+Math.min(24,(int)hh-8));}g.dispose();}
        void drawRoute(Graphics2D g,Route r){if(r.stops.isEmpty())return;List<Node> ns=new ArrayList<>();ns.add(r.depot);ns.addAll(r.stops);ns.add(r.depot);Path2D p=new Path2D.Double();p.moveTo(sx(ns.get(0).x),sy(ns.get(0).y));for(int i=0;i<ns.size()-1;i++){Leg l=leg(ns.get(i),ns.get(i+1));if(roadNetworkLoaded&&l.path.size()>1){for(RoadNode rn:l.path)p.lineTo(sx(rn.x),sy(rn.y));}else p.lineTo(sx(ns.get(i+1).x),sy(ns.get(i+1).y));}g.draw(p);}
        Node nearest(double x,double y){Node best=null;double bd=Double.MAX_VALUE;for(Node n:customers){double d=Math.hypot(n.x-x,n.y-y);if(d<bd){bd=d;best=n;}}for(Node n:depots){double d=Math.hypot(n.x-x,n.y-y);if(d<bd){bd=d;best=n;}}return bd<5?best:null;}
    }
    private static double clamp(double v,double lo,double hi){return Math.max(lo,Math.min(hi,v));}
}
