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
 * $Id: sddTableSpace.h 86125 2019-09-05 02:22:00Z et16 $
 *
 * Description :
 *
 * �� ������ ��ũ�������� tablespace ��忡 ���� ��������̴�.
 *
 * # ����
 *
 * ��Ƽ���̽�4 ���ķ� ��뷮 ��ũ�� ���������ν� �ϳ� �Ǵ� �ϳ� �̻���
 * �������� ����Ÿ������ �������� �����ϴ� ���� level�� �ڷᱸ��
 *
 * # Ÿ�԰� Ư¡
 *
 * 1) �ý��� tablespace (tablespace ID 0 ���� 3����)
 *   �ý��� ���̺����̽��� createdb�ÿ� �ڵ����� �����Ǹ�, ��� user��
 *   �⺻���� tablespace�� �����ȴ�. ����, �ý��� ������ �ʿ��� ������
 *   ���ԵǾ� �ִ�.
 *
 * - system memory tablespace
 *   ���� �޸� ��� ���̺� ���� ���̺����̽�
 *   �ý��� ���������� tablespace�� ������ �ϰ����� ���� �����ϸ�,
 *   ���ٸ� �ǹ̴� ������ �ʴ´�.
 *
 * - system data tablespace
 *   �ý����� �⺻ �����͸� �����ϱ� ���� �����̴�.
 *
 * - system undo tablespace
 *   undo ���׸�Ʈ�� TSS(transaction status slot)�� �����ϱ� ���� �����̴�.
 *
 * - system temporary tablespace
 *   ����Ÿ���̽��� ���� ������ ó���ϱ� ���� �ӽð����̴�.
 *
 * 2) ��ý��� tablespace (tablespace ID 4 ����)
 *   runtime�� DDL�� ���� ���������ϴ�.
 *
 * - user data tablespace
 *   ������� ����Ÿ�� �����ϱ� ���� ����
 *
 * - user temporary tablespace
 *   ����ڸ� ���� �ӽð����̸�, ����ڸ��� �ϳ����� ���� ����
 *
 *
 *
 * # ����
 *
 *   - sddTableSpaceNode
 *               ____________
 *               |*_________|   ____ ____
 *               |          |---|@_|-|@_|- ... : sddDatabaseFileNode list
 *               |          |
 *        ____   |          |   ____
 *   prev |*_|...|          |...|*_| next      : sddTableSpaceNode list
 *               |__________|
 *
 * !!] Ư¡
 * - ���̺����̽��� ��� ������ ������
 * - ����Ÿ���� ������ ���� list�� ������
 *
 *
 * # ���� �ڷᱸ��
 *
 *   sddTableSpaceNode ����ü
 *   sddDataFileNode   ����ü
 *
 *
 * # ����
 *
 * - 4.1.1.1 ���̺����̽� UI.doc
 * - 4.2.1.8 (SDD-SDB) Disk Manager �� Buffer Manager �������.doc
 *
 **********************************************************************/

#ifndef _O_SDD_TABLESPACE_H_
#define _O_SDD_TABLESPACE_H_ 1

#include <smDef.h>
#include <sddDef.h>

class sddTableSpace
{
public:

    /* tablespace ��� �ʱ�ȭ */
    static IDE_RC initialize( sddTableSpaceNode*  aSpaceNode,
                              smiTableSpaceAttr*  aSpaceAttr );

    /* tablespace ��� ���� */
    static IDE_RC destroy( sddTableSpaceNode* aSpaceNode );

    /* tablespace ����� datafile ��� �� datafile ���� ���ʻ��� */
    static IDE_RC createDataFiles( idvSQL             * aStatistics,
                                   void               * aTrans,
                                   sddTableSpaceNode  * aSpaceNode,
                                   smiDataFileAttr   ** aFileAttr,
                                   UInt                 aAttrCnt,
                                   smiTouchMode         aTouchMode,
                                   UInt                 aMaxDataFileSize );

    /* PROJ-1923 ALTIBASE HDB Disaster Recovery
     * tablespace ����� datafile ��� �� datafile �� ���� */
    static IDE_RC createDataFile4Redo( idvSQL             * aStatistics,
                                       void               * aTrans,
                                       sddTableSpaceNode  * aSpaceNode,
                                       smiDataFileAttr    * aFileAttr,
                                       smLSN                aCurLSN,
                                       smiTouchMode         aTouchMode,
                                       UInt                 aMaxDataFileSize );

    /* �ϳ��� datafile ��� �� datafile�� ���� */
    static IDE_RC removeDataFile( idvSQL*             aStatistics,
                                  void*               aTrans,
                                  sddTableSpaceNode*  aSpaceNode,
                                  sddDataFileNode*    aFileNode,
                                  smiTouchMode        aTouchMode,
                                  idBool              aDoGhostMark );

    /* �ش� tablespace ����� ��� datafile ��� �� datafile�� ���� */
    static IDE_RC removeAllDataFiles( idvSQL*             aStatistics,
                                      void*               aTrans,
                                      sddTableSpaceNode*  aSpaceNode,
                                      smiTouchMode        aTouchMode,
                                      idBool              aDoGhostMark);

    /* datafile ����� remove ���ɿ��� Ȯ�� */
    static IDE_RC canRemoveDataFileNodeByName(
                     sddTableSpaceNode* aSpaceNode,
                     SChar*             aDataFileName,
                     scPageID           aUsedPageLimit,
                     sddDataFileNode**  aFileNode );

    /* ������ ID�� ��ȿ�Ҷ� DBF Node�� ��ȯ */
    static IDE_RC getDataFileNodeByPageID( sddTableSpaceNode* aSpaceNode,
                                           scPageID           aPageID,
                                           sddDataFileNode**  aFileNode,
                                           idBool             aFatal = ID_TRUE );

