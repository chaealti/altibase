/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/*******************************************************************************
    2003.05.27
    bluetheme Work it...

properties file   : DBPool.prop
    DBURL     : jdbc:Altibase://127.0.0.1:20536/mydb
    className : Altibase.jdbc.driver.AltibaseDriver
    DBUser    : SYS
    DBPasswd  : MANAGER
    initPools : 5
    maxPools  : 10
    timeout   : 300
*******************************************************************************/

import java.net.*;
import java.io.*;
import java.util.*;
import java.text.*; // because of Date Format
import java.sql.*;
import java.util.Properties;

public class DBPool {
    static private int connectedCount;   // pool�� �ִ� ��� ���� �� connection ��
    private Vector vector= new Vector(); // Pool

    private String poolName;
    private String databaseURL;
    private String driverPath;
    private String driverURL;
    private String user;
    private String password;

    private int maxConnCount;
    private int initConnCount;
    private int timeOut;

    private FileInputStream fis;
    private Properties readProp;

    private String propFileName = new String("DBPool.prop");

/*******************************************************************************
  connection�� ���� ������ ��� �����ϰ� init() method call
*******************************************************************************/
    public DBPool() {
        getProperties();
        initPool(initConnCount);
        //debug("New Pool created.");
    }

/*******************************************************************************
    initConnCount �� ��ŭ createConnection �޼ҵ带 ȣ���� ��
    ��ȯ�Ǵ� connection ��ü�� ���Ϳ� �ִ´�.
*******************************************************************************/
    private void initPool(int initConnCount) {
        for(int i= 0; i<initConnCount; i++) {
            Connection connection= createConnection();
            vector.addElement(connection);
        }
    }

/*******************************************************************************
    getConnection(int )�� ȣ���Ͽ� (���ܰ� �߻����� �ʴ´ٸ�!)
    connection ��ü�� �����ݴϴ�.
*******************************************************************************/
    public Connection getConnection() throws SQLException {
        try {
            return getConnection(timeOut*1000);
        }
        catch(SQLException exception) {
            //debug(exception);
            throw exception;
        }
    }

/*******************************************************************************
    connection ��ü�� �����ִ� method
    Ǯ���� connection ��ü�� �����´�.
    case 1 : ��ü�� ��������. -> ���°� ������ ����(isConnection())���� �����ش�.
    case 2 : �����°� null �̴�! -> �ִ� ��û ������ �ʰ��ߴ�. (�� ����� ����)
      case 2.1. : �־��� �Ⱓ���� ��ٸ���.
        case 2.1.1 : timeout �ð� ���� ��ٸ��� connection ��ü�� ��ȭ�Ǹ� ������ �� �����ش�.
        case 2.1.2 : timeout ���� connection ��ü�� ��ȯ�Ǿ ���� ���� �� �����ش�.
        case 2.1.3 : timeout�� ������ connection ���� ���ϸ� SQLException�� �߻���ŵ�ϴ�.
*******************************************************************************/
    private synchronized Connection getConnection(long timeout) throws SQLException {
        Connection connection= null;

        long startTime= System.currentTimeMillis();

        long currentTime= 0;
        long remaining= timeout;

        while((connection=getPoolConnection())==null) {
            try {
      	        //debug("all connection is used. waiting for connection. "+remaining+" milliseconds remaining..");
                wait(remaining);
            }
            catch(InterruptedException exception) {
                //debug(exception);
            }
            catch(Exception exception) {
                //debug("Unknown error.");
            }
            currentTime= System.currentTimeMillis();
            remaining= timeout-(currentTime-startTime);

            if(remaining<=0) {
                throw new SQLException("connection time out.");
            }
        }

        if(!isConnection(connection)) {
            //debug("get select bad connection. retry get connection from pool in the "+remaining+" time");
            return (getConnection(remaining));
        }

        //debug("Delivered connection from pool.");
        connectedCount++;
        //debug("getConnection : "+connection.toString());
        return connection;
    }

/*******************************************************************************
    �ش� connection ��ü�� ���¸� �˻��ϴ� �޼ҵ�
    ���� ����    : ����̹��� �翬�� ���� �ְ�, �����͵� ���ӵǾ� �ִ� ���
    ���� �� ���� : ����̹��� ���� �ִµ�, ������ ������ �ȵǾ� �ִ� ���
    JDBC�� API�� boolean Connection.isClosed() method �ε� ���� ���¸�
    �Ǻ��� ���� ���ϹǷ� DB���� statement�� ��������.
    ���ܰ� �߻��Ѵٸ� ������ �Ǿ� ���� �ʴٴ� ��.
*******************************************************************************/
    private boolean isConnection(Connection connection) {
        Statement statement= null;
        try {
            if(!connection.isClosed()) {
      	        statement= connection.createStatement();
      	        statement.close();
            }
            else {
      	        return false;
            }
        }
        catch(SQLException exception) {
            if(statement!=null) {
      	        try { statement.close(); } catch(SQLException ignored) {}
            }
            //debug("Pooled Connection was not okay");
            return false;
        }
        return true;
    }

/*******************************************************************************
    Ǯ���� connection ��ü�� �������� �޼ҵ�
    case 1 : Ǯ�� connection ��ü�� ������ ��û�� ������ �����ְ�, ���Ϳ��� ����.
    case 2 : ���� Ǯ�� ��ü�� ���µ�, �ƽø��� �ƴ϶�� createConnection�� ȣ���Ͽ� connection��
      �����ϰ� �����ش�.
    case 3 : Ǯ���� �������� ����, �ƽø��� �Ѿ��ٸ� null�� �����ش�.
  ���� : ���� �� ���� createConnection �޼ҵ�� Ǯ�� addElement�� ���� �ʴ´�.
    connection ��ü�� ���� ������ �ٷ� �����ְ� ������ �� �躤�Ϳ� �߰��Ѵ�.
*******************************************************************************/
    private Connection getPoolConnection() {
        Connection connection= null;
        if(vector.size()>0) {
            connection= (Connection)vector.firstElement();
            vector.removeElementAt(0);
        }
        else if(connectedCount<maxConnCount) {
            //debug("create new connection");
            connection= createConnection();
        }
        return connection;
    }

/*******************************************************************************
    connection ��ü�� �����ϴ� �޼ҵ�
    �ش� ����̹� ������ ��Ÿ ��� ������ �̿��ؼ� connection ��ü�� ����� �޼ҵ��Դϴ�.
    ���� ����� ���� �����ϴ�.
*******************************************************************************/
    private Connection createConnection() {
        Connection connection= null;
        try {
            Class.forName(driverURL);
        }
        catch(ClassNotFoundException exception) {
            //debug("class not found..");
        }

        try {
            connection= DriverManager.getConnection(databaseURL, user, password);
        }
        catch(SQLException exception) {
            //debug(exception);
        }
        return connection;
    }
/*******************************************************************************
    DB connection �� ���� �� ������ ���Ͽ��� �о�´�.
    fileName : DBPool.prop
    ���� : property�� ��� ������ ��ҹ��ڸ� �����Ѵ�.
*******************************************************************************/
    public void getProperties() {
        try
        {
            fis = new FileInputStream(propFileName);
            readProp = new Properties();

            readProp.load(fis);

            poolName = readProp.getProperty("POOLNAME");
            //debug("poolname : " + poolName);
            databaseURL = readProp.getProperty("DBURL");
            //debug("databaseURL : " + databaseURL);
            driverURL = readProp.getProperty("CLASSNAME");
            //debug("driverURL : " + driverURL);
            user = readProp.getProperty("DBUSER");
            //debug("user : " + user);
            password = readProp.getProperty("DBPASSWD");
            //debug("password : " + password);

            maxConnCount = Integer.parseInt(readProp.getProperty("MAXPOOLS"));
            //debug("maxConnCount : " + maxConnCount);
            initConnCount = Integer.parseInt(readProp.getProperty("INITPOOLS"));
            //debug("initConnCount : " + initConnCount);
            timeOut = Integer.parseInt(readProp.getProperty("TIMEOUT"));
            //debug("timeOut : " + timeOut);
        }
        catch (Exception e) {
            e.printStackTrace();
        }
	    finally {
	        try {
                fis.close();
	        }
	        catch (IOException e) {
                e.printStackTrace();
            }
	    }
    }
/*******************************************************************************
    connection ��ü�� �ݳ���Ű�� �޼ҵ�
    ���� : �ش� connection ��ü�� close() ��Ű�� ���� �ƴ�
*******************************************************************************/
    public synchronized void freeConnection(Connection connection) {
        vector.addElement(connection);
        connectedCount--;
        notifyAll();
        //debug("Returned connection to Pool");
    }

/*******************************************************************************
  explanation      : Ǯ�� ��� �ڿ��� ���������� �޼ҵ�
*******************************************************************************/
    public synchronized void release() {
        //debug("all connection releasing.");
        Enumeration enum = vector.elements();
        while(enum.hasMoreElements()) {
            Connection connection= (Connection)enum.nextElement();
            try {
      	        connection.close();
      	        //debug("Closed connection.");
            }
            catch(SQLException exception) {
      	        //debug("Couldn't close connection");
            }
        }
        vector.removeAllElements();
    }

/*******************************************************************************
    ����� �޼ҵ��
*******************************************************************************/
    public DBPool getDBPool() { return this; }
    public void debug(Exception exception) { exception.printStackTrace(); }
    public void debug(String string) { System.out.println(string); }
    public void printStates() {
        debug("pool states : max : "+maxConnCount+"   created : "+connectedCount+"   free : "+(maxConnCount-connectedCount));
    }
    public void printVector() {
        for(int i= 0; i<vector.size(); i++) {
            debug("[ "+(i+1)+" ] : "+(vector.get(i)).toString());
        }
    }
}
