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
 **********************************************************************/

# include <smErrorCode.h>
# include <smuProperty.h>
# include <smxTransMgr.h>
# include <smxMinSCNBuild.h>

smxMinSCNBuild::smxMinSCNBuild() : idtBaseThread()
{

}

smxMinSCNBuild::~smxMinSCNBuild()
{

}

ULong smxMinSCNBuild::mVersioningMinTime;

/**********************************************************************
 *
 * Description : Thread�� �ʱ�ȭ�Ѵ�.
 *
 **********************************************************************/
IDE_RC smxMinSCNBuild::initialize()
{
    /* PROJ-2733
       TIME-SCN ����Ʈ�� �ִ� ������� ���Ѵ�. 

       - REBUILD_MIN_VIEWSCN_INTERVAL_�� 1ms �� ��´ٸ�, �ʴ� 1000���� TIME-SCN�� ����ϰ� �ȴ�. 
       - VERSIONING_MIN_TIME�� �ִ밪�� 100sec �̴�. 
       => �ִ�ũ��� 1,000 * 100 = 100,000 (+2 ����) ���� ���Ѵ�.
     */
    mTimeSCNListMaxCnt = ( 1000 /* ������Ƽ REBUILD_MIN_VIEWSCN_INTERVAL_ �� �ʴ� 1000�� */ 
                           * 100 /* ������Ƽ VERSIONING_MIN_TIME �ִ밪 100 sec */
                           + 2 /* ������ 2���� �� �ش�. */ );

    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMX,
                                 mTimeSCNListMaxCnt * ID_SIZEOF( smxTimeSCNNode ),
                                 (void**)&mTimeSCNList ) != IDE_SUCCESS );

    clearTimeSCNList();

    SM_SET_SCN_INFINITE( &mTimeSCN );
    SM_SET_SCN_INFINITE( &mAccessSCN );
    SM_SET_SCN_INFINITE( &mAgingMemViewSCN );
    SM_SET_SCN_INFINITE( &mAgingDskViewSCN );

    mVersioningMinTime  = smuProperty::getVersioningMinTime(); /* milli sec */

    mFinish = ID_FALSE;
    mResume = ID_FALSE;

    SM_INIT_SCN( &mSysMinDskViewSCN );

    // BUG-24885 wrong delayed stamping
    SM_INIT_SCN( &mSysMinDskFstViewSCN );

    // BUG-26881 �߸��� CTS stamping���� acces�� �� ���� row�� ������
    SM_INIT_SCN( &mSysMinOldestFstViewSCN );

    IDE_TEST( mMutex.initialize((SChar*)"TRANSACTION_MINVIEWSCN_BUILDER",
                                IDU_MUTEX_KIND_POSIX,
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    IDE_TEST_RAISE(mCV.initialize((SChar*)"TRANSACTION_MINVIEWSCN_BUILDER")
                   != IDE_SUCCESS, err_cond_var_init);

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_var_init );
    {
        IDE_SET( ideSetErrorCode(smERR_FATAL_ThrCondInit) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/**********************************************************************
 *
 * Description : Thread�� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smxMinSCNBuild::destroy()
{
    IDE_TEST_RAISE( mCV.destroy() != IDE_SUCCESS, err_cond_destroy );

    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );

    IDE_ASSERT( iduMemMgr::free( mTimeSCNList )
                == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_destroy );
    {
        IDE_SET( ideSetErrorCode(smERR_FATAL_ThrCondDestroy) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 *
 * Description : Thread�� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smxMinSCNBuild::startThread()
{
    /* PROJ-2733
     * db�� startSCN ���� ū SCN ���� ������ �����Ѵ�. */
    if ( isActiveVersioningMinTime() == ID_TRUE )
    {
        setGlobalConsistentSCN();
    }

    IDE_TEST(start() != IDE_SUCCESS);

    IDE_TEST(waitToStart(0) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 *
 * Description : Thread�� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smxMinSCNBuild::shutdown()
{
    SInt          sState = 0;

    lock( NULL /* idvSQL* */);
    sState = 1;

    mFinish = ID_TRUE;

    IDE_TEST_RAISE(mCV.signal() != IDE_SUCCESS, err_cond_signal);

    sState = 0;
    unlock();

    IDE_TEST_RAISE(join() != IDE_SUCCESS, err_thr_join);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_thr_join);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Systhrjoin));
    }
    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();

        unlock();

        IDE_POP();
    }

    return IDE_FAILURE;

}


