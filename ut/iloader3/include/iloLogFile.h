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
 * $Id: iloLogFile.h 88494 2020-09-04 04:29:31Z chkim $
 **********************************************************************/

#ifndef _O_ILO_LOGFILE_H
#define _O_ILO_LOGFILE_H

class iloLogFile
{
public:
    iloLogFile();

    void SetTerminator(SChar *szFiledTerm, SChar *szRowTerm);

    /* BUG-47652 Set file permission */
    SInt OpenFile( SChar *szFileName, idBool aIsExistFilePerm );

    SInt CloseFile();

    void PrintLogMsg(const SChar *szMsg);
    // BUG-21640 iloader���� �����޽����� �˾ƺ��� ���ϰ� ����ϱ�
    // ���� �����޽����� ������ �������� ����ϴ� �Լ��߰�
    void PrintLogErr(uteErrorMgr *aMgr);

    void PrintTime(const SChar *szPrnStr);

    SInt PrintOneRecord( ALTIBASE_ILOADER_HANDLE  aHandle,
                         iloTableInfo            *pTableInfo,
                         SInt                     aReadRecCount,
                         SInt                     aArrayCount);
    /* TASK-2657 */
    void setIsCSV ( SInt aIsCSV );

private:
    FILE      *m_LogFp;
    SInt       m_UseLogFile;
    SChar      m_FieldTerm[11];
    SChar      m_RowTerm[11];
    /* TASK-2657 */
    SInt      mIsCSV;
};

#endif /* _O_ILO_LOGFILE_H */