    /* ������ ID�� ��ȿ�Ҷ� DBF Node�� ��ȯ */
    static void getDataFileNodeByPageIDWithoutException(
                                           sddTableSpaceNode* aSpaceNode,
                                           scPageID           aPageID,
                                           sddDataFileNode**  aFileNode );


    /* �ش� ���ϸ��� ������ datafile ��� ��ȯ */
    static IDE_RC getDataFileNodeByName( sddTableSpaceNode*   aSpaceNode,
                                         SChar*               aFileName,
                                         sddDataFileNode**    aFileNode );

    /*PRJ-1671 ���� ���ϳ�带 ����*/
    static void getNextFileNode(sddTableSpaceNode* aSpaceNode,
                                sdFileID           aCurFileID,
                                sddDataFileNode**  aFileNode);


    /* autoextend ��带 ������ datafile ��带 �˻� */
    static IDE_RC getDataFileNodeByAutoExtendMode(
                     sddTableSpaceNode* aSpaceNode,
                     SChar*             aDataFileName,
                     idBool             aAutoExtendMode,
                     scPageID           aUsedPageLimit,
                     sddDataFileNode**  aFileNode );

    /* DBF ID�� FileNode ��ȯ ���ٸ� Exception �߻� */
    static IDE_RC getDataFileNodeByID(
                     sddTableSpaceNode*  aSpaceNode,
                     UInt                aFileID,
                     sddDataFileNode**   aFileNode);

    /* ����Ÿ������ �������� ������ Null�� ��ȯ�Ѵ� */
    static void getDataFileNodeByIDWithoutException(
                     sddTableSpaceNode*  aSpaceNode,
                     UInt                aFileID,
                     sddDataFileNode**   aFileNode);

    /* datafile ����� page ������ ��ȯ */
    static IDE_RC getPageRangeByName( sddTableSpaceNode* aSpaceNode,
                                      SChar*             aDataFileName,
                                      sddDataFileNode**  aFileNode,
                                      scPageID*          aFstPageID,
                                      scPageID*          aLstPageID );
    // ����Ÿ���ϳ�带 �˻��Ͽ� ������ ������ ��´�.
    static IDE_RC getPageRangeInFileByID( sddTableSpaceNode * sSpaceNode,
                                          UInt                aFileID,
                                          scPageID          * aFstPageID,
                                          scPageID          * aLstPageID );

    /* ���̺����̽���  �� ������ ���� ��ȯ */
    static ULong getTotalPageCount( sddTableSpaceNode* aSpaceNode );

    /* ���̺����̽���  �� DBF ���� ��ȯ */
    static UInt  getTotalFileCount( sddTableSpaceNode* aSpaceNode );

    /* datafile ���� ����Ʈ�� ��� �߰� */
    static void addDataFileNode( sddTableSpaceNode*  aSpaceNode,
                                 sddDataFileNode*    aFileNode );

    static void removeMarkDataFileNode( sddDataFileNode * aFileNode );

    static void getTableSpaceAttr( sddTableSpaceNode* aTableSpaceNode,
                                   smiTableSpaceAttr* aTableSpaceAttr);

    static inline UInt getTBSAttrFlag( sddTableSpaceNode  * aSpaceNode );
    static inline void setTBSAttrFlag( sddTableSpaceNode  * aSpaceNode,
                                       UInt                 aAttrFlag );

    static sdFileID getNewFileID( sddTableSpaceNode* aTableSpaceNode );

    static void setOnlineTBSLSN4Idx ( sddTableSpaceNode* aSpaceNode,
                                      smLSN *            aOnlineTBSLSN4Idx )
    { SM_GET_LSN( aSpaceNode->mOnlineTBSLSN4Idx, *aOnlineTBSLSN4Idx ); }

    static smLSN getOnlineTBSLSN4Idx ( void * aSpaceNode )
    { return ((sddTableSpaceNode*)aSpaceNode)->mOnlineTBSLSN4Idx; }

    /* tablespace ����� ������ ��� */
    static IDE_RC dumpTableSpaceNode( sddTableSpaceNode* aSpaceNode );

    /* tablespace ����� datafile ��� ����Ʈ�� ��� */
    static void dumpDataFileList( sddTableSpaceNode* aSpaceNode );

    // ���������� �������Ŀ� ���̺����̽���
    // DataFileCount�� TotalPageCount�� ����Ͽ� �����Ѵ�.
    static void calculateFileSizeOfTBS( sddTableSpaceNode * aSpaceNode );
    
    // ���̺����̽� ����� ó���Ѵ�.
    static IDE_RC doActOnlineBackup( idvSQL            * aStatistics,
                                     sctTableSpaceNode * aSpaceNode,
                                     void              * aActionArg );

    // ���̺����̽��� Dirty�� ����Ÿ������ Sync �Ѵ�.
    static IDE_RC doActSyncTBSInNormal( idvSQL            * aStatistics,
                                        sctTableSpaceNode * aSpaceNode,
                                        void              * aActionArg );

};


/*
   ��� : tablespace �Ӽ����� �ּҸ� ��ȯ�Ѵ�.
          tablespace �Ӽ����� �����ϰ��� �Ҷ� ����Ѵ�.
*/
UInt sddTableSpace::getTBSAttrFlag( sddTableSpaceNode  * aSpaceNode )
{
    IDE_DASSERT( aSpaceNode != NULL );

    return aSpaceNode->mAttrFlag;
}

void sddTableSpace::setTBSAttrFlag( sddTableSpaceNode  * aSpaceNode,
                                    UInt                 aAttrFlag )
{
    IDE_DASSERT( aSpaceNode != NULL );

    aSpaceNode->mAttrFlag = aAttrFlag;
}

#endif // _O_SDD_TABLESPACE_H_