/**********************************************************************
 *
 * Description : Thread�� Main Job�� �����Ѵ�.
 *
 * UPDATE_MIN_VIEWSCN_INTERVAL_ ������Ƽ�� ������ �ð����� �������
 * Minimum Disk ViewSCN�� ���Ͽ� �����Ѵ�.
 *
 **********************************************************************/
void smxMinSCNBuild::run()
{
    IDE_RC         sRC;
    SLong          sInterval;
    UInt           sState = 0;
    smSCN          sSysMinDskViewSCN;
    smSCN          sSysMinDskFstViewSCN;
    smSCN          sSysMinOldestFstViewSCN;
    PDL_Time_Value sCurrTimeValue;
    PDL_Time_Value sPropTimeValue;

restart :
    sState = 0;

    lock( NULL /* idvSQL* */);
    sState = 1;

    mResume = ID_TRUE;

    while( 1 )
    {
        // BUG-28819 REBUILD_MIN_VIEWSCN_INTERVAL_�� 0���� �����ϰ� restart�ϸ�
        // SysMinOldestFstViewSCN�� ���ŵ��� �ʰ� �ʱⰪ ��� 0�� �����Ǿ
        // ���� ���� page�� ���� �Ǿ� System�� ������ �����մϴ�.
        // Inserval�� 0�̴��� ó�� �ѹ��� �� ������ ���ϵ��� �����մϴ�.
        if( mResume == ID_TRUE )
        {
            if ( isActiveVersioningMinTime() == ID_TRUE )
            {
                /* PROJ-2733
                   SHARDING ENABLE�ÿ��� TimeSCN, AccessSCN, AgingViewSCN �� �����Ѵ�. */
                setGlobalConsistentSCN();
            }

            // Time�� �� �� ��쿡�� ���� ������ ���ϰ�
            // Inserval�� ���� �� ��쿡�� ������ �ʽ��ϴ�.

            // BUG-24885 wrong delayed stamping
            // ��� active Ʈ����ǵ��� minimum disk FstSCN�� ���ϵ��� �߰�
            smxTransMgr::getDskSCNsofAll( &sSysMinDskViewSCN,
                                          &sSysMinDskFstViewSCN,    
                                          &sSysMinOldestFstViewSCN );  // BUG-26881

            // BUG-24885 wrong delayed stamping
            // set the minimum disk FstSCN
            SM_SET_SCN( &mSysMinDskViewSCN, &sSysMinDskViewSCN );
            SM_SET_SCN( &mSysMinDskFstViewSCN, &sSysMinDskFstViewSCN );

            // BUG-26881 �߸��� CTS stamping���� acces�� �� ���� row�� ������
            SM_SET_SCN( &mSysMinOldestFstViewSCN, &sSysMinOldestFstViewSCN );
        }

        mResume   = ID_FALSE;
        sInterval = smuProperty::getRebuildMinViewSCNInterval();

        if ( sInterval != 0 )
        {
            sPropTimeValue.set( 0, sInterval * 1000 );

            sCurrTimeValue  = idlOS::gettimeofday();
            sCurrTimeValue += sPropTimeValue;

            sState = 0;
            sRC = mCV.timedwait(&mMutex, &sCurrTimeValue, IDU_IGNORE_TIMEDOUT);
            sState = 1;
        }
        else
        {
            ideLog::log( SM_TRC_LOG_LEVEL_TRANS,
                         SM_TRC_TRANS_DISABLE_MIN_VIEWSCN_BUILDER );

            sRC = mCV.wait(&mMutex);
            IDE_TEST_RAISE( sRC != IDE_SUCCESS, err_cond_wait );
        }

        if ( mFinish == ID_TRUE )
        {
            break;
        }

        IDE_TEST_RAISE( sRC != IDE_SUCCESS , err_cond_wait );

        if(mCV.isTimedOut() == ID_TRUE)
        {
            mResume = ID_TRUE;
        }
        else
        {
            /* do nothing */
        }
    }

    mResume = ID_FALSE;

    sState = 0;
    unlock();

    return;

    IDE_EXCEPTION(err_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();

        unlock();

        IDE_POP();
    }

    goto restart;
}

