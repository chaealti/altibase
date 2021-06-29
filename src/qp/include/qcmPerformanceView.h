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
 * $Id: qcmPerformanceView.h 83394 2018-07-02 03:28:18Z hykim $
 *
 * Description :
 *
 *     FT ���̺�鿡 ���Ͽ�
 *     CREATE VIEW V$VIEW AS SELECT * FROM X$TABLE;
 *     �� ���� view�� �����Ͽ� �Ϲ� ����ڿ��Դ�  �� view�� ���� ���길��
 *     �����ϵ��� �Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QCM_PERFORMANCEVIEW_H_
#define _O_QCM_PERFORMANCEVIEW_H_ 1

#include    <smiDef.h>
#include    <qc.h>
#include    <qtc.h>
#include    <qdbCommon.h>
#include    <qdbCreate.h>
#include    <qdv.h>
#include    <qcmTableInfo.h>

extern SChar * gQcmPerformanceViews[];

// PROJ-1726
// qcmPerformanceViewManager
// �뵵 : �������ȭ�� ����Ǹ鼭 ������ ���� ����� ��������
// performance view�� ����ϴ� ���(qcm/qcmPerformanceView.cpp
// �� gQcmPerformanceViews �迭�� �߰�) �ܿ� ���� ��� ������
// ��Ÿ�� �� �߰��Ǵ� performance view �� �����Ѵ�.
// �� �ΰ��� ����� performance view �� ó���ϱ� ����,
// ������ gQcmPerformanceViews �迭�� ���� ������ wrapper class.

// �������ȭ�� ���� ���� ����� ��� performance view��
// ������Ĵ�� gQcmPerformanceViews �� �߰��ϸ�,
// �������ȭ�� ����� ����� ��� <���>i.h �� ��ũ�η� ���� ��
// <���>im.h :: initSystemTables ���� 
// qciMisc::addPerformanceViews(aQueryStr) �� �̿�, ����Ѵ�.
// ��� ���� rpi.h, rpim.cpp ���� �����Ѵ�.
class qcmPerformanceViewManager
{
public:
    static IDE_RC   initialize();
    static IDE_RC   finalize();

    // �� performance view �� ���� ��´�.
    static SInt     getTotalViewCount( qcmPVType aType )
    {
        SInt sCount = 0;

        switch( aType )
        {
            case QCM_PV_TYPE_NORMAL:
                sCount = mNumOfPreViews + mNumOfAddedViews;
                break;
            case QCM_PV_TYPE_SHARD:
                sCount = mShardViewCount;
                break;
            default:
                IDE_DASSERT(0);
                break;
        }

        return sCount;
    }

    // �������� performance view �� �߰�
    static IDE_RC   add(SChar* aViewStr);

    // performance view �� ����. ������ gQcmPerformanceViews��
    // �迭 ���� ��İ� ���� �������̽� ����.
    static SChar *  get( int aIdx, qcmPVType aType );

private:
    static SChar *  getFromAddedViews(int aIdx);

    static SChar ** mPreViews;
    static SInt     mNumOfPreViews;

    static SInt     mNumOfAddedViews;
    static iduList  mAddedViewList;

    static SChar ** mShardViews;
    static SInt     mShardViewCount;
};

class qcmPerformanceView
{

private:

    static IDE_RC runDDLforPV( idvSQL * aStatistics, qcmPVType aType );

    static IDE_RC executeDDL( qcStatement * aStatement,
                              SChar       * aText );

    static IDE_RC registerOnIduFixedTableDesc( qdTableParseTree * aParseTree );

public:

    static IDE_RC makeParseTreeForViewInSelect(
            qcStatement   * aStatement,
            qmsTableRef   * aTableRef );


    static IDE_RC registerPerformanceView( idvSQL * aStatistics, qcmPVType aType );

    // parse
    static IDE_RC parseCreate( qcStatement * aStatement );

    // validate
    static IDE_RC validateCreate( qcStatement * aStatement );

    // execute
    static IDE_RC executeCreate( qcStatement * aStatement );

    // null buildRecord function
    static IDE_RC nullBuildRecord( idvSQL *,
                                   void * ,
                                   void * ,
                                   iduFixedTableMemory *aMemory);

};

#endif /* _O_QCM_PERFORMANCEVIEW_H_ */
