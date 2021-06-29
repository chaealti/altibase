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
 * $Id: qcmJob.cpp 82251 2018-02-13 00:15:24Z donovan.seo $
 **********************************************************************/

#include <qc.h>
#include <qcmJob.h>
#include <qcg.h>
#include <qdcJob.h>
#include <mtc.h>
#include <qtcDef.h>

const void * gQcmJobs;
const void * gQcmJobsIndex[QCM_MAX_META_INDICES];

#define QCM_JOB_DATE_FORMAT      "YYYY/MM/DD HH24:MI:SS"
#define QCM_JOB_DATE_FORMAT_LEN  (21)
#define QCM_JOB_UPDATE_QUERY_LEN (1024)

/**
 * isReadyJob
 *
 * Description
 *
 *    ���� �ð��� JOB �� ������ �ð������� �Ǵ��Ѵ�.
 *    YEAR, �� MONTH �� ����ϴ� ��İ� DAY, HOUR, MINUTE�� ��� ����� �ٸ���.
 *
 *    YEAR �� MONTH �� �����̶����, �޸��� ��¥�� �ٸ��Ƿ� ������ YEAR��, MONTH�� ��������
 *    12���� Interval��ŭ ���� �ش� �ð��� �����غ���.
 *    �� Month�� �������� ���� �� �ּ� 1�⿡ �ѹ��� Job�� ���۵����� ������ ���� ������ ���̴�.
 *
 *    DAY, HOUR, MINUTE �� ���� �ð����� ó�� �ð��� �� �Ŀ� Interval ��ŭ�� Mod������
 *    �����ϴ� ������� �����ð��� ������ �ð������� �Ǵ��ϴ�.
 */