/**********************************************************************
 *
 * Description : ALTER SYSTEM �������� ����ڰ� Disk Minimum View SCN�� �����Ѵ�.
 *
 *               ALTER SYSTEM REBUILD MIN_VIEWSCN;
 *
 *               Rebuild ����̹Ƿ� �Լ� �Ϸ� �������� Disk Min View SCN��
 *               ������ �־�� �Ѵ�. Thread �۾��� ������ �ʰ� ���� ���Ѵ�.
 *
 * aStatistics  - [IN] statistics
 *
 **********************************************************************/
IDE_RC smxMinSCNBuild::resumeAndWait( idvSQL  * aStatistics )
{
    SInt    sState = 0;
    smSCN   sSysMinDskViewSCN;
    smSCN   sSysMinDskFstViewSCN;
    smSCN   sSysMinOldestFstViewSCN;

    lock( aStatistics );
    sState = 1;

    if ( isActiveVersioningMinTime() == ID_TRUE )
    {
        /* PROJ-2733
           SHARDING ENABLE�ÿ��� TimeSCN, AccessSCN, AgingViewSCN �� �����Ѵ�. */
        setGlobalConsistentSCN();
    }

    // BUG-24885 wrong delayed stamping
    // ��� active Ʈ����ǵ��� minimum disk FstSCN�� ���ϵ��� �߰�
    smxTransMgr::getDskSCNsofAll( &sSysMinDskViewSCN,
                                  &sSysMinDskFstViewSCN,        
                                  &sSysMinOldestFstViewSCN );  // BUG-26881

    // BUG-24885 wrong delayed stamping
    // set the minimum disk FstSCN
    SM_SET_SCN( &mSysMinDskViewSCN, &sSysMinDskViewSCN );
    SM_SET_SCN( &mSysMinDskFstViewSCN, &sSysMinDskFstViewSCN );

    IDU_FIT_POINT( "1.BUG-32650@smxMinSCNBuild::resumeAndWait" );

    // BUG-26881 �߸��� CTS stamping���� acces�� �� ���� row�� ������
    SM_SET_SCN( &mSysMinOldestFstViewSCN, &sSysMinOldestFstViewSCN );

    IDE_TEST( clearInterval() != IDE_SUCCESS );

    sState = 0;
    unlock();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();

        unlock();

        IDE_POP();
    }

    return IDE_FAILURE;

}


/**********************************************************************
 *
 * Description : MinDskViewSCN �����ֱ⸦ �����Ѵ�.
 *
 * ALTER SYSTEM SET UPDATE_MIN_VIEWSCN_INTERVAL_ .. ��������
 * ����ڰ� Minimum Disk View SCN�� �����ֱ⸦ �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smxMinSCNBuild::resetInterval()
{
    SInt   sState = 0;

    lock( NULL /* idvSQL* */ );
    sState = 1;

    IDE_TEST( clearInterval() != IDE_SUCCESS );

    sState = 0;
    unlock();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        unlock();
        IDE_POP();
    }

    return IDE_FAILURE;
}

/**********************************************************************
 *
 * PROJ-2733 �л� Ʈ����� ���ռ�
 *
 * Description : versioning min time �� �����Ѵ�.
 *
 * ALTER SYSTEM SET VERSIONING_MIN_TIME  ��������
 * ����ڰ� versioning min time�� �����Ѵ�.
 *
 * smuProperty::mVersioningMinTime ���� �������, ���Լ��� ȣ��ȴ�.
 * VERSIONING_MIN_TIME = 0 �� accessSCN, agingViewSCN�� ���̻� �������ʰ�,
 * sytem (view) scn �� ����Ѵٴ� �ǹ��̴�.
 *
 **********************************************************************/
