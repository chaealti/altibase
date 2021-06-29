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
 * $Id: smrRedoLSNMgr.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_SMR_REDO_LSN_MGR_H_
#define _O_SMR_REDO_LSN_MGR_H_ 1

#include <smDef.h>
#include <smrLogFile.h>
#include <iduMemoryHandle.h>

/* Parallel Logging : �������� Redo��
   smrRedoLSNMgr���� �����Ѵ�. smrRedoLSNMgr�� smrRedoInfo��
   mSN���� Sorting�ϰ� Redo�� ��������
   mSN���� ������ smrRedoInfo�� mRedoLSN�� ����Ű�� Log����
   Redo�� �Ѵ�.*/
typedef struct smrRedoInfo
{
    /* Redo�� Log LSN */
    smLSN       mRedoLSN;
    /* mRedoLSN�� ����Ű�� Log�� LogHead */
    smrLogHead  mLogHead;
    /* mRedoLSN�� ����Ű�� Log�� LogBuffer Ptr */
    SChar*      mLogPtr;
    /* mRedoLSN�� ����Ű�� Log�� �ִ� logfile Ptr */
    smrLogFile* mLogFilePtr;
    /* mRedoLSN�� ����Ű�� Log�� Valid�ϸ� ID_TRUE, �ƴϸ� ID_FALSE */
    idBool      mIsValid;

    /* �α� ������������ �ڵ�*/
    iduMemoryHandle * mDecompBufferHandle;

    /* �α����Ϸ� ���� ���� �α��� ũ��
       ����� �α��� ��� �α��� ũ��� �α����ϻ��� �α�ũ�Ⱑ �ٸ���
     */
    UInt         mLogSizeAtDisk;
} smrRedoInfo;

class smrRedoLSNMgr
{
public:
    static IDE_RC initialize( smLSN *aRedoLSN );
    static IDE_RC destroy();


    // Decompress Log Buffer ũ�⸦ ���´�
    static ULong getDecompBufferSize();

    // Decompress Log Buffer�� �Ҵ��� ��� �޸𸮸� �����Ѵ�.
    static IDE_RC clearDecompBuffer();

   /*
    mSortRedoInfo�� ����ִ� smrRedoInfo�߿��� ���� ���� mSN����
    ���� Log�� �о���δ�.
   */
    static IDE_RC readLog(smLSN         ** aLSN,
                          smrLogHead    ** aLogHead,
                          SChar         ** aLogPtr,
                          UInt           * aLogSizeAtDisk,
                          idBool         * aIsValid);

    /* Check�� �Ǵ� Check�� log�� LSN�� ����*/
    /* ȣ�� ������ ���� �޶��� */
    static inline smLSN getLstCheckLogLSN()
    {
        return mRedoInfo.mRedoLSN;
    }

    /* ������ Redo �α��� LSN���� ����*/
    static inline smLSN getNextLogLSNOfLstRedoLog()
    {
        return mCurRedoInfoPtr->mRedoLSN;
    }

    /* smrRedoInfo�� Invalid�ϰ� �����.*/
    static void setRedoLSNToBeInvalid();

    smrRedoLSNMgr();
    virtual ~smrRedoLSNMgr();

private:
    static SInt   compare(const void *arg1,const void *arg2);

    // Redo Info�� �ʱ�ȭ�Ѵ�
    static IDE_RC initializeRedoInfo( smrRedoInfo * aRedoInfo );

    // Redo Info�� �ı��Ѵ�
    static IDE_RC destroyRedoInfo( smrRedoInfo * aRedoInfo );


    // Redo Info�� Sort Array�� Push�Ѵ�.
    static IDE_RC pushRedoInfo( smrRedoInfo * aRedoInfo,
                                smLSN *aRedoLSN );


    // �޸� �ڵ�κ��� �Ҵ��� �޸𸮿� �α׸� �����Ѵ�.
    static IDE_RC makeCopyOfDiskLog( iduMemoryHandle * aMemoryHandle,
                                     SChar *      aOrgLogPtr,
                                     UInt         aOrgLogSize,
                                     SChar**      aCopiedLogPtr );

    static IDE_RC makeCopyOfDiskLogIfNonComp(
                          smrRedoInfo     * aCurRedoInfoPtr,
                          iduMemoryHandle * aDecompBufferHandle,
                          ULong             aOrgDecompBufferSize );
private:
    /* Redo������ ������ �ִ�. */
    static smrRedoInfo   mRedoInfo;
    /* ���� Redo�� �������� smrRedoInfo */
    static smrRedoInfo * mCurRedoInfoPtr;
    /* ������ Redo�� ������ Redo Log�� mLSN */
    static smLSN         mLstLSN;

};

/* smrRedoInfo�� Invalid�ϰ� �����.*/
inline void smrRedoLSNMgr::setRedoLSNToBeInvalid()
{
    mRedoInfo.mIsValid = ID_FALSE;
}

#endif /* _O_SMR_REDO_LSN_MGR_H_ */
