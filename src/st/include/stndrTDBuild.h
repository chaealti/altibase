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
 * $Id: stndrTDBuild.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 * Buffer Flush ������
 *
 * # Concept
 *
 * - ���� �Ŵ����� dirty page list�� �ֱ�������
 *   flush �ϴ� ������
 *
 * # Architecture
 * 
 * - ���� startup�� �ϳ��� buffer flush ������ ����
 * 
 **********************************************************************/

#ifndef _O_STNDR_TDBUILD_H_
#define _O_STNDR_TDBUILD_H_ 1

#include <idu.h>
#include <idtBaseThread.h>
#include <idv.h>
#include <smcDef.h>
#include <smnDef.h>

class stndrTDBuild : public idtBaseThread
{
public:

    /* ������ �ʱ�ȭ */
    IDE_RC initialize( UInt             aTotalThreadCnt,
                       UInt             aID,
                       smcTableHeader * aTable,
                       smnIndexHeader * aIndex,
                       smSCN            aTxBeginSCN,
                       idBool           aIsNeedValidation,
                       UInt             aBuildFlag,
                       idvSQL         * aStatistics,
                       idBool         * aContinue );
    
    IDE_RC destroy();         /* ������ ���� */
    
    virtual void run();       /* main ���� ��ƾ */

    stndrTDBuild();
    virtual ~stndrTDBuild();

    inline UInt getErrorCode() { return mErrorCode; };
    inline ideErrorMgr* getErrorMgr() { return &mErrorMgr; };

private:

    idBool            mFinished;    /* ������ ���� ���� flag */
    idBool          * mContinue;    /* ������ �ߴ� ���� flag */
    UInt              mErrorCode;
    ideErrorMgr       mErrorMgr;
    
    UInt              mTotalThreadCnt;
    UInt              mID;
    void            * mTrans;
    smcTableHeader  * mTable;
    smnIndexHeader  * mIndex;
    idBool            mIsNeedValidation;
    UInt              mBuildFlag;
    idvSQL          * mStatistics;
};

#endif // _O_STNDR_TDBUILD_H_