IDE_RC smxMinSCNBuild::resetVersioningMinTime()
{
    SInt   sState = 0;
    ULong sOldVersioningMinTime;
    ULong sNewVersioningMinTime;

    lock( NULL /* idvSQL* */ );
    sState = 1;

    if ( smuProperty::getShardEnable() == ID_FALSE )
    {
        IDE_CONT( SKIP_VERSIONING_MIN_TIME_RESET );
    }

    sOldVersioningMinTime = mVersioningMinTime;

    sNewVersioningMinTime = smuProperty::getVersioningMinTime(); /* milli sec */

    if ( ( sOldVersioningMinTime == 0 ) &&
         ( sNewVersioningMinTime > 0 ) )
    {
        /* 1. �۵� ���� */

        /* ���� �۵��� �����������ִ� TIME-SCN LIST�� �����Ѵ�. */
        ID_SERIAL_BEGIN( clearTimeSCNList() );
        ID_SERIAL_END(   setGlobalConsistentSCN() );
    }
    else if ( ( sOldVersioningMinTime > 0 ) &&
              ( sNewVersioningMinTime == 0 ) )
    {
        /* 2.  �۵� ���� */

        /* AccessSCN, AgingViewSCN ���� ������ �������� �Ѵ�. */
        ID_SERIAL_BEGIN( SM_SET_SCN_INFINITE( &mTimeSCN ) );  
        ID_SERIAL_EXEC(  SM_SET_SCN_INFINITE( &mAccessSCN ), 1 ); 
        ID_SERIAL_EXEC(  SM_SET_SCN_INFINITE( &mAgingMemViewSCN ), 2 ); 
        ID_SERIAL_END(   SM_SET_SCN_INFINITE( &mAgingDskViewSCN ) ); 
    }

    mVersioningMinTime = sNewVersioningMinTime;

    IDE_EXCEPTION_CONT( SKIP_VERSIONING_MIN_TIME_RESET );

    IDE_TEST( clearInterval() != IDE_SUCCESS );

    sState = 0;
    unlock();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        unlock();
    }

    return IDE_FAILURE;
}

/* TIME-SCN LIST�� clear�Ѵ�. */
void smxMinSCNBuild::clearTimeSCNList()
{
    SInt i;

    mTimeSCNListLastIdx = -1;
    mTimeSCNListBaseIdx = -1;

    for ( i = 0;
          i < mTimeSCNListMaxCnt;
          i++ )
    {
        mTimeSCNList[i].mTime.initialize();
        SM_INIT_SCN( &(mTimeSCNList[i].mSCN) );
    }
}

/**********************************************************************
 *
 * Description : �������� ���� Interval�� �ʱ�ȭ�Ѵ�.
 *
 **********************************************************************/
