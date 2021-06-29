/*
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

package Altibase.jdbc.driver.cm;

import Altibase.jdbc.driver.AltibaseMessageCallback;
import Altibase.jdbc.driver.AltibaseVersion;
import Altibase.jdbc.driver.ClobReader;
import Altibase.jdbc.driver.LobConst;
import Altibase.jdbc.driver.datatype.ColumnFactory;
import Altibase.jdbc.driver.datatype.ListBufferHandle;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.logging.DumpByteUtil;
import Altibase.jdbc.driver.logging.LoggingProxy;
import Altibase.jdbc.driver.logging.MultipleFileHandler;
import Altibase.jdbc.driver.logging.TraceFlag;
import Altibase.jdbc.driver.util.*;

import java.io.IOException;
import java.io.Reader;
import java.math.BigInteger;
import java.net.*;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.nio.charset.CoderResult;
import java.nio.charset.CodingErrorAction;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.*;
import java.util.regex.Pattern;

import static Altibase.jdbc.driver.datatype.ListBufferHandle.BUFFER_ALLOC_UNIT_4_SIMPLE;
import static Altibase.jdbc.driver.datatype.ListBufferHandle.BUFFER_INIT_SIZE_4_SIMPLE;

public class CmChannel extends CmBufferWriter
{
    static final byte NO_OP = -1;

    private static final int CHANNEL_STATE_CLOSED = 0;
    private static final int CHANNEL_STATE_WRITABLE = 1;
    private static final int CHANNEL_STATE_READABLE = 2;
    private static final String[] CHANNEL_STATE_STRING = {"CHANNEL_STATE_CLOSED", "CHANNEL_STATE_WRITABLE", "CHANNEL_STATE_READABLE"};

    public  static final int   CM_BLOCK_SIZE                   = 32 * 1024;
    public  static final int   CM_PACKET_HEADER_SIZE           = 16;
    private static final byte  CM_PACKET_HEADER_VERSION        = AltibaseVersion.CM_MAJOR_VERSION;
    private static final byte  CM_PACKET_HEADER_RESERVED_BYTE  = 0;
    private static final short CM_PACKET_HEADER_RESERVED_SHORT = 0;
    private static final int   CM_PACKET_HEADER_RESERVED_INT   = 0;

    private static final Pattern IP_PATTERN = Pattern.compile("(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}|\\[[^\\]]+\\])");

    // PROJ-2583 jdbc logging
    private static final String JRE_DEFAULT_VERSION = "1.4.";

    private short mSessionID = 0;

    private int mState;
    private CmConnType mConnType = CmConnType.TCP;    // PROJ-2681
    private CmSocket mSocket;
    private CharBuffer mCharBuf;
    private byte[] mLobBuf;
    private AltibaseReadableCharChannel mReadChannel4Clob;
    private ColumnFactory mColumnFactory = new ColumnFactory();

    /* packet header */
    private short mPayloadSize = 0;
    private boolean mIsLastPacket = true;
    private boolean mIsRedundantPacket = false;

    private ByteBuffer mPrimitiveTypeBuf;
    private static final int MAX_PRIMITIVE_TYPE_SIZE = 8;

    private int mNextSendSeqNo = 0;
    private int mPrevReceivedSeqNo = -1;

    private CharsetDecoder mDBDecoder = CharsetUtils.newAsciiDecoder();
    private CharsetDecoder mNCharDecoder = CharsetUtils.newAsciiDecoder();

    private boolean mIsPreferIPv6 = false;

    private boolean mRemoveRedundantMode = false;

    private int CLOB_BUF_SIZE;

    private int mResponseTimeout;

    private int mLobCacheThreshold;

    // PROJ-2681
    private AltibaseProperties mProps;

    private transient Logger mCmLogger;

    // PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning
    public static final int CM_DEFAULT_SNDBUF_SIZE = CM_BLOCK_SIZE * 2;
    public static final int CM_DEFAULT_RCVBUF_SIZE = CM_BLOCK_SIZE * 2;
    public static final int CHAR_VARCHAR_COLUMN_SIZE = 32000;

    private int mSockRcvBufSize = CM_DEFAULT_RCVBUF_SIZE;

    private CmProtocolContext mAsyncContext;

    // BUG-45237 DB Message protocol�� ó���� �ݹ�
    private AltibaseMessageCallback mMessageCallback;

    // BUG-46443 �Ϲ� bind parameter�� list protocol�� �����Ҷ� ����� ��ü
    private ListBufferHandle mTempListBufferHandle;

    // Channel for Target
    private class AltibaseReadableCharChannel implements ReadableCharChannel
    {
        private final CharBuffer mCharBuf  = CharBuffer.allocate(LobConst.LOB_BUFFER_SIZE);
        private boolean          mIsOpen;
        private CoderResult      mResult;
        private Reader           mSrcReader;
        private char[]           mSrcBuf;
        private int              mSrcBufPos;
        private int              mSrcBufLimit;

        private int              mUserReadableLength = -1;
        private int              mTotalActualCharReadLength = 0;

        public void open(Reader aSrc) throws IOException
        {
            close();

            mSrcReader = aSrc;
            mCharBuf.clear();
            mIsOpen = true;
        }

        public void open(Reader aSrc, int aUserReadableLength) throws IOException
        {
            close();

            mSrcReader = aSrc;
            mCharBuf.clear();
            mIsOpen = true;
            mUserReadableLength = aUserReadableLength;
        }

        public void open(char[] aSrc) throws IOException
        {
            open(aSrc, 0, aSrc.length);
        }

        public void open(char[] aSrc, int aOffset, int aLength) throws IOException
        {
            close();

            mSrcBuf = aSrc;
            mSrcBufPos = aOffset;
            mSrcBufLimit = aOffset + aLength;
            mCharBuf.clear();
            mIsOpen = true;
        }

        public int read(ByteBuffer aDst) throws IOException
        {
            if (isClosed())
            {
                Error.throwIOException(ErrorDef.STREAM_ALREADY_CLOSED);
            }

            if (mUserReadableLength > 0)
            {
                // BUG-44553 clob �����͸� ��� �о����� mCharBuf�� compact�� �����Ͱ� ���� ������ �������� �ʴ´�.
                if (mUserReadableLength <= mTotalActualCharReadLength && mCharBuf.position() == 0)
                {
                    return -1;
                }
            }

            mTotalActualCharReadLength += readClobData();

            return copyToSocketBuffer(aDst);
        }

        /**
         * mCharBuf�� encoding�� �� ���� ���۷� �����Ѵ�.
         * @param aDst ���� ����
         * @return ������ ���� ���۷� ���ڵ��Ǿ� ����� ������.
         */
        private int copyToSocketBuffer(ByteBuffer aDst)
        {
            int sReaded = -1;

            if (mCharBuf.flip().remaining() > 0)
            {
                int sBeforeBytePos4Dst = aDst.position();
                mResult = mDBEncoder.encode(mCharBuf, aDst, true);
                sReaded = aDst.position() - sBeforeBytePos4Dst;

                if (mResult.isOverflow())
                {
                    mCharBuf.compact();
                }
                else
                {
                    mCharBuf.clear();
                }
            }

            return sReaded;
        }

        /**
         * Src�κ��� clob �����͸� �о mCharBuf ���ۿ� �����Ѵ�.
         * @return ������ �о���� clob ������ ������
         * @throws IOException read ���� I/O ������ �߻����� ��
         */
        private int readClobData() throws IOException
        {
            int sReadableCharLength = getReadableCharLength();
            if (sReadableCharLength <= 0) // BUG-44553 �о���� clob�����Ͱ� ������������ read�� ����
            {
                return 0;
            }

            int sActualCharReadLength = 0;
            if (mSrcBuf == null)
            {
                sActualCharReadLength = mSrcReader.read(mCharBuf.array(), mCharBuf.position(), sReadableCharLength);
                if (sActualCharReadLength > 0)
                {
                    mCharBuf.position(mCharBuf.position() + sActualCharReadLength);
                }
            }
            else
            {
                if (mSrcBufPos < mSrcBufLimit)
                {
                    sActualCharReadLength = Math.min(sReadableCharLength, mSrcBufLimit - mSrcBufPos);
                    mCharBuf.put(mSrcBuf, mSrcBufPos, sActualCharReadLength);
                    mSrcBufPos += sActualCharReadLength;
                }
            }

            return sActualCharReadLength;
        }

        /**
         * clob �����͸� �о���� �� �ִ� �ִ� ����� ���Ѵ�.
         * @return ���� �� �ִ� �ִ� ������
         */
        private int getReadableCharLength()
        {
            int sReadableCharLength;

            if (mUserReadableLength > 0)
            {
                // BUG-44553 ���� ���� �� ��ŭ�� ���������� ���� ����
                sReadableCharLength = Math.min(mUserReadableLength - mTotalActualCharReadLength, CLOB_BUF_SIZE);
                sReadableCharLength = Math.min(mCharBuf.remaining(), sReadableCharLength);
            }
            else
            {
                sReadableCharLength = Math.min(mCharBuf.remaining(), CLOB_BUF_SIZE);
            }

            return sReadableCharLength;
        }

        public boolean isOpen()
        {
            return mIsOpen;
        }

        public void close() throws IOException
        {
            if (!isOpen())
            {
                return;
            }

            mUserReadableLength = -1;
            mTotalActualCharReadLength = 0;

            mIsOpen = false;
            mSrcReader = null;
            mSrcBuf = null;
        }

        public Object getSource()
        {
            if (mSrcReader != null)
            {
                return mSrcReader;
            }
            else
            {
                return mSrcBuf;
            }
        }
    }

    public CmChannel()
    {
        mBuffer = ByteBuffer.allocateDirect(CM_BLOCK_SIZE);
        mCharBuf = CharBuffer.allocate(CM_BLOCK_SIZE);
        mCharVarcharColumnBuffer = ByteBuffer.allocate(CHAR_VARCHAR_COLUMN_SIZE);
        mTempListBufferHandle = new ListBufferHandle(BUFFER_INIT_SIZE_4_SIMPLE, BUFFER_ALLOC_UNIT_4_SIMPLE);
        mState = CHANNEL_STATE_CLOSED;

        initializeLogger();
    }

    private void initializeLogger()
    {
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mCmLogger = Logger.getLogger(LoggingProxy.JDBC_LOGGER_CM);
            try
            {
                String sJreVersion = RuntimeEnvironmentVariables.getVariable("java.version", JRE_DEFAULT_VERSION);
                // PROJ-2583 JDK1.4������ �������Ͽ��� handlers�� �����ص� �ҷ����� ���ϱ� ������
                // ���α׷������� handler�� add���ش�.
                if (sJreVersion.startsWith(JRE_DEFAULT_VERSION))
                {
                    String sHandlers = getProperty(MultipleFileHandler.CM_HANDLERS, "");
                    if (sHandlers.contains(MultipleFileHandler.HANDLER_NAME))
                    {
                        mCmLogger.addHandler(new MultipleFileHandler());
                    }
                    else if (sHandlers.contains("java.util.logging.FileHandler"))
                    {
                        String sLocalPattern = getProperty("java.util.logging.FileHandler.pattern", "%h/jdbc_net.log");
                        int sLocalLimit = Integer.parseInt(getProperty("java.util.logging.FileHandler.limit", "10000000"));
                        int sLocalCount = Integer.parseInt(getProperty("java.util.logging.FileHandler.count", "1"));
                        boolean sLocalAppend = Boolean.getBoolean(getProperty("java.util.logging.FileHandler.append", "false"));
                        String sLevel = getProperty("java.util.logging.FileHandler.level", "FINEST");

                        FileHandler sFileHandler = new FileHandler(sLocalPattern, sLocalLimit, sLocalCount, sLocalAppend);
                        sFileHandler.setLevel(Level.parse(sLevel));
                        sFileHandler.setFormatter(new XMLFormatter());
                        mCmLogger.addHandler(sFileHandler);
                    }
                }
                mCmLogger.setUseParentHandlers(false);
            }
            catch (IOException sIOE)
            {
                mCmLogger.log(Level.SEVERE, "Cannot add handler", sIOE);
            }
        }
    }

    private String getProperty(String aName, String aDefaultValue)
    {
        String property = LogManager.getLogManager().getProperty(aName);
        return property != null ? property : aDefaultValue;
    }

    private boolean isPreferIPv6()
    {
        return mIsPreferIPv6;
    }

    public void setPreferredIPv6()
    {
        mIsPreferIPv6 = true;
    }

    private boolean isHostname(String aConnAddr)
    {
        return !IP_PATTERN.matcher(aConnAddr).matches();
    }

    /**
     * Hostname�� ���� IP ����� ��´�.
     *
     * @param aHost       IP ����� ���� Hostname
     * @param aPreferIPv6 IP ��Ͽ� IPv6�� ���� ������ ����.
     *                    true�� IPv6 ���� IPv4 �ּҰ�, false�� IPv4 ���� IPv6 �ּ� �´�.
     *
     * @return Hostname�� ���� IP ���
     * 
     * @throws SQLException �� �� ���� Host�̰ų�, IP ��� ��ȸ�� ������ ���� ���
     */
    private static List<InetAddress> getAllIpAddressesByName(String aHost, boolean aPreferIPv6) throws SQLException
    {
        InetAddress[] sAddrs = null;

        ArrayList<InetAddress> sIPv4List = new ArrayList<InetAddress>();
        ArrayList<InetAddress> sIPv6List = new ArrayList<InetAddress>();
        ArrayList<InetAddress> sIpAddrList = new ArrayList<InetAddress>();

        try
        {
            sAddrs = InetAddress.getAllByName(aHost);
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.UNKNOWN_HOST, aHost, sEx);
        }

        for (InetAddress sAddr : sAddrs)
        {
            if (sAddr instanceof Inet4Address)
            {
                sIPv4List.add(sAddr);
            }
            else
            {
                sIPv6List.add(sAddr);
            }
        }

        // To find out in the order of preferred IP address type
        if (aPreferIPv6)
        {
            sIpAddrList.addAll(sIPv6List);
            sIpAddrList.addAll(sIPv4List);
        }
        else
        {
            sIpAddrList.addAll(sIPv4List);
            sIpAddrList.addAll(sIPv6List);
        }

        sIPv6List.clear();
        sIPv4List.clear();

        return sIpAddrList;
    }

    /**
     * ���� ������ �ΰ�, CmChannel�� ����ϱ� ���� �غ� �Ѵ�.
     * 
     * @param aAddr            ������ ������ IP �ּ� �Ǵ� Hostname
     * @param aBindAddr        ���Ͽ� ���ε� �� IP �ּ�. null�̸� ���ε� ���� ����.
     * @param aPort            ������ ������ Port
     * @param aLoginTimeout    Socket.connect()�� ����� Ÿ�Ӿƿ� �� (�� ����)
     *
     * @throws SQLException �̹� ����Ǿ��ְų� ���� ����, �ʱ�ȭ �Ǵ� ���ῡ ������ ���
     */
    public void open(String aAddr, String aBindAddr, int aPort, int aLoginTimeout) throws SQLException
    {
        if (!isClosed())
        {
            Error.throwSQLException(ErrorDef.OPENED_CONNECTION);
        }

        if (isHostname(aAddr))
        {
            this.openWithName(aAddr, aBindAddr, aPort, aLoginTimeout);
        }
        else
        {
            open(new InetSocketAddress(aAddr, aPort), aBindAddr, aLoginTimeout);
        }
    }

    private void openWithName(String aHostname, String aBindAddr, int aPort, int aLoginTimeout)
        throws SQLException
    {
        List<InetAddress> sIpAddrList = getAllIpAddressesByName(aHostname, isPreferIPv6());

        int sIdx;
        InetAddress sAddr;
        InetSocketAddress sSockAddr;
        
        // try to connect using All IPs
        for (sIdx = 0; sIdx < sIpAddrList.size();)
        {
            sAddr = sIpAddrList.get(sIdx);
            sSockAddr = new InetSocketAddress(sAddr, aPort);
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mCmLogger.log(Level.INFO, "openWithName: try to connect to " + sSockAddr);
            }
            try
            {
                open(sSockAddr, aBindAddr, aLoginTimeout);
                break;
            }
            catch (SQLException sEx)
            {
                if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                {
                    mCmLogger.log(Level.SEVERE, "Connection Failed by Hostname: " + sEx.getMessage());
                }
                sIdx++;
                // If all of the attempting failed, throw a exception.
                if (sIdx == sIpAddrList.size())
                {
                    throw sEx;
                }
            }
        }
    }

    /**
     * ���� ������ �ΰ�, CmChannel�� ����ϱ� ���� �غ� �Ѵ�.
     * 
     * @param aSockAddr        ������ ����
     * @param aBindAddr        ���Ͽ� ���ε� �� IP �ּ�. null�̸� ���ε� ���� ����.
     * @param aLoginTimeout    Socket.connect()�� ����� Ÿ�Ӿƿ� �� (�� ����)
     *
     * @throws SQLException �̹� ����Ǿ��ְų� ���� ����, �ʱ�ȭ �Ǵ� ���ῡ ������ ���
     */
    private void open(SocketAddress aSockAddr, String aBindAddr, int aLoginTimeout) throws SQLException
    {
        // BUG-47492 ���� ���ӽÿ��� login_timeout������ response_timeout���� �����Ѵ�.
        mResponseTimeout = aLoginTimeout;
        try
        {
            switch (mConnType)
            {
                case TCP:
                    mSocket = new CmTcpSocket();
                    break;

                case SSL:
                    mSocket = new CmSecureSocket(mProps);
                    break;

                case IB:
                    mSocket = new CmRdmaSocket(mProps);
                    break;

                default:
                    Error.throwSQLException(ErrorDef.UNKNOWN_CONNTYPE,
                                            mConnType.toString() + "(" + Integer.toString(mConnType.getValue()) + ")");
                    break;
            }

            mSocket.setSockRcvBufSize(mSockRcvBufSize);
            mSocket.open(aSockAddr, aBindAddr, aLoginTimeout);

            initToWrite();
        }
        catch (SQLException e)
        {
            quietClose();
            throw e;
        }
        // connect ���н� �� �� �ִ� ���ܰ� �������� �ƴϹǷ� ��� ���ܸ� ��� ���� ���з� ó���Ѵ�.
        // �������, AIX 6.1������ ClosedSelectorException�� ���µ� �̴� RuntimeException�̴�. (ref. BUG-33341)
        catch (Exception e)
        {
            quietClose();
            Error.throwCommunicationErrorException(e);
        }
    }

    public boolean isClosed()
    {
        return (mState == CHANNEL_STATE_CLOSED);
    }

    public void close() throws IOException
    {
        if (mState != CHANNEL_STATE_CLOSED)
        {
            if (mSocket != null)
            {
                mSocket.close();
                mSocket = null;
            }

            mState = CHANNEL_STATE_CLOSED;
        }
    }

    /**
     * ä���� �ݴ´�.
     * �� ��, ���ܰ� �߻��ϴ��� ������ �Ѿ��.
     */
    public void quietClose()
    {
        try
        {
            close();
        }
        catch (IOException ex)
        {
            // quite
        }
    }

    public void setCharset(Charset aCharset, Charset aNCharset)
    {
        super.setCharset(aCharset, aNCharset); // BUG-45156 encoder�� ���� �κ��� CmBufferWrite���� ó��

        mDBDecoder = aCharset.newDecoder();
        mNCharDecoder = aNCharset.newDecoder();

        mDBDecoder.onMalformedInput(CodingErrorAction.REPORT);
        mDBDecoder.onUnmappableCharacter(CodingErrorAction.REPORT);
        mNCharDecoder.onMalformedInput(CodingErrorAction.REPORT);
        mNCharDecoder.onUnmappableCharacter(CodingErrorAction.REPORT);

        CLOB_BUF_SIZE = LobConst.LOB_BUFFER_SIZE  / getByteLengthPerChar();

        // PROJ-2427 getBytes�� ����� Encoder�� ColumnFactory�� ����
        mColumnFactory.setCharSetEncoder(mDBEncoder);
        mColumnFactory.setNCharSetEncoder(mNCharEncoder);

        // ListBufferHandle �� Encoder �� ������ charset name ���� �����;��Ѵ�.
        mTempListBufferHandle.setCharset(getCharset(), getNCharset());
    }
    
    public Charset getCharset()
    {
        return mDBEncoder.charset();
    }
    
    public Charset getNCharset()
    {
        return mNCharEncoder.charset();
    }
    
    public int getByteLengthPerChar()
    {
        return (int)Math.ceil(mDBEncoder.maxBytesPerChar());
    }
    
    public void writeOp(byte aOpCode) throws SQLException
    {
        checkWritable(1);
        mBuffer.put(aOpCode);
    }

    public void writeBytes(byte[] aValue, int aOffset, int aLength) throws SQLException
    {
        checkWritable(aLength);
        mBuffer.put(aValue, aOffset, aLength);
    }
    
    int writeBytes4Clob(ReadableCharChannel aChannelFromSource, long aLocatorId, boolean isCopyMode) throws SQLException
    {
        throwErrorForClosed();

        int sByteReadSize = -1;
        try
        {
            checkWritable4Lob(LobConst.LOB_BUFFER_SIZE);

            int sOrgPos4Op = mBuffer.position();
            mBuffer.position(sOrgPos4Op + LobConst.FIXED_LOB_OP_LENGTH_FOR_PUTLOB);
            sByteReadSize = aChannelFromSource.read(mBuffer);
            if (sByteReadSize == -1)
            {
                mBuffer.position(sOrgPos4Op);
            }
            else
            {
                mBuffer.position(sOrgPos4Op);
                this.writeOp(CmOperationDef.DB_OP_LOB_PUT);
                this.writeLong(aLocatorId);
                this.writeUnsignedInt(sByteReadSize);
                mBuffer.position(mBuffer.position() + sByteReadSize);

                if (isCopyMode)
                {
                    sendPacket(false);
                    initToWrite();

                    Object sSrc = aChannelFromSource.getSource();
                    if (sSrc instanceof ClobReader)
                    {
                        ((ClobReader)sSrc).readyToCopy();
                    }
                }
            }
        }
        catch (IOException sException)
        {
            Error.throwSQLExceptionForIOException(sException.getCause());
        }
        return sByteReadSize;
    }

    public void writeBytes(ByteBuffer aValue) throws SQLException
    {
        throwErrorForClosed();

        while (aValue.remaining() > 0)
        {
            if (aValue.remaining() <= mBuffer.remaining())
            {
                mBuffer.put(aValue);
            }
            else
            {
                int sOrgLimit = aValue.limit();
                aValue.limit(aValue.position() + mBuffer.remaining());
                mBuffer.put(aValue);
                sendPacket(false); // ���� �� = �������� �ƴ�

                aValue.limit(sOrgLimit);
                initToWrite();
            }
        }
    }

    public void sendAndReceive() throws SQLException
    {
        throwErrorForClosed();
        sendPacket(true);
        initToRead();
        receivePacket();
    }
    
    public void send() throws SQLException
    {
        throwErrorForClosed();
        sendPacket(true);
        initToWrite();
    }

    public void receive() throws SQLException
    {
        throwErrorForClosed();
        initToRead();
        receivePacket();
    }

    /**
     * LOB I/O�� ����� �ӽ� ���۸� ��´�.
     * <p>
     * ���ü� ������ �߻��� �� �����Ƿ� �ݵ�� synchronozed ���������� ����ؾ��Ѵ�.
     * 
     * @return LOB I/O�� ����� �ӽ� ����
     */
    byte[] getLobBuffer()
    {
        if (mLobBuf == null)
        {
            mLobBuf = new byte[LobConst.LOB_BUFFER_SIZE];
        }
        return mLobBuf;
    }
    
    int checkWritable4Lob(long aLengthToWrite) throws SQLException
    {
        checkWritable(LobConst.FIXED_LOB_OP_LENGTH_FOR_PUTLOB);

        if (mBuffer.remaining() < (aLengthToWrite + LobConst.FIXED_LOB_OP_LENGTH_FOR_PUTLOB))
        {
            return mBuffer.remaining() - LobConst.FIXED_LOB_OP_LENGTH_FOR_PUTLOB; 
        }
        else
        {
            return (int)aLengthToWrite;
        }
    }
    
    public void checkWritable(int aNeedToWrite) throws SQLException
    {
        throwErrorForClosed();

        if (mState == CHANNEL_STATE_READABLE)
        {
            initToWrite();
        }
        else
        {
            if (mBuffer.remaining() < aNeedToWrite)
            {
                sendPacket(false);
                initToWrite();
            }
        }
    }

    private void writePacketHeader(int aPayloadLength, boolean aIsLast)
    {
        mBuffer.put(CM_PACKET_HEADER_VERSION);
        mBuffer.put(CM_PACKET_HEADER_RESERVED_BYTE);
        mBuffer.putShort((short) aPayloadLength);
        
        if(aIsLast) 
        {
            mBuffer.putInt(setLast(mNextSendSeqNo));
        }
        else
        {
            mBuffer.putInt(mNextSendSeqNo);
        }
        
        mBuffer.putShort(CM_PACKET_HEADER_RESERVED_SHORT);
        mBuffer.putShort(mSessionID);
        mBuffer.putInt(CM_PACKET_HEADER_RESERVED_INT);
    }

    private void initToWrite()
    {
        mBuffer.clear();
        mBuffer.position(CM_PACKET_HEADER_SIZE);
        mState = CHANNEL_STATE_WRITABLE;
    }

    private void sendPacket(boolean aIsLast) throws SQLException
    {
        int sTotalWrittenLength = mBuffer.position();
        mBuffer.flip();
        writePacketHeader(sTotalWrittenLength - CM_PACKET_HEADER_SIZE, aIsLast);
        mBuffer.rewind();
        try
        {
            // PROJ-2583 jdbc packet logging
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED && mCmLogger.isLoggable(Level.FINEST))
            {
                byte[] sByteArry = new byte[mBuffer.remaining()];
                mBuffer.get(sByteArry);
                mBuffer.rewind();
                String sDumpedBytes = DumpByteUtil.dumpBytes(sByteArry, 0, sByteArry.length);
                mCmLogger.log(Level.FINEST, sDumpedBytes, Integer.toHexString(mSessionID & 0xffff));
            }
            while (mBuffer.hasRemaining())
            {
                mSocket.write(mBuffer);
            }
            // BUG-48360 ���� ������ ������ �� ������ ���� ������Ų��.
            mNextSendSeqNo = incAsUnsignedInt(mNextSendSeqNo);
        }
        catch (IOException sException)
        {
            // BUG-48360 ���� ���ܰ� �߻��� ��쿡�� ��Ź����� ���¸� readable�� �ٲ�� �Ѵ�.
            initToRead();
            Error.throwCommunicationErrorException(sException);
        }        
    }

    private void initToRead()
    {
        mBuffer.clear();
        mState = CHANNEL_STATE_READABLE;
    }

    /**
     * ����Ÿ�� aSkipLen ��ŭ �ǳ� �ڴ�.
     * <p>
     * ����, aSkipLen�� 0�̸� ������ �����Ѵ�.
     *
     * @param aSkipLen �ǳ� �� byte ��
     * @throws InternalError aSkipLen�� 0���� ���� ���
     */
    public void skip(int aSkipLen)
    {
        if (aSkipLen < 0)
        {
            Error.throwInternalError(ErrorDef.INVALID_ARGUMENT,
                                     "Skip length",
                                     "0 ~ Integer.MAX_VALUE",
                                     String.valueOf(aSkipLen));
        }

        if (aSkipLen > 0)
        {
            mBuffer.position(mBuffer.position() + aSkipLen);
        }
    }

    private void readyToRead()
    {
        // Expected Size
        int size = mPayloadSize + CM_PACKET_HEADER_SIZE;
        if (mBuffer.limit() != size)
        {
            Error.throwInternalError(ErrorDef.INTERNAL_ASSERTION);
        }

        // Skip Header Size
        mBuffer.position(CM_PACKET_HEADER_SIZE);
    }
    
    private void receivePacket() throws SQLException
    {
        if (mBuffer.remaining() != mBuffer.capacity())
        {
            if (mBuffer.remaining() != 0)
            {
                Error.throwInternalError(ErrorDef.INTERNAL_ASSERTION);
            }
            mBuffer.clear();
        }

        readFromSocket(CM_PACKET_HEADER_SIZE);
        parseHeader();
        readFromSocket(mPayloadSize);

        // PROJ-2583 jdbc packet logging
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED && mCmLogger.isLoggable(Level.FINEST))
        {
            mBuffer.rewind();  // BUG-43694 limit�� ���� �ʰ� �ٷ� rewind�Ѵ�.
            byte[] sByteArry = new byte[mBuffer.remaining()];
            mBuffer.get(sByteArry);
            String sDumpedBytes = DumpByteUtil.dumpBytes(sByteArry, 0, sByteArry.length);
            mCmLogger.log(Level.FINEST, sDumpedBytes, String.valueOf(mSessionID));
        }
        
        readyToRead();
    }

    private void readFromSocket(int size) throws SQLException
    {
        long sLeftPacket, sReadLength;

        if (mBuffer.limit() < mBuffer.capacity())
        {
            mBuffer.limit(mBuffer.limit() + size);
        }
        else
        {
            mBuffer.limit(size);
        }

        for (sLeftPacket = size; sLeftPacket > 0; sLeftPacket -= sReadLength)
        {
            throwErrorForThreadInterrupted();
            sReadLength = readFromSocket();
        }
    }

    private int readFromSocket() throws SQLException
    {
        int sRead = 0;
        
        while (true)
        {
            try
            {
                sRead = mSocket.read(mBuffer);
            }
            catch (SocketTimeoutException sTimeoutException)
            {
                quietClose(); // RESPONSE_TIMEOUT�� ���� ������ ���´�.
                Error.throwSQLException(ErrorDef.RESPONSE_TIMEOUT, String.valueOf(mResponseTimeout), sTimeoutException);
            }
            catch (IOException sException)
            {
                Error.throwCommunicationErrorException(sException);
            }
            
            if (sRead == -1)
            {
                Error.throwSQLException(ErrorDef.COMMUNICATION_ERROR, "There was no response from the server, and the channel has reached end-of-stream.");
            }

            if (sRead > 0)
            {
                break;
            }
        }
        
        return sRead;
    }
    
    public byte readOp() throws SQLException
    {
        if (mBuffer.remaining() == 0)
        {
            if (mIsLastPacket)
            {
                return NO_OP;
            }
            receivePacket();
        }
        return mBuffer.get();
    }

    public byte readByte() throws SQLException
    {
        checkReadable(1);
        return mBuffer.get();
    }

    public short readUnsignedByte() throws SQLException
    {
        checkReadable(1);
        byte sByte = mBuffer.get();
        return (short) (sByte & 0xFF);
    }

    public short readShort() throws SQLException
    {
        checkReadable(2);
        return mBuffer.getShort();
    }

    public int readUnsignedShort() throws SQLException
    {
        checkReadable(2);
        short sShort = mBuffer.getShort();
        return sShort & 0xFFFF;
    }

    public int readInt() throws SQLException
    {
        checkReadable(4);
        return mBuffer.getInt();
    }
    
    public long readUnsignedInt() throws SQLException
    {
        checkReadable(4);        
        return mBuffer.getInt() & 0xFFFFFFFFL;
    }

    public BigInteger readUnsignedLong() throws SQLException
    {
        checkReadable(8);
        byte[] uLongMagnitude = new byte[8];
        mBuffer.get(uLongMagnitude);
        return new BigInteger(1, uLongMagnitude);
    }
    
    public long readLong() throws SQLException
    {
        checkReadable(8);
        return mBuffer.getLong();
    }

    public float readFloat() throws SQLException
    {
        checkReadable(4);
        return mBuffer.getFloat();
    }

    public double readDouble() throws SQLException
    {
        checkReadable(8);
        return mBuffer.getDouble();
    }

    public void readBytes(byte[] aDest) throws SQLException
    {
        readBytes(aDest, aDest.length);
    }
    
    public void readBytes(byte[] aDest, int aOffset, int aLength) throws SQLException
    {
        for (int sBufPos = aOffset, sNeededBytesSize = aLength;;)
        {
            int sReadableBytesSize = mBuffer.remaining();
            if (sNeededBytesSize <= sReadableBytesSize)
            {
                int sOrgLimit = mBuffer.limit();
                mBuffer.limit(mBuffer.position() + sNeededBytesSize);
                mBuffer.get(aDest, sBufPos, sNeededBytesSize);
                mBuffer.limit(sOrgLimit);
                throwErrorForPacketTwisted(sBufPos + sNeededBytesSize - aOffset != aLength);
                break;
            }
            else /* if (sNeededBytesSize > mBuf.remaining()) */
            {
                mBuffer.get(aDest, sBufPos, sReadableBytesSize);
                sBufPos += sReadableBytesSize;
                sNeededBytesSize -= sReadableBytesSize;
                receivePacket();
            }
        }
    }
    
    public void readBytes(byte[] aDest, int aLength) throws SQLException
    {
        readBytes(aDest, 0, aLength);
    }

    public void readBytes(ByteBuffer aBuf) throws SQLException
    {
        for (int sNeededBytesSize = aBuf.remaining();;)
        {
            int sReadableBytesSize = mBuffer.remaining();
            if (sNeededBytesSize <= sReadableBytesSize)
            {
                int sOrgLimit = mBuffer.limit();
                mBuffer.limit(mBuffer.position() + sNeededBytesSize);
                aBuf.put(mBuffer);
                mBuffer.limit(sOrgLimit);
                throwErrorForPacketTwisted(aBuf.remaining() > 0);
                break;
            }
            else /* if (sNeededBytesSize > mBuf.remaining()) */
            {
                aBuf.put(mBuffer);
                sNeededBytesSize -= sReadableBytesSize;
                receivePacket();
            }
        }
    }

    public String readDecodedString(int aByteSize, int aSkipLength) throws SQLException
    {
        StringBuffer sStrBuf = readStringBy(aByteSize, aSkipLength, mDBDecoder);
        return sStrBuf.toString();
    }

    public String readString(int aLength) throws SQLException
    {
        byte[] sBuf = new byte[aLength];
        readBytes(sBuf);

        return new String(sBuf);
    }

    int readCharArrayTo(char[] aDst, int aDstOffset, int aByteLength, int aCharLength) throws SQLException
    {
        StringBuffer sStrBuf = readStringBy(aByteLength, 0, mDBDecoder);
        aCharLength = sStrBuf.length();
        sStrBuf.getChars(0, aCharLength, aDst, aDstOffset);
        return aCharLength;
    }
    
    public String readString(byte[] aBytesValue)
    {
        return decodeString(ByteBuffer.wrap(aBytesValue), mDBDecoder);
    }

    /**
     * Char, Varchar �÷� �����͸� ���� �� Charset�� �°� decode�� ���ڿ��� �����Ѵ�.
     * @param aColumnSize �÷������� ������
     * @param aSkipSize  ��ŵ������
     * @param aIsNationalCharset �ٱ���ĳ���ͼ����� ����
     * @return ���ڵ��� char, varchar�÷� ��Ʈ��
     * @throws SQLException ���������� �÷������͸� �޾ƿ��� ���� ���
     */
    public String readCharVarcharColumnString(int aColumnSize, int aSkipSize,
                                              boolean aIsNationalCharset) throws SQLException
    {
        mCharVarcharColumnBuffer.clear();
        mCharVarcharColumnBuffer.limit(aColumnSize);
        this.readBytes(mCharVarcharColumnBuffer);
        this.skip(aSkipSize);
        mCharVarcharColumnBuffer.flip();

        return  aIsNationalCharset ? decodeString(mCharVarcharColumnBuffer, mNCharDecoder) :
                                     decodeString(mCharVarcharColumnBuffer, mDBDecoder);
    }

    private String decodeString(ByteBuffer aByteBuf, CharsetDecoder aDecoder)
    {
        mCharBuf.clear();
        while (aByteBuf.remaining() > 0)
        {
            CoderResult sResult = aDecoder.decode(aByteBuf, mCharBuf, true);
            if (sResult == CoderResult.OVERFLOW)
            {
                Error.throwInternalError(ErrorDef.INTERNAL_BUFFER_OVERFLOW);
            }
            if (sResult.isError())
            {
                char data = (char)aByteBuf.get();
                if (sResult.isUnmappable())
                {
                    // fix BUG-27782 ��ȯ�� �� ���� ���ڴ� '?'�� ���
                    mCharBuf.put('?');
                }
                else if (sResult.isMalformed())
                {
                    mCharBuf.put(data);
                }
            }
        }
        mCharBuf.flip();
        return mCharBuf.toString();
    }

    public String readStringForErrorResult() throws SQLException
    {
        int sSize = readUnsignedShort();
        byte[] sBuf = new byte[sSize];
        readBytes(sBuf);
        return new String(sBuf);
    }

    /**
     * DB Message ���������� ����� �о� �޽��� ó�� �ݹ鿡 �����Ѵ�.
     * @throws SQLException message�� �о���� ���� ������ �߻����� ��
     */
    void readAndPrintServerMessage() throws SQLException
    {
        long sSize = readUnsignedInt();
        if (mMessageCallback == null)
        {
            // BUG-45237 �ݹ��� ������ �޽����� ���� �ʰ� �׳� skip�Ѵ�.
            skip((int)sSize);
        }
        else
        {
            byte[] sBuf = new byte[(int) sSize];
            readBytes(sBuf);

            long sRestLength = sSize - Integer.MAX_VALUE;
            while (sRestLength > 0)
            {
                if (sRestLength >= Integer.MAX_VALUE)
                {
                    skip(Integer.MAX_VALUE);
                    sRestLength -= Integer.MAX_VALUE;
                }
                else
                {
                    skip((int) sRestLength);
                    sRestLength = 0;
                }
            }

            // BUG-45237 DB Message ���������� ó���� �ݹ��� ������ �ݹ��� ȣ���Ѵ�.
            mMessageCallback.print(new String(sBuf));
        }
    }

    String readStringForColumnInfo() throws SQLException
    {
        int sStrLen = (int)readByte();
        if (sStrLen == 0)
        {
            return null;
        }

        byte[] sBuf = new byte[sStrLen];
        readBytes(sBuf);
        return new String(sBuf);
    }

    private void checkReadable(int aLengthToRead) throws SQLException
    {
        if (mState != CHANNEL_STATE_READABLE)
        {
            Error.throwInternalError(ErrorDef.INVALID_STATE, "CHANNEL_STATE_READABLE", CHANNEL_STATE_STRING[mState]);
        }

        if (mBuffer.remaining() < aLengthToRead)
        {
            if (mIsLastPacket) 
            {
                Error.throwInternalError(ErrorDef.MORE_DATA_NEEDED);
            }

            if (mPrimitiveTypeBuf == null)
            {
                mPrimitiveTypeBuf = ByteBuffer.allocateDirect(MAX_PRIMITIVE_TYPE_SIZE);
            }

            int sRemain = mBuffer.remaining();

            if (sRemain > MAX_PRIMITIVE_TYPE_SIZE)
            {
                Error.throwInternalError(ErrorDef.EXCEED_PRIMITIVE_DATA_SIZE);
            }

            if (sRemain > 0)
            {
                mPrimitiveTypeBuf.clear();
                mPrimitiveTypeBuf.put(mBuffer);
                mPrimitiveTypeBuf.flip();
            }

            receivePacket();

            if (sRemain > 0)
            {
                mBuffer.position(mBuffer.position() - sRemain);
                int sPos = mBuffer.position();
                mBuffer.put(mPrimitiveTypeBuf);
                mBuffer.position(sPos);
            }
        }
    }

    private int parseHeader() throws SQLException
    {
        int sOrgPos = mBuffer.position();
        mBuffer.flip();

        byte sByte = mBuffer.get();
        if (sByte != CM_PACKET_HEADER_VERSION)
        {
            Error.throwInternalError(ErrorDef.INVALID_PACKET_HEADER_VERSION,
                                     String.valueOf(CM_PACKET_HEADER_VERSION),
                                     String.valueOf(sByte));
        }

        mBuffer.get();  // Reserved 1
        mPayloadSize = mBuffer.getShort();        
        int sSequenceNo = mBuffer.getInt();               
        mBuffer.getShort(); // Reserved 2

        mIsLastPacket = !isFragmented(sSequenceNo);

        mSessionID = mBuffer.getShort();
        mBuffer.getInt(); // Reserved 3
        
        mBuffer.position(sOrgPos);
        
        checkValidSeqNo(removeIntegerMSB(sSequenceNo));
        
        return mPayloadSize;
    }

    private StringBuffer readStringBy(int aByteSize, int aSkipLength, CharsetDecoder aDecoder) throws SQLException
    {
        StringBuffer sStrBuf = new StringBuffer();
        int sNeededBytesSize = aByteSize;
        int sReadableBytesSize;
        while (sNeededBytesSize > (sReadableBytesSize = mBuffer.remaining()))
        {
            mCharBuf.clear();
            CoderResult sCoderResult = aDecoder.decode(mBuffer, mCharBuf, true);
            if (sCoderResult == CoderResult.OVERFLOW)
            {
                Error.throwInternalError(ErrorDef.INTERNAL_BUFFER_OVERFLOW);
            }
            mCharBuf.flip();
            sStrBuf.append(mCharBuf.array(), mCharBuf.arrayOffset(), mCharBuf.limit());
            if (sCoderResult.isError()) // BUG-27782
            {
                char data = (char)mBuffer.get();
                if (sCoderResult.isUnmappable())
                {
                    sStrBuf.append("?");
                }
                else if (sCoderResult.isMalformed())
                {
                    sStrBuf.append(data);
                }
            }

            // ���ڰ� ©�� encode�� ���� ���ۿ� ���ܵ� ����Ÿ�� ���� �� �����Ƿ�
            // ���� ���̸� ���� �ٽ� ó���� ���̿� ������� �Ѵ�.
            int sRemainBytesSize = mBuffer.remaining();
            sNeededBytesSize -= sReadableBytesSize;
            checkReadable(sNeededBytesSize);
            sNeededBytesSize += sRemainBytesSize;
        }
        if (sNeededBytesSize > 0)
        {
            int sOrgLimit = mBuffer.limit();
            mBuffer.limit(mBuffer.position() + sNeededBytesSize);
            CoderResult sCoderResult;
            do
            {
                mCharBuf.clear();
                sCoderResult = aDecoder.decode(mBuffer, mCharBuf, true);
                mCharBuf.flip();
                sStrBuf.append(mCharBuf.array(), mCharBuf.arrayOffset(), mCharBuf.limit());
                if (sCoderResult.isError())
                {
                    // �ش� byte �鿡 ���� mapping �� �� �ִ� char �� ���� ��� :
                    // ByteBuffer �� �Һ� �����־�� ���� ���ڿ� ���� ó���� �����ϹǷ�,
                    // �ϴ� get���� �����͸� ���� ��, �̸� ������ string buffer�� ����.
                    // ���� encoding table ���� �ٸ� ���̹Ƿ� ���� ���ڷ� ǥ����.
                    // ('?' �� ǥ���Ǹ�, �ڵ尪�� 63)
                    char data = (char)mBuffer.get();
                    if (sCoderResult.isUnmappable())
                    {
                        // fix BUG-27782 ��ȯ�� �� ���� ���ڴ� '?'�� ����Ѵ�.
                        sStrBuf.append("?");
                    }
                    else if (sCoderResult.isMalformed())
                    {
                        sStrBuf.append(data);
                    }
                }
            } while (sCoderResult != CoderResult.OVERFLOW && sCoderResult != CoderResult.UNDERFLOW);

            if (sCoderResult == CoderResult.OVERFLOW) 
            {
                Error.throwInternalError(ErrorDef.INTERNAL_BUFFER_OVERFLOW);
            }
            aSkipLength += mBuffer.remaining();
            mBuffer.limit(sOrgLimit);
            if (aSkipLength > 0)
            {
                mBuffer.position(mBuffer.position() + aSkipLength);
            }
        }
        return sStrBuf;
    }

    private void checkValidSeqNo(int aNumber) 
    {
        if (mPrevReceivedSeqNo + 1 == aNumber)
        {
            mPrevReceivedSeqNo = aNumber;
        }
        else
        {
            Error.throwInternalError(ErrorDef.INVALID_PACKET_SEQUENCE_NUMBER, String.valueOf(mPrevReceivedSeqNo + 1), String.valueOf(aNumber));
        }
    }
    
    private static int incAsUnsignedInt(int aNumber)
    {
        if (aNumber == 0x7FFFFFFF)
        {
            return 0;
        }

        return ++aNumber;
    }
    
    private static int removeIntegerMSB(int aNumber) 
    {
        return aNumber & 0x7FFFFFFF;
    }
    
    private static boolean isFragmented(int aSequenceNo)
    {
        return (aSequenceNo & 0x80000000) == 0;
    }

    private static int setLast(int aSequenceNo)
    {
        return aSequenceNo | 0x80000000;
    }

    public int getLobCacheThreshold()
    {
        return mLobCacheThreshold;
    }

    public void setLobCacheThreshold(int aLobCacheThreshold)
    {
        this.mLobCacheThreshold = aLobCacheThreshold;
    }

    ReadableCharChannel getReadChannel4Clob(Reader aSource) throws IOException
    {
        if(mReadChannel4Clob == null)
        {
            mReadChannel4Clob = new AltibaseReadableCharChannel();
        }
        
        mReadChannel4Clob.open(aSource);
        return mReadChannel4Clob;
    }
    
    ReadableCharChannel getReadChannel4Clob(Reader aSource, int aSourceLength) throws IOException
    {
        if(mReadChannel4Clob == null)
        {
            mReadChannel4Clob = new AltibaseReadableCharChannel();
        }
        
        mReadChannel4Clob.open(aSource, aSourceLength);
        return mReadChannel4Clob;
    }

    ReadableCharChannel getReadChannel4Clob(char[] aSource, int aOffset, int aLength) throws IOException
    {
        if(mReadChannel4Clob == null)
        {
            mReadChannel4Clob = new AltibaseReadableCharChannel();
        }
        
        mReadChannel4Clob.open(aSource, aOffset, aLength);
        return mReadChannel4Clob;
    }

    public void setRemoveRedundantMode(boolean aMode, AltibaseProperties aProps)
    {
        this.mRemoveRedundantMode = aMode;

        mColumnFactory.setProperties(aProps);
    }
    
    public ColumnFactory getColumnFactory()
    {
        return mColumnFactory;
    }
    
    private void throwErrorForClosed() throws SQLException
    {
        if (isClosed())
        {
            Error.throwSQLException(ErrorDef.CLOSED_CONNECTION);
        }
    }
    
    private void throwErrorForThreadInterrupted() throws SQLException
    {
        if (Thread.interrupted())
        {
            Error.throwSQLException(ErrorDef.THREAD_INTERRUPTED);
        }
    }
    
    private void throwErrorForPacketTwisted(boolean aTest) throws SQLException
    {
        if (aTest)
        {
            Error.throwSQLException(ErrorDef.PACKET_TWISTED);
        }
    }

    public CmConnType getConnType()
    {
        return mConnType;
    }

    public void setConnType(String aConnType) throws SQLException
    {
        CmConnType sConnType = CmConnType.toConnType(aConnType);

        if (sConnType == CmConnType.None)
        {
            Error.throwSQLException(ErrorDef.UNKNOWN_CONNTYPE, aConnType);
        }

        mConnType = sConnType;
    }

    /**
     * PROJ-2681 ���� Ÿ�Կ� ���� ������Ƽ ���� �б� ���� �����Ѵ�. 
     * (SSL : PROJ-2474 SSL ���� ������ ������ �о�� mSslProps ��ü�� �����Ѵ�.)
     * @param aProps  AltibaseProperties ��ü
     */
    public void setProps(AltibaseProperties aProps)
    {
        this.mProps = aProps;
    }

    /**
     * ���� SSLSocket���� ������ CipherSuites ����Ʈ�� �����ش�.
     * @return CipherSuite ����Ʈ
     */
    public String[] getCipherSuitList()
    {
        if (mConnType != CmConnType.SSL)
            return null;

        return ((CmSecureSocket)mSocket).getEnabledCipherSuites();
    }

    /**
     * Socket receive buffer ũ�⸦ ��ȯ�Ѵ�.
     */
    public int getSockRcvBufSize()
    {
        return mSockRcvBufSize;
    }

    /**
     * Socket receive buffer ũ�⸦ �����Ѵ�.
     */
    public void setSockRcvBufSize(int aSockRcvBufSize) throws IOException
    {
        int sSockRcvBufSize = aSockRcvBufSize;

        if (!isClosed())
        {
            mSocket.setSockRcvBufSize(sSockRcvBufSize);
        }

        mSockRcvBufSize = sSockRcvBufSize;
    }

    /**
     * Socket receive buffer ũ�⸦ CM block ������ �����Ѵ�.
     */
    public void setSockRcvBufBlockRatio(int aSockRcvBufBlockRatio) throws IOException
    {
        int sSockRcvBufSize;
        
        if (aSockRcvBufBlockRatio <= 0)
        {
            sSockRcvBufSize = CM_DEFAULT_RCVBUF_SIZE;
        }
        else
        {
            sSockRcvBufSize = aSockRcvBufBlockRatio * CM_BLOCK_SIZE;
        }

        if (!isClosed())
        {
            mSocket.setSockRcvBufSize(sSockRcvBufSize);
        }

        mSockRcvBufSize = sSockRcvBufSize;
    }

    /**
     * �񵿱������� fetch ���������� �۽��� protocol context �� �����Ѵ�.
     */
    public void setAsyncContext(CmProtocolContext aContext)
    {
        mAsyncContext = aContext;
    }

    /**
     * �񵿱������� fetch ���������� �۽��� protocol context �� ��´�.
     */
    public boolean isAsyncSent()
    {
        return (mAsyncContext != null);
    }

    /**
     * �񵿱������� fetch ���������� �۽��� protocol context �� ��´�.
     */
    public CmProtocolContext getAsyncContext()
    {
        return mAsyncContext;
    }

    /**
     * Socket �� file descriptor �� ��ȯ�Ѵ�.
     */
    public int getSocketFD() throws SQLException
    {
        throwErrorForClosed();

        if (mSocket != null)
        {
            return mSocket.getSocketFD();
        }

        return CmSocket.INVALID_SOCKTFD;
    }

    /**
     * AltibaseConnection���� ���� Message Callback ��ü�� ���Թ޴´�.
     * @param aCallback �ݹ鰴ü
     */
    public void setMessageCallback(AltibaseMessageCallback aCallback)
    {
        this.mMessageCallback = aCallback;
    }

    /**
     * Message Callback ��ü�� ��ȯ�Ѵ�.
     */
    public AltibaseMessageCallback getMessageCallback()  /* BUG-46019 */
    {
        return this.mMessageCallback;
    }

    public ListBufferHandle getTempListBufferHandle()
    {
        return mTempListBufferHandle;
    }

    /**
     * CLOB�� ��� LOB_CACHE_THRESHOLD�� CM_BLOCK_SIZE�� �ʰ��� ���
     * ���ڵ��� ���� mCharBuf�� ���Ҵ��Ѵ�.
     * @param aLobDataLength LOB�����ͱ���
     */
    public void checkDecodingBuffer(int aLobDataLength) throws SQLException
    {
        if (aLobDataLength > mCharBuf.capacity())  /* BUG-46411 */
        {
            mCharBuf = CharBuffer.allocate(AltibaseProperties.MAX_LOB_CACHE_THRESHOLD);
        }
    }

    public void setResponseTimeout(int aResponseTimeout) throws SQLException
    {
        mSocket.setResponseTimeout(aResponseTimeout);
        mResponseTimeout = aResponseTimeout;
    }

    public void setSocket(CmSocket aSocket)
    {
        // BUG-48360 �׽�Ʈ�� ���� setter�޼ҵ� �߰�
        mSocket = aSocket;
    }

    public int getResponseTimeout()
    {
        return mResponseTimeout;
    }
}