IDE_RC qcmJob::isReadyJob( mtdDateType * aStart,
                           mtdDateType * aCurrent,
                           mtdDateType * aLastExec,
                           SInt          aInterval,
                           UChar       * aIntervalType,
                           idBool      * aResult )
{
    idBool             sResult   = ID_FALSE;
    SLong              sTimeDiff;
    SLong              sMod;
    mtdDateType        sNextDate;
    mtdDateType        sLastDate;
    SInt               sNextInterval;
    SInt               sCompare;
    SInt               i;
    qdcJobIntervalType sIntervalType;

    switch ( *aIntervalType )
    {
        case 'Y':
            sIntervalType = QDC_JOB_INTERVAL_TYPE_YEAR;
            break;
        case 'M':
            if ( *( aIntervalType + 1 ) == 'M' )
            {
                sIntervalType = QDC_JOB_INTERVAL_TYPE_MONTH;
            }
            else
            {
                sIntervalType = QDC_JOB_INTERVAL_TYPE_MINUTE;
            }
            break;
        case 'D':
            sIntervalType = QDC_JOB_INTERVAL_TYPE_DAY;
            break;
        case 'H':
            sIntervalType = QDC_JOB_INTERVAL_TYPE_HOUR;
            break;
        default:
            sIntervalType = QDC_JOB_INTERVAL_TYPE_NONE;
            break;
    }

    if ( ( sIntervalType == QDC_JOB_INTERVAL_TYPE_YEAR ) ||
         ( sIntervalType == QDC_JOB_INTERVAL_TYPE_MONTH ) )
    {
        sLastDate = *aStart;

        /* LAST DATE�� ������ ��� YEAR�� MONTH �� LAST DATE �� �����Ѵ�. */
        if ( MTD_DATE_IS_NULL( aLastExec ) != ID_TRUE )
        {
            sLastDate.year = aLastExec->year;
            sLastDate.mon_day_hour &= ~MTD_DATE_MON_MASK;
            sLastDate.mon_day_hour |= ( aLastExec->mon_day_hour & MTD_DATE_MON_MASK );
        }
        else
        {
            /* Nothing to do */
        }

        if ( sIntervalType == QDC_JOB_INTERVAL_TYPE_YEAR )
        {
            sNextInterval = aInterval * 12;
        }
        else
        {
            sNextInterval = aInterval;
        }

        /* ������ ���� ���ڿ��� 12 ���� interval ŭ �ش� ���ڿ� ���� ���� ���غ���. */
        for ( i = 1; i <= 12; i++ )
        {

            IDE_TEST( mtdDateInterface::addMonth( &sNextDate,
                                                  &sLastDate,
                                                  sNextInterval * i )
                      != IDE_SUCCESS );
            sCompare = mtdDateInterface::compare( aCurrent, &sNextDate );
            if ( sCompare == 0 )
            {
                sResult = ID_TRUE;
                break;
            }
            else if ( sCompare < 0 )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        IDE_TEST( mtdDateInterface::dateDiff( &sTimeDiff,
                                              aStart,
                                              aCurrent,
                                              MTD_DATE_DIFF_MINUTE )
                  != IDE_SUCCESS );

        if ( sIntervalType == QDC_JOB_INTERVAL_TYPE_DAY )
        {
            sMod = aInterval * 1440;
        }
        else if ( sIntervalType == QDC_JOB_INTERVAL_TYPE_HOUR )
        {
            sMod = aInterval * 60;
        }
        else
        {
            sMod = aInterval;
        }

        if ( ( sTimeDiff % sMod == 0 ) )
        {
            sResult = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    *aResult = sResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * getExecuteJobItems
 *
 * ���� �ð��� ������ Job Item �� �����Ѵ�.
 *
 *  aItems    - JOB ID�� �迭
 *  aCount    - JOB ID ��
 *  aMaxCount - �ѹ��� ������ �� �ִ� Max Count
 */
IDE_RC qcmJob::getExecuteJobItems( smiStatement * aSmiStmt,
                                   SInt         * aItems,
                                   UInt         * aCount,
                                   UInt           aMaxCount )
{
    idBool            sIsCursorOpen = ID_FALSE;
    idBool            sIsReady;
    const void      * sRow          = NULL;
    scGRID            sRid;
    mtcColumn       * sJobIDColumn;
    mtcColumn       * sStartColumn;
    mtcColumn       * sEndColumn;
    mtcColumn       * sLastExecColumn;
    mtcColumn       * sStateColumn;
    mtcColumn       * sIntervalColumn;
    mtcColumn       * sIntervalTypeColumn;
    mtcColumn       * sIsEnableColumn;
    SLong             sCount    = 0;

    mtdDateType          sStart;
    mtdDateType          sEnd;
    mtdDateType          sCurrent;
    mtdDateType        * sDate;
    mtdDateType        * sLastExec;
    mtdIntegerType       sID;
    mtdIntegerType       sState;
    mtdIntegerType       sInterval;
    mtdCharType        * sIntervalType;
    mtdCharType        * sIsEnable;
    PDL_Time_Value       sSleepTime;

    smiTableCursor       sCursor;
    smiCursorProperties  sCursorProperty;
    UInt                 sMask;

    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_ID_COL_ORDER,
                                  (const smiColumn**)&sJobIDColumn )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_START_COL_ORDER,
                                  (const smiColumn**)&sStartColumn )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_END_COL_ORDER,
                                  (const smiColumn**)&sEndColumn )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_STATE_COL_ORDER,
                                  (const smiColumn**)&sStateColumn )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_INTERVAL_COL_ORDER,
                                  (const smiColumn**)&sIntervalColumn )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_INTERVALTYPE_COL_ORDER,
                                  (const smiColumn**)&sIntervalTypeColumn )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_LASTEXEC_COL_ORDER,
                                  (const smiColumn**)&sLastExecColumn )
              != IDE_SUCCESS );

    // BUG-41317 each job enable disable
    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_IS_ENABLE_COL_ORDER,
                                  (const smiColumn**)&sIsEnableColumn )
              != IDE_SUCCESS );

    sCursor.initialize();

    SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN( &sCursorProperty, NULL );

    IDE_TEST( sCursor.open(
                  aSmiStmt,
                  gQcmJobs,
                  NULL,
                  smiGetRowSCN( gQcmJobs ),
                  NULL,
                  smiGetDefaultKeyRange(),
                  smiGetDefaultKeyRange(),
                  smiGetDefaultFilter(),
                  QCM_META_CURSOR_FLAG,
                  SMI_SELECT_CURSOR,
                  &sCursorProperty )
              != IDE_SUCCESS );

    sIsCursorOpen = ID_TRUE;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );

    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
              != IDE_SUCCESS );

    sCurrent.year = 0;
    sCurrent.mon_day_hour = 0;
    sCurrent.min_sec_mic = 0;

    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
    {
        IDE_TEST( qtc::sysdate( &sCurrent ) != IDE_SUCCESS );
    }
    else
    {
        if ( QCU_SYSDATE_FOR_NATC[0] != '\0' )
        {
            IDE_TEST( mtdDateInterface::toDate( &sCurrent,
                                                (UChar*)QCU_SYSDATE_FOR_NATC,
                                                idlOS::strlen(QCU_SYSDATE_FOR_NATC),
                                                (UChar*)QCM_JOB_DATE_FORMAT,
                                                QCM_JOB_DATE_FORMAT_LEN )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qtc::sysdate( &sCurrent ) != IDE_SUCCESS );
        }
    }

    sMask = MTD_DATE_SEC_MASK | MTD_DATE_MSEC_MASK;
    sCurrent.min_sec_mic &= ~sMask;

    while ( sRow != NULL )
    {
        sID       = *(mtdIntegerType*)((UChar *)sRow + sJobIDColumn->column.offset );
        sStart    = *(mtdDateType*)((UChar *)sRow + sStartColumn->column.offset );
        sDate     = (mtdDateType*)((UChar *)sRow + sEndColumn->column.offset );
        sState    = *(mtdIntegerType*)((UChar *)sRow + sStateColumn->column.offset );
        sInterval = *(mtdIntegerType*)((UChar *)sRow + sIntervalColumn->column.offset );
        sIntervalType = (mtdCharType*)((UChar *)sRow +
                        sIntervalTypeColumn->column.offset );
        sLastExec = (mtdDateType*)((UChar *)sRow + sLastExecColumn->column.offset );
        sIsEnable  = (mtdCharType *)((UChar *)sRow + sIsEnableColumn->column.offset );
        sStart.min_sec_mic &= ~sMask;

        /* Test�� Timing���� ���� ������ �� �����Ƿ� 10ms�Ŀ� �ٽ� �д´�. */
        while ( mtdDateInterface::compare( sLastExec, &sCurrent ) == 0 )
        {
            sSleepTime.set( 0, 10000 );
            idlOS::sleep(sSleepTime);
            if ( QCU_SYSDATE_FOR_NATC[0] != '\0' )
            {
                IDE_TEST( mtdDateInterface::toDate( &sCurrent,
                                                    (UChar*)QCU_SYSDATE_FOR_NATC,
                                                    idlOS::strlen(QCU_SYSDATE_FOR_NATC),
                                                    (UChar*)QCM_JOB_DATE_FORMAT,
                                                    QCM_JOB_DATE_FORMAT_LEN )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( qtc::sysdate( &sCurrent ) != IDE_SUCCESS );
            }
            sCurrent.min_sec_mic &= ~sMask;
        }

        /* ���� �ð��� Start �ð����� �ڶ�� ���� Row�� �д´�. */
        // BUG-41317 IS_ENABLE column�� FALSE ��� �������� �ʴ´�.
        if ( ( sState == QCM_JOB_STATE_EXEC ) ||
             ( mtdDateInterface::compare( &sStart, &sCurrent ) > 0 ) ||
             ( sIsEnable->value[0] == 'F' ) )
        {
            IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                      != IDE_SUCCESS );
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        if ( MTD_DATE_IS_NULL( sDate ) != ID_TRUE )
        {
            sEnd              = *(mtdDateType*)((UChar *)sRow + sEndColumn->column.offset );
            sEnd.min_sec_mic &= ~sMask;

            /* End Date �� �����Ǿ��ִٸ� ���� �ð��� End Date ���Ķ�� ���� Row�� �д´�. */
            if ( mtdDateInterface::compare( &sCurrent, &sEnd ) > 0 )
            {
                IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                          != IDE_SUCCESS );
                continue;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        /* ���� �ð��� Start �ð��� ���ٸ� Job�� �����Ѵ�. */
        if ( mtdDateInterface::compare( &sCurrent, &sStart ) == 0 )
        {
            aItems[sCount] = sID;
            sCount++;
        }
        else
        {
            sIsReady = ID_FALSE;

            if ( ( sIntervalType->length == 2 ) &&
                 ( sInterval > 0 ) )
            {
                IDE_TEST( isReadyJob( &sStart, &sCurrent, sLastExec, sInterval,
                                      sIntervalType->value, &sIsReady )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            if ( sIsReady == ID_TRUE )
            {
                aItems[sCount] = sID;
                sCount++;
            }
            else
            {
                /* Nothing to do */
            }
        }

        if ( aMaxCount <= sCount )
        {
            *aCount = sCount;
            IDE_RAISE( BUFFER_OVERFLOW );
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                  != IDE_SUCCESS );
    }

    *aCount = sCount;

    sIsCursorOpen = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( BUFFER_OVERFLOW )
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_JOB_QUEUE_OVERFLOW));
    }

    IDE_EXCEPTION_END;

    if ( sIsCursorOpen == ID_TRUE )
    {
        (void)sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

/**
 * getJobInfo
 *
 *  JobID �� �̿��ؼ� Meta���� Job �� ������ �����Ѵ�.
 *
 *  aJobID     - Job ID
 *  aJobName   - Job Name
 *  aExecTxt   - ������ Procecure ����
 *  aExecCount - ���� Ƚ��
 *  aState     - Job State ����
 */
IDE_RC qcmJob::getJobInfo( smiStatement * aSmiStmt,
                           SInt           aJobID,
                           SChar        * aJobName,
                           SChar        * aExecQuery,
                           UInt         * aExecQueryLen,
                           SInt         * aExecCount,
                           qcmJobState  * aState )
{
    idBool                sIsCursorOpen = ID_FALSE;
    const void          * sRow          = NULL;
    scGRID                sRid;
    mtcColumn           * sExecColumn;
    mtcColumn           * sStateColumn;
    mtcColumn           * sExecCountColumn;
    mtcColumn           * sJobNameColumn;
    mtdCharType         * sExec;
    mtdCharType         * sJobName;
    mtdIntegerType        sState = 0;
    mtdIntegerType        sExecCount = -1;
    smiRange              sRange;
    qtcMetaRangeColumn    sFirstMetaRange;
    smiTableCursor        sCursor;
    smiCursorProperties   sCursorProperty;
    mtcColumn            * sFirstMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_ID_COL_ORDER,
                                  (const smiColumn**)&sFirstMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn( &sFirstMetaRange,
                                    sFirstMtcColumn,
                                    (const void *)&aJobID,
                                    &sRange );

    sCursor.initialize();

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sCursorProperty, NULL );

    IDE_TEST( sCursor.open(
                  aSmiStmt,
                  gQcmJobs,
                  gQcmJobsIndex[QCM_JOBS_ID_IDX_ORDER],
                  smiGetRowSCN( gQcmJobs ),
                  NULL,
                  &sRange,
                  smiGetDefaultKeyRange(),
                  smiGetDefaultFilter(),
                  QCM_META_CURSOR_FLAG,
                  SMI_SELECT_CURSOR,
                  &sCursorProperty )
              != IDE_SUCCESS );

    sIsCursorOpen = ID_TRUE;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );

    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_NAME_COL_ORDER,
                                  (const smiColumn**)&sJobNameColumn )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_EXECQUERY_COL_ORDER,
                                  (const smiColumn**)&sExecColumn )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_STATE_COL_ORDER,
                                  (const smiColumn**)&sStateColumn )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_EXECCOUNT_COL_ORDER,
                                  (const smiColumn**)&sExecCountColumn )
              != IDE_SUCCESS );

    if ( sRow != NULL )
    {
        sJobName   = (mtdCharType*)((UChar *)sRow + sJobNameColumn->column.offset );
        sExec      = (mtdCharType*)(mtc::value( sExecColumn, sRow, MTD_OFFSET_USE ));
        sState     = *(mtdIntegerType*)((UChar *)sRow + sStateColumn->column.offset );
        sExecCount = *(mtdIntegerType*)((UChar *)sRow +
                      sExecCountColumn->column.offset );

        aExecQuery[0] = 'E';
        aExecQuery[1] = 'X';
        aExecQuery[2] = 'E';
        aExecQuery[3] = 'C';
        aExecQuery[4] = ' ';

        idlOS::strncpy( aExecQuery + 5, (const SChar *)sExec->value, sExec->length );
        aExecQuery[sExec->length + 5] = '\0';
        *aExecQueryLen = sExec->length + 5;
        
        idlOS::strncpy( aJobName, (const SChar *)sJobName->value, sJobName->length );
        aJobName[sJobName->length] = '\0';
    }
    else
    {
        *aExecQueryLen = 0;
    }

    *aState     = (qcmJobState)sState;
    *aExecCount = sExecCount;

    sIsCursorOpen = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsCursorOpen == ID_TRUE )
    {
        (void)sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

/**
 * updateStateAndLastExecTime
 *
 *   Job �� �����ϱ� ���� Job State�� �����ϰ� ������ ���� �ð��� �����Ѵ�.
 */
IDE_RC qcmJob::updateStateAndLastExecTime( smiStatement * aSmiStmt,
                                           SInt           aJobID )
{
    SChar           sSqlStr[QCM_JOB_UPDATE_QUERY_LEN];
    vSLong          sRowCnt = 0;
    mtdIntegerType  sID;
    mtdDateType     sCurrent;
    UChar           sDate[QCM_JOB_DATE_FORMAT_LEN + 1];
    UInt            sDateSize;

    sID = ( mtdIntegerType )aJobID;

    sCurrent.year = 0;
    sCurrent.mon_day_hour = 0;
    sCurrent.min_sec_mic = 0;

    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
    {
        IDE_TEST( qtc::sysdate( &sCurrent ) != IDE_SUCCESS );
    }
    else
    {
        if ( QCU_SYSDATE_FOR_NATC[0] != '\0' )
        {
            IDE_TEST( mtdDateInterface::toDate( &sCurrent,
                                                (UChar*)QCU_SYSDATE_FOR_NATC,
                                                idlOS::strlen(QCU_SYSDATE_FOR_NATC),
                                                (UChar*)QCM_JOB_DATE_FORMAT,
                                                QCM_JOB_DATE_FORMAT_LEN )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qtc::sysdate( &sCurrent ) != IDE_SUCCESS );
        }
    }

    sCurrent.min_sec_mic &= ~( MTD_DATE_SEC_MASK | MTD_DATE_MSEC_MASK );

    IDE_TEST( mtdDateInterface::toChar( &sCurrent,
                                        sDate,
                                        &sDateSize,
                                        QCM_JOB_DATE_FORMAT_LEN,
                                        (UChar*)QCM_JOB_DATE_FORMAT,
                                        QCM_JOB_DATE_FORMAT_LEN )
              != IDE_SUCCESS );
    sDate[sDateSize] = '\0';

    idlOS::snprintf( sSqlStr, QCM_JOB_UPDATE_QUERY_LEN,
                     "UPDATE SYS_JOBS_ SET  STATE = "
                     "INTEGER'%"ID_INT32_FMT"', "
                     "LAST_EXEC_TIME = "
                     "TO_DATE('%s', '%s') "
                     "WHERE JOB_ID = "
                     "INTEGER'%"ID_INT32_FMT"'  ",
                     QCM_JOB_STATE_EXEC,
                     sDate,
                     QCM_JOB_DATE_FORMAT,
                     sID );

    IDE_TEST( qcg::runDMLforDDL( aSmiStmt,
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * updateExecuteResult
 *
 *  Meta�� Job �� ���� ����� update �Ѵ�.
 *
 *  Error �� ���ٸ� Error Code �� ������Ʈ �Ѵ�.
 */
IDE_RC qcmJob::updateExecuteResult( smiStatement * aSmiStmt,
                                    SInt           aJobID,
                                    SInt           aExecCount,
                                    UInt           aErrorCode )
{
    vSLong         sRowCnt = 0;
    mtdIntegerType sID;
    SChar          sSqlStr[QCM_JOB_UPDATE_QUERY_LEN];

    sID = ( mtdIntegerType )aJobID;

    if ( aErrorCode == 0 )
    {
        idlOS::snprintf( sSqlStr, QCM_JOB_UPDATE_QUERY_LEN,
                         "UPDATE SYS_JOBS_ SET STATE = 0, "
                         "EXEC_COUNT = "
                         QCM_SQL_INT32_FMT", "
                         "ERROR_CODE = NULL "
                         "WHERE JOB_ID = "
                         QCM_SQL_INT32_FMT,
                         aExecCount,
                         sID );
    }
    else
    {
        idlOS::snprintf( sSqlStr, QCM_JOB_UPDATE_QUERY_LEN,
                         "UPDATE SYS_JOBS_ SET STATE = 0, "
                         "EXEC_COUNT = "
                         QCM_SQL_INT32_FMT", "
                         "ERROR_CODE = "
                         "VARCHAR'0x%05"ID_XINT32_FMT"' "
                         "WHERE JOB_ID = "
                         QCM_SQL_INT32_FMT,
                         aExecCount,
                         E_ERROR_CODE( aErrorCode ),
                         sID );
    }

    IDE_TEST( qcg::runDMLforDDL( aSmiStmt,
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmJob::isExistJob( smiStatement   * aSmiStmt,
                           qcNamePosition * aJobName,
                           idBool         * aIsExist )
{
    idBool                sIsCursorOpen = ID_FALSE;
    const void          * sRow          = NULL;
    scGRID                sRid;
    smiTableCursor        sCursor;
    smiCursorProperties   sCursorProperty;
    smiRange              sRange;
    qtcMetaRangeColumn    sFirstMetaRange;
    qcNameCharBuffer      sJobNameBuffer;
    mtdCharType         * sJobName = (mtdCharType *)&sJobNameBuffer;
    mtcColumn           * sFirstMtcColumn;

    qtc::setVarcharValue( sJobName,
                          NULL,
                          aJobName->stmtText + aJobName->offset,
                          aJobName->size );

    IDE_TEST( smiGetTableColumns( gQcmJobs,
                                  QCM_JOBS_NAME_COL_ORDER,
                                  (const smiColumn**)&sFirstMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn( &sFirstMetaRange,
                                    sFirstMtcColumn,
                                    (const void *)sJobName,
                                    &sRange );
    sCursor.initialize();

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sCursorProperty, NULL );

    IDE_TEST( sCursor.open(
                  aSmiStmt,
                  gQcmJobs,
                  gQcmJobsIndex[QCM_JOBS_NAME_IDX_ORDER],
                  smiGetRowSCN( gQcmJobs ),
                  NULL,
                  &sRange,
                  smiGetDefaultKeyRange(),
                  smiGetDefaultFilter(),
                  QCM_META_CURSOR_FLAG,
                  SMI_SELECT_CURSOR,
                  &sCursorProperty )
              != IDE_SUCCESS );

    sIsCursorOpen = ID_TRUE;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );

    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
              != IDE_SUCCESS );

    if ( sRow != NULL )
    {
        *aIsExist = ID_TRUE;
    }
    else
    {
        *aIsExist = ID_FALSE;
    }

    sIsCursorOpen = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsCursorOpen == ID_TRUE )
    {
        (void)sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC qcmJob::updateInitialJobState( smiStatement * aSmiStmt )
{
    SChar           sSqlStr[QCM_JOB_UPDATE_QUERY_LEN];
    vSLong          sRowCnt = 0;

    idlOS::snprintf( sSqlStr, QCM_JOB_UPDATE_QUERY_LEN,
                     "UPDATE SYS_JOBS_ SET STATE = 0,"
                     "ERROR_CODE = "
                     "VARCHAR'0x%05"ID_XINT32_FMT"' "
                     "WHERE STATE = 1",
                     E_ERROR_CODE(idERR_ABORT_Session_Closed) );

    IDE_TEST( qcg::runDMLforDDL( aSmiStmt,
                                 sSqlStr,
                                 &sRowCnt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