IDE_RC smxMinSCNBuild::clearInterval()
{
    mResume = ID_FALSE;
    IDE_TEST_RAISE( mCV.signal() != IDE_SUCCESS, err_cond_signal );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_signal );
    {
        IDE_SET( ideSetErrorCode(smERR_FATAL_ThrCondSignal) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 PROJ-2733 �л� Ʈ����� ���ռ� :  TIME-SCN ����Ʈ�� �ֽ������� �߰��Ѵ�.

 TIME-SCN ����Ʈ�� ������ ũ���� ARRAY�� index 0,1,2, ... ������ ��ϵǸ�,
 ��� ����� ��� index 0 ���� ������������.
 �������� ����� index ��ġ�� mTimeSCNListLastIdx�� ��ϵȴ�.
 **********************************************************************/
void smxMinSCNBuild::addTimeSCNList()
{
    SInt sLastIdx;

    sLastIdx = mTimeSCNListLastIdx;

    sLastIdx = ( (sLastIdx + 1) % mTimeSCNListMaxCnt );

    mTimeSCNList[sLastIdx].mTime = idlOS::gettimeofday();
    mTimeSCNList[sLastIdx].mSCN  = smmDatabase::getLstSystemSCN();

    mTimeSCNListLastIdx = sLastIdx;
}

/**********************************************************************
 PROJ-2733 �л� Ʈ����� ���ռ� : ����ð��� �������� TIME SCN�� ���Ѵ�.

 ����ð����� ������ƼVERSIONING_MIN_TIME(����Ʈ 6��)�� �� �ð���
 TIME-SCN ����Ʈ���� ã�� �װͿ� ��Ī�Ǵ� SCN�� TIME SCN���� ���Ѵ�.
 ��Ī�Ǵ� �ð��� ���ٸ� ���� ����� ���ð����� ���Ѵ�.
 
 ex) TIME-SCN ����Ʈ�� ��ϵ� �ð���
     1, 2, 3, 4, 5, 6, 7, 8 �̰� ����ð��� 8.5���,

     8.5 - 6(������Ƽ ����Ʈ) = 2.5 
     2.5�� ���� ����� ���ð� 2�� ���õȴ�.
 **********************************************************************/
void smxMinSCNBuild::setTimeSCN()
{
    SInt           sIdx;
    smSCN          sSCN;
#ifdef DEBUG
    smSCN          sOldTimeSCN;
#endif
    smSCN          sNewTimeSCN;
    PDL_Time_Value sOldTime;
    PDL_Time_Value sNewTime;
    PDL_Time_Value sTime;
    PDL_Time_Value sAddTime;
    PDL_Time_Value sCurTime;

    /* addTimeSCNList()�� ȣ��Ǿ�,
       TIME-SCN LIST�� ��� �ϳ��� item�� ������ �����Ѵ�. */
    addTimeSCNList();

    IDE_DASSERT( mTimeSCNListLastIdx >= 0 );

    sIdx     = mTimeSCNListLastIdx;
    sCurTime = idlOS::gettimeofday();

    sAddTime.msec( mVersioningMinTime ); /* milli sec */

    /* ���� �ֱٽð����� ������ �˻��Ѵ�. */
    while ( 1 )
    {
        sTime = mTimeSCNList[sIdx].mTime;
        sSCN  = mTimeSCNList[sIdx].mSCN;

        if ( SM_SCN_IS_INIT( sSCN ) )
        {
            /* �ʱ�ȭ�� ���̸� �������� �����Ѵ�. */
            sIdx = ( (sIdx + 1) % mTimeSCNListMaxCnt );
            break;      
        }

        if ( ( sTime + sAddTime ) <= sCurTime )
        {
            /* found */
            break;
        }

        sIdx = ( ( sIdx == 0 )
                 ? ( mTimeSCNListMaxCnt - 1 ) : ( sIdx - 1 ) );

        if ( sIdx == mTimeSCNListLastIdx )
        {
            /* �˻��� �����µ�, ������ TIME SCN�� ã�����ߴ�.
               LIST���� ���� ������ ������ TIME SCN�� �����Ѵ�. */
            sIdx = ( (sIdx + 1) % mTimeSCNListMaxCnt );
            break;      
        }
    }

#ifdef DEBUG
    getTimeSCN( &sOldTimeSCN );
#endif
    sOldTime.initialize(); /* 0, 0 */
    if ( mTimeSCNListBaseIdx > 0 )
    {
        sOldTime = mTimeSCNList[mTimeSCNListBaseIdx].mTime;
    }

    sNewTimeSCN = mTimeSCNList[sIdx].mSCN;
    sNewTime = mTimeSCNList[sIdx].mTime;

    /* set */
    if ( mTimeSCNListBaseIdx != sIdx )
    {
        if ( sOldTime < sNewTime )
        {
#ifdef DEBUG
            if ( SM_SCN_IS_NOT_INFINITE( sOldTimeSCN ) )
            {
                IDE_ASSERT( SM_SCN_IS_LE( &sOldTimeSCN, &sNewTimeSCN ) );
            }
#endif
            SM_SET_SCN( &mTimeSCN, &sNewTimeSCN );
            mTimeSCNListBaseIdx = sIdx;
        }
    }
}

/* min view�� TIME SCN���� AgingViewSCN�� ���Ѵ�. */
void smxMinSCNBuild::setAgingMemViewSCN()
{
    smSCN sNewAgingMemViewSCN;
    smTID sDummyTID;

    /* ���ο��� TIME SCN�� ���Ѵ�. */
    smxTransMgr::getMinMemViewSCNofAll( &sNewAgingMemViewSCN,
                                        &sDummyTID,
                                        ID_TRUE /* TIME_SCN�� �ʱⰪ���� min view�� ���Ѵ�. */ );

    /* AgingViewSCN �� ��� �����Ѵ� */
    if ( SM_SCN_IS_INFINITE( mAgingMemViewSCN ) ||
         SM_SCN_IS_LT( &mAgingMemViewSCN, &sNewAgingMemViewSCN ) )
    {
        SM_SET_SCN( &mAgingMemViewSCN, &sNewAgingMemViewSCN );
    }
}

/* dsk min view, TIME SCN �� ���� ������ AgingViewSCN�� ���Ѵ�. 
   smxMinSCNBuild::run() ���� ȣ��ȴ�. 
   AccessSCN �������� �ʰ� �����Ǿ�� �Ѵ�. 
 */
void smxMinSCNBuild::setAgingDskViewSCN()
{
    smSCN sTimeSCN; 
    smSCN sSysMinDskViewSCN;
    smSCN sNewAgingDskViewSCN;


    getTimeSCN( &sTimeSCN );
    IDE_DASSERT( SM_SCN_IS_EQ( &sTimeSCN,&mAccessSCN ) );
    SM_SET_SCN_VIEW_BIT( &sTimeSCN );

    /* ���ο��� TIME SCN�� �� �� �ߴ� */
    /* BUGBUG - �������� �� �ֱ��� mSysMinDskFstViewSCN �̱���. 
                aging �и��� ���⵵ ������ */
    getMinDskViewSCN( &sSysMinDskViewSCN );

    sNewAgingDskViewSCN = SM_SCN_IS_LT(&sTimeSCN,&sSysMinDskViewSCN) ? sTimeSCN : sSysMinDskViewSCN;

    IDE_DASSERT( SM_SCN_IS_NOT_INFINITE( sNewAgingDskViewSCN ) );

    /* AgingViewSCN �� ��� �����Ѵ� */
    if ( SM_SCN_IS_INFINITE( mAgingDskViewSCN ) ||
         SM_SCN_IS_LT( &mAgingDskViewSCN, &sNewAgingDskViewSCN ) )
    {
        SM_SET_SCN( &mAgingDskViewSCN, &sNewAgingDskViewSCN );
    }
}

/* TimeSCN���� AccessSCN�� �����Ѵ�. */
void smxMinSCNBuild::setAccessSCN()
{
    smSCN sOldAccessSCN;
    smSCN sNewAccessSCN;

    getAccessSCN( &sOldAccessSCN );

    getTimeSCN( &sNewAccessSCN );

    /* AccessSCN �� ��� �����Ѵ� */
    if ( SM_SCN_IS_INFINITE( sOldAccessSCN ) ||
         SM_SCN_IS_LT( &sOldAccessSCN, &sNewAccessSCN ) )
    {
        SM_SET_SCN( &mAccessSCN, &sNewAccessSCN );
    }
}

/* TimeSCN/AccessSCN/AgingViewSCN �� ���� system scn ���� �缳���Ѵ�.
   Replica SCN�� �����Ҷ� ���ȴ�.
   �ܺ� �������̽����� ȣ��ȴ�. */
void smxMinSCNBuild::setGlobalConsistentSCNAsSystemSCN()
{
    lock( NULL );

    if ( isActiveVersioningMinTime() == ID_FALSE )
    {
        IDE_CONT( SKIP_ACCESS_SCN_SET );
    }

    ID_SERIAL_BEGIN( clearTimeSCNList() );
    ID_SERIAL_END(   setGlobalConsistentSCN() );

    IDE_EXCEPTION_CONT( SKIP_ACCESS_SCN_SET );

    unlock();
}

void smxMinSCNBuild::setGlobalConsistentSCN()
{
    /* AccessSCN�� AgingViewSCN�� �����Ǵ� ���� �������� ������ �������� �Ѵ�.

       AccessSCN�� ���� �����ϰ�, AgingViewSCN �� �����ؾ��Ѵ�.
       MUST BE ( AccessSCN >= AgingViewSCN )

       �ݷ�
     
       0. AgingViewSCN�� ���ŵ�.
       1. TX�� AgingViewSCN���� ���� fstMinView�� ���õȴ�.
          ( ���ĺ��� AgingViewSCN�� ���� )
       2. AccessSCN��fstMinView üũ������, ���
         => �����߻�: Aging�� �Ǿ���ȴµ� ����fstMinView�� ������.
       3. AccessSCN���ŵ�.

       ������ �߻������������� AccessSCN�� AgingViewSCN���� ���� ���ŵǾ�� ��.
     */
    ID_SERIAL_BEGIN( setTimeSCN() );  
    ID_SERIAL_EXEC(  setAccessSCN(), 1 ); 
    ID_SERIAL_EXEC(  setAgingMemViewSCN(), 2 ); 
    ID_SERIAL_END(   setAgingDskViewSCN() ); 
}

