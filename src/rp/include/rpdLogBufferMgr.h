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
 

/***********************************************************************
 * $Id: rpdLogBufferMgr.h 22507 2007-07-10 01:29:09Z lswhh $
 **********************************************************************/

#ifndef _O_RPD_LOGBUFFERMGR_H_
#define _O_RPD_LOGBUFFERMGR_H_

#include <rp.h>
#include <smiLogRec.h>

/*
 * Write pointer: Service�� �α׸� ������ �� ������ ����
 * Writable Flag(Copy Mode Flag): Service�� �α� ���ۿ� �����ϴ� ���� ������ �� OFF/
 *                  Service�� Accessing Info CountȮ��(Sender�� ��� �α׸� �� �о��) �� ON/
 *                  Sender�� ���� �������� Ȯ���� �� ����/ �� ���� �αװ� ���� �� Ȯ��
 * Accessing Info Count : Sender�� ������ ���� ���ö� ����/
 * Min Read Pointer:Service�� �α׸� �����Ҷ� �ʿ��� ��� ����/
 *                  Sender�� �����Ҷ� ���� / Sender�� �а� �ִ� ������ �ٷ� ���� ����Ű�� ����
 * Min SN,  Max SN: Service�� ��� �α� ó���� ����
 */
#define RP_BUF_NULL_PTR     (ID_UINT_MAX)
#define RP_BUF_NULL_ID      (ID_UINT_MAX)

typedef enum
{
    RP_ON = 0,
    RP_OFF
} flagSwitch;


typedef struct AccessingInfo
{
    UInt    mReadPtr;
    UInt    mNextPtr;
} AccessingInfo;
/***************************************************************************************
 *   RP LOG Buffer�� service thread�� �α׸� write�ϰ� Sender�� �о�� ������ �Ǿ�����,
 *   ȯ������ �����Ǳ� ������ Sender�� �д� ��ġ�� Service Thread�� ���� ��ġ�� ���󰬴���, 
 *   Service Thread�� ���� ��ġ�� Sender�� �д� ��ġ�� ���󰬴��� Ȯ���� �� �־�� �ϸ�, 
 *   �̰��� �����ϱ� ���� Service Thread�� ���⸦ �Ϸ��� ���� ��ġ(mWrotePtr)�� Sender�� 
 *   �б⸦ �Ϸ��� ���� ��ġ(mMinReadPtr)�� ����Ѵ�. Sender�� Service Thread�� ���⸦ �Ϸ��� 
 *   ��ġ�� mWritePtr���� mMinReadPtr�� �̵��� �� ������, �� ���� ���� ���  
 *   Service Thread�� �� ���� Sender�� ��� �о��ٴ� ���̵ȴ�.
 *   �׸��� Service Thread�� mMinReadPtr�� ����Ű�� ���� ���⸦ �� �� ������ ��ȣ�ϱ� ������ 
 *   Service Thread�� Sender�� �д� ���� ������ ���󰣴ٰ� �ϴ��� �� ���� ���� �� �� ����.
 ***************************************************************************************/
class rpdLogBufferMgr
{
public:
    IDE_RC initialize(UInt aSize, UInt aMaxSenderCnt);

    void   destroy();
    void   finalize();

    SChar*        mBasePtr;
    UInt          mBufSize;
    UInt          mMaxSenderCnt;
    UInt          mEndPtr;

    smiDummyLog   mBufferEndLog;
    UInt          mBufferEndLogSize;


    /*Buffer Info*/
    /* mMinReadPtr: ������ �ּ� ��ġ (Sender�� ���� ������ �ּ� ��ġ�� ������ �ּ� ��ġ�� ����)
     * Sender�� ���� �� ����ϰ�, Service�� overwrite���� �ʱ� ���ػ��*/
    UInt           mMinReadPtr;
    /*Service Thread�� ���⸦ �Ϸ��� ������ ��ġ*/
    UInt           mWrotePtr;
    /*Service Thread�� �������� ��� �� ������ ��ġ*/
    UInt           mWritePtr;

    /* Service Thread�� ���ۿ� �۾��� �ؾ��ϴ� �� ���θ� ��Ÿ��
     * Sender�� Service Thread�� ���ۿ� �۾��� �ϰ� �ִ��� ���θ� Ȯ���ϴ� �� ���
     */
    flagSwitch     mWritableFlag;

    iduMutex       mBufInfoMutex;
    /*������ �ּ� SN�� �ִ� SN*/
    smSN           mMinSN;
    smSN           mMaxSN;
    smLSN          mMaxLSN;
    /*���ۿ��� �α׸� �а� �ִ� Sender�鿡 ���� ����*/
    UInt           mAccessInfoCnt;
    AccessingInfo* mAccessInfoList;

    /*aCopyFlag�� TRUE�� �� Log�� Buffer�� �����Ѵ�, �׷��� ������ Control������ �����Ѵ�*/
    IDE_RC copyToRPLogBuf(idvSQL * aStatistics,
                          UInt     aSize, 
                          SChar  * aLogPtr, 
                          smLSN    aLSN);



    /*Log�� Buffer���� ������ �о���ϴ� �α��� �����͸� �Ѱ��ش�.*/
    IDE_RC readFromRPLogBuf(UInt       aSndrID,
                            smiLogHdr* aLogHead,
                            void**     aLogPtr,
                            smSN*      aSN,
                            smLSN*     aLSN,
                            idBool*    aIsLeave);

    /*buffer���� ���� �������� �׽�Ʈ �ϰ� ��带 ������ �� ID�� ��ȯ�Ѵ�. by sender*/
    idBool tryEnter(smSN    aNeedSN,
                    smSN*   aMinSN,
                    smSN*   aMaxSN,
                    UInt*   aSndrID );

    void getSN(smSN* aMinSN, smSN* aMaxSN);

    void leave(UInt aSndrID);

private:
    void updateMinReadPtr();

    IDE_RC writeBufferEndLogToRPBuf();
    void printBufferInfo();
    inline idBool isBufferEndLog( smiLogHdr * aLogHdr )
    {
        return smiLogRec::isDummyLog( aLogHdr );
    }

    inline UInt getWritePtr(UInt aWrotePtr)
    {
        return ((aWrotePtr + 1) % mBufSize);
    }
    inline UInt getReadPtr(UInt aReadPtr)
    {
        return ((aReadPtr + 1) % mBufSize);
    }
};

#endif
