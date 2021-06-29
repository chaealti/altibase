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
 * $Id: qdtCommon.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/
#ifndef  _O_QDT_COMMON_H_
#define  _O_QDT_COMMON_H_  1

#include <iduMemory.h>
#include <qc.h>
#include <qdParseTree.h>

typedef struct qdtTBSAttrValueMap 
{
    SChar * mValueString;
    UInt    mValue;
} qcpTBSAttrValueMap ;

#define QCP_TBS_ATTR_MAX_VALUE_MAP_COUNT (2)

typedef struct qdtTBSAttrMap 
{
    SChar * mName;
    UInt    mMask;
    qcpTBSAttrValueMap mValueMap[QCP_TBS_ATTR_MAX_VALUE_MAP_COUNT+1];
} qdtTBSAttrMap;


class qdtCommon
{
public:
    static IDE_RC validateFilesSpec( qcStatement       * aStatement,
                                     smiTableSpaceType   aType,
                                     qdTBSFilesSpec    * aFilesSpec,
                                     ULong               aExtentSize,
                                     UInt              * aDataFileCount);

    // To Fix PR-10437
    // INDEX TABLESPACE tbs_name �� ���� Validation
    static IDE_RC getAndValidateIndexTBS( qcStatement       * aStatement,
                                          scSpaceID           aTableTBSID,
                                          smiTableSpaceType   aTableTBSType,
                                          qcNamePosition      aIndexTBSName,
                                          UInt                aIndexOwnerID,
                                          scSpaceID         * aIndexTBSID,
                                          smiTableSpaceType * aIndexTBSType );
    
    static IDE_RC getAndValidateTBSOfIndexPartition( 
                                          qcStatement       * aStatement,
                                          scSpaceID           aTableTBSID,
                                          smiTableSpaceType   aTableTBSType,
                                          qcNamePosition      aIndexTBSName,
                                          UInt                aIndexOwnerID,
                                          scSpaceID         * aIndexTBSID,
                                          smiTableSpaceType * aIndexTBSType );

    // ���� ���� Attribute Flag List�� Flag����
    // Bitwise Or���� �Ͽ� �ϳ��� UInt ���� Flag ���� �����
    static IDE_RC getTBSAttrFlagFromList(qdTBSAttrFlagList * aAttrFlagList,
                                         UInt              * aAttrFlag );

    // Tablespace�� Attribute Flag List�� ���� Validation����
    static IDE_RC validateTBSAttrFlagList(qcStatement       * aStatement,
                                          qdTBSAttrFlagList * aAttrFlagList);

    // BUG-40728 ���� tbs type���� �˻��Ѵ�.
    static idBool isSameTBSType( smiTableSpaceType  aTBSType1,
                                 smiTableSpaceType  aTBSType2 );
    
private:
    // Tablespace�� Attribute Flag List���� ������
    // Attribute List�� ������ ��� ����ó��
    static IDE_RC checkTBSAttrIsUnique(qcStatement       * aStatement,
                                       qdTBSAttrFlagList * aAttrFlagList);
    
};

#endif  //  _O_QDT_COMMON_H_
