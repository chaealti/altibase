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
 * $Id$
 *
 * Description :
 *
 * �� ������ DRDB �������������� Corrupt page �����ڿ� ���� ��� �����̴�.
 *
 * DRDB�� ���� restart recovery ������ corrupted page�� �߰� �Ͽ��� ��
 * �׿� ���� ó���� ���� class�̴�.
 *
 * restart redo ���� �� corrupt page�� �߰��ϸ� corruptedPagesHash��
 * ��� �ξ��ٰ� undo������ ��� ��ģ �� hash �� ���Ե� page�� ���Ͽ�
 * page�� ���� extent�� free�������� Ȯ���Ѵ�.
 * free�̸� �����ϰ� alloc�� �����̸� ������ �����Ų��.
 *
 * ���� ���� ���δ� CORRUPT_PAGE_ERR_POLICY ������Ƽ�� ���Ҽ� �ִ�
 *
 * 0 - restart recovery ���� �߿� corrupt page�� �߰� �ߴ���
 *     ������ �����Ű�� �ʴ´�. (default)
 * 1 - restart recovery ���� �߿� corrupt page�� �߰��� ���
 *     corrupt page�� �߰��� ��� ������ �����Ѵ�.
 *
 * restart redo���� �߿� corrupt page�� �߰��� ��쿡�� �ش�Ǹ�
 * restart undo���� �߿� corrupt page�� �߰��Ͽ��ų�,
 * GG, LG Hdr page�� corrupt�� �߰� �� ���
 * ������Ƽ�� ��� ���� �ٷ� ����ȴ�.
 *
 **********************************************************************/
#ifndef _O_SDR_CORRUPT_PAGE_MGR_H_
#define _O_SDR_CORRUPT_PAGE_MGR_H_ 1

#include <sdrDef.h>

class sdrCorruptPageMgr
{
public:

    static IDE_RC initialize( UInt  aHashTableSize );

    static IDE_RC destroy();

    // Corrupt Page�� mCorruptedPages �� ���
    static IDE_RC addCorruptPage( scSpaceID aSpaceID,
                                  scPageID  aPageID );

    // Corrupt Page�� mCorruptedPages ���� ����
    static IDE_RC delCorruptPage( scSpaceID aSpaceID,
                                  scPageID  aPageID );
    // Corrupted Page ���� �˻�
    static IDE_RC checkCorruptedPages();

    static idBool isOverwriteLog( sdrLogType aLogType );

    static void allowReadCorruptPage();

    static void fatalReadCorruptPage();

    /* BUG-45598: CORRUPT_PAGE_ERR_POLICY ������Ƽ�� ���ǵ� ���� ���� 
     * Erroró�� ��å ����.
     */ 
    static void setPageErrPolicyByProp();

    static inline sdbCorruptPageReadPolicy getCorruptPageReadPolicy();

private:
    // �ؽ� key ���� �Լ�
    static UInt genHashValueFunc( void* aGRID );

    // �ؽ� �� �Լ�
    static SInt compareFunc( void* aLhs, void* aRhs );

private:
    // corrupted page�� ����� Hash
    static smuHashBase   mCorruptedPages;

    // �ؽ� ũ��
    static UInt          mHashTableSize;

    // corrupt page�� �о��� ���� ��å (fatal,abort,pass)
    static sdbCorruptPageReadPolicy mCorruptPageReadPolicy;
};

/****************************************************************
 * Description: corrupt page �� �о��� �� ��ó�� ���� ��å�� �����Ѵ�.
 ****************************************************************/
inline sdbCorruptPageReadPolicy sdrCorruptPageMgr::getCorruptPageReadPolicy()
{
    return mCorruptPageReadPolicy;
}


#endif // _O_SDR_CORRUPT_PAGE_MGR_H_
