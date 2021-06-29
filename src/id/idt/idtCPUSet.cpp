/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
/*****************************************************************************
 * $Id:
 ****************************************************************************/

/*****************************************************************************
 *   NAME
 *     idtCPUSet.cpp - CPU Set Ŭ���� ����
 *
 *   DESCRIPTION
 *     TASK-6764�� ������ �߰��� CPU Set ����
 *
 *   PUBLIC FUNCTION(S)
 *
 *   PRIVATE FUNCTION(S)
 *
 *   NOTES
 *
 *   MODIFIED   (MM/DD/YY)
 ****************************************************************************/

#include <idtCPUSet.h>
#include <idu.h>

idtCPUCore  idtCPUSet::mCPUCores[IDT_MAX_CPU_CORES];
idtCPUSet   idtCPUSet::mCorePsets[IDT_MAX_CPU_CORES];
idtCPUSet   idtCPUSet::mNUMAPsets[IDT_MAX_NUMA_NODES];
idtCPUSet   idtCPUSet::mProcessPset;
SInt        idtCPUSet::mSystemCPUCount;
SInt        idtCPUSet::mNUMACount;
SInt        idtCPUSet::mAvailableCPUCount;

/*
 * Global init and destroy
 */

IDE_RC idtCPUSet::readCPUInfo(void)
{
    SInt         sRC;
    UInt         sCpuCount;
    UInt         sActualCpuCount=0;
    UInt         i;
    acp_pset_t   sPset;

    sRC = acpSysGetCPUCount(&sCpuCount);

    IDE_TEST(sRC != IDE_SUCCESS);

    idlOS::memset(mCPUCores, 0, sizeof(idtCPUCore) * IDT_MAX_CPU_CORES);
    for(i = 0; i < IDT_MAX_CPU_CORES; i++)
    {
        mCorePsets[i].clear();
    }
    for(i = 0; i < IDT_MAX_NUMA_NODES; i++)
    {
        mNUMAPsets[i].clear();
    }

    mAvailableCPUCount = IDT_MAX_CPU_CORES;

    /* ------------------------------------------------
     *  Get Actual CPU count by loop checking
     *  We are binding each cpu to this process in order to know
     *  whether this number of cpu exist actually or not.
     * ----------------------------------------------*/
        
    ACP_PSET_ZERO(&sPset);
    for (i = 0; sActualCpuCount < sCpuCount; i++)
    {
        ACP_PSET_SET(&sPset,i);
            
        if ( acpPsetBindThread(&sPset) == IDE_SUCCESS )
        {
            /* success of bind : that means, exist this cpu */

            mCPUCores[sActualCpuCount].mCoreID       = i;
            mCPUCores[sActualCpuCount].mLogicalID    = sActualCpuCount;
            mCPUCores[sActualCpuCount].mSocketID     = 0;
            mCPUCores[sActualCpuCount].mInUse        = ID_TRUE;

            mCorePsets[sActualCpuCount].addCPU(sActualCpuCount);
            mNUMAPsets[0].addCPU(sActualCpuCount);

            mProcessPset.addCPU(sActualCpuCount);
            sActualCpuCount++;
        }
        else
        {
            /* error on binding : this means there is no cpu number %i */
        }
        ACP_PSET_CLR(&sPset, i);
    }

    IDE_TEST(sActualCpuCount != sCpuCount);
    acpPsetUnbindThread();
    mNUMACount = 1;
    mSystemCPUCount = sCpuCount;
    mAvailableCPUCount = sActualCpuCount;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idtCPUSet::initializeStatic(void)
{
#if defined(ALTI_CFG_OS_LINUX)

# if (defined(ALTI_CFG_CPU_X86) || defined(ALTI_CFG_CPU_IA64))
#  define CORE_ID         "processor"
#  define MODEL_NAME      "model name"
# elif defined(ALTI_CFG_CPU_POWERPC)
#  define CORE_ID         "processor"
#  define MODEL_NAME      "cpu"
# endif

    PDL_HANDLE       sCPUInfo;

    DIR*             sNodes;
    DIR*             sCPUs;
    struct dirent*   sNodeEnt;
    struct dirent*   sCPUEnt;
    SInt             sSocketID;
    SInt             sCoreID;
    SInt             sCoreLen;
    SInt             sModelLen;
    SInt             sIndex;
    SInt             i;

    SChar*           sNumber;
    SChar            sPath[ID_MAX_FILE_NAME];
    SChar            sCPUInfoPath[ID_MAX_FILE_NAME];
    SInt             sSocketOfCPU[IDT_MAX_CPU_CORES];
    SInt             sIndexOfCPU[IDT_MAX_CPU_CORES];

    SChar            sModelName[128];

    SChar            sLine[16384];

    const SChar*     sNodePath = "/sys/devices/system/node";

    idlOS::memset(mCPUCores, 0, sizeof(idtCPUCore) * IDT_MAX_CPU_CORES);
    idlOS::memset(sSocketOfCPU, 0, IDT_MAX_CPU_CORES * sizeof(SInt));
    idlOS::memset(sIndexOfCPU,  0, IDT_MAX_CPU_CORES * sizeof(SInt));

    for(i = 0; i < IDT_MAX_CPU_CORES; i++)
    {
        mCorePsets[i].clear();
    }
    for(i = 0; i < IDT_MAX_NUMA_NODES; i++)
    {
        mNUMAPsets[i].clear();
    }

    mAvailableCPUCount = IDT_MAX_CPU_CORES;

    sNodes = idlOS::opendir(sNodePath);
    IDE_TEST_CONT(sNodes == NULL, EOLDKERNEL);
    sIndex = 0;

    while((sNodeEnt = idlOS::readdir(sNodes)) != NULL)
    {
        if(idlOS::strncmp(sNodeEnt->d_name, "node", 4) == 0)
        {
            idlOS::snprintf(sPath, ID_MAX_FILE_NAME, "%s/%s", sNodePath, sNodeEnt->d_name);
            sSocketID = (UInt)idlOS::strtol(sNodeEnt->d_name + 4, NULL, 0);

            IDE_TEST_RAISE(sSocketID >= IDT_MAX_NUMA_NODES, ENODEEXCEED);

            sCPUs = idlOS::opendir(sPath);
            IDE_TEST_RAISE(sCPUs == NULL, ENOSYSINFO);
            while((sCPUEnt = idlOS::readdir(sCPUs)) != NULL)
            {
                if((idlOS::strncmp(sCPUEnt->d_name, "cpu", 3) == 0) &&
                        (isdigit((UChar)sCPUEnt->d_name[3]) != 0))
                {
                     /* check online files(BUG-47105) */
                     idlOS::snprintf(sCPUInfoPath ,PATH_MAX ,"%s/%s/online" ,sPath ,sCPUEnt->d_name );
                     /* if there is no the file online,it means it's always available. */
                     if(idlOS::access(sCPUInfoPath, F_OK) == 0) /* if it exists. */
                     {

                        sCPUInfo = idf::open(sCPUInfoPath, O_RDONLY);
                        IDE_TEST_RAISE(sCPUInfo == PDL_INVALID_HANDLE, ENOSYSINFO);

                        if( (idf::fdgets(sLine, ID_SIZEOF(sLine), sCPUInfo) == NULL) || ( sLine[0] == '0' )  )
                        {
                            idf::close(sCPUInfo);
                            continue;
                        }
                        idf::close(sCPUInfo);

                    }
                    sCoreID    = (UInt)idlOS::strtol(sCPUEnt->d_name + 3, NULL, 0);
                    IDE_TEST_RAISE(sIndex >= IDT_MAX_CPU_CORES, ECPUEXCEED);

                    mCPUCores[sIndex].mCoreID       = sCoreID;
                    mCPUCores[sIndex].mSocketID     = sSocketID;
                    mCPUCores[sIndex].mLogicalID    = sIndex;
                    mCPUCores[sIndex].mInUse        = ID_FALSE;

                    mCorePsets[sIndex].addCPU(sIndex);
                    mNUMAPsets[sSocketID].addCPU(sIndex);

                    mNUMACount = IDL_MAX(mNUMACount, sSocketID);
                    sIndex++;
                }
                else
                {
                    /* continue */
                }
            }

            idlOS::closedir(sCPUs);
        }
        else
        {
            /* continue; */
        }
    }

    mNUMACount++;
    mSystemCPUCount = sIndex;

    sCPUInfo = idf::open("/proc/cpuinfo", O_RDONLY);
    IDE_TEST_RAISE(sCPUInfo == PDL_INVALID_HANDLE, ENOSYSINFO);

    sCoreLen  = idlOS::strlen(CORE_ID);
    sModelLen = idlOS::strlen(MODEL_NAME);

    while(idf::fdgets(sLine, sizeof(sLine), sCPUInfo) != NULL)
    {
        if(idlOS::strncmp(sLine, CORE_ID, sCoreLen) == 0)
        {
            sNumber = idlOS::strstr(sLine, ": ") + 2;
            sCoreID = idlOS::strtol(sNumber, NULL, 10);
            continue;
        }

        if(idlOS::strncmp(sLine, MODEL_NAME, sModelLen) == 0)
        {
            idlOS::strcpy(sModelName, idlOS::strstr(sLine, ": ") + 2);
            continue;
        }

        if(sLine[0] == '\n')
        {
            /* End of each CPU information reached */

            for( sIndex=0; sIndex < mSystemCPUCount ; sIndex++)
            {
                if( mCPUCores[sIndex].mCoreID == sCoreID )
                {
                    idlOS::strcpy(mCPUCores[sIndex].mModelName, sModelName);
                    break;
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_CONT(EOLDKERNEL);
    /* Try again with /proc/cpuinfo */
    IDE_CALLBACK_SEND_SYM("Could not open NUMA information.");
    IDE_CALLBACK_SEND_SYM("Trying again with cpuinfo.");
    IDE_TEST(readCPUInfo() != IDE_SUCCESS);
    return IDE_SUCCESS;

    IDE_EXCEPTION(ENOSYSINFO)
    {
        IDE_SET(ideSetErrorCode(idERR_IGNORE_NO_SUCH_CPUINFO, 
                                errno));
    }
    IDE_EXCEPTION(ENODEEXCEED)
    {
        IDE_SET(ideSetErrorCode(idERR_IGNORE_EXCEED_NUMA_NODE, 
                                sSocketID,
                                IDT_MAX_NUMA_NODES ));
    }
    IDE_EXCEPTION(ECPUEXCEED)
    {
        IDE_SET(ideSetErrorCode(idERR_IGNORE_EXCEED_CPUNO, 
                                sIndex, sSocketID,
                                IDT_MAX_CPU_CORES, IDT_MAX_NUMA_NODES));
    }

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
#else
    IDE_TEST(readCPUInfo() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
#endif
}

IDE_RC idtCPUSet::destroyStatic(void)
{
    return IDE_SUCCESS;
}

/* 
 * License CPU ���� ������ ���� mProcessPset�� �缳�� �Ѵ�.
 * ���Ǵ� CPU ����, NUMA Node ������ ���ڷ� �ް�,
 * System���� �� Node, Node������ ū CPU��ȣ ������ ����Ѵ�.
 * �������, (11,2) �� ��� ��밡�� cpu ��ȣ�� 25-29, 34-39 �� CPU�̴�.  
 * Node[0] xxxxxxxxxx (0~9)
 * Node[1] xxxxxxxxxx (10~19)
 * Node[2] xxxxxooooo (20~29)
 * Node[3] xxxxoooooo (30~39)
 */
IDE_RC idtCPUSet::relocateCPUs(SInt aCPUCount, SInt aNUMACount)
{
#if defined(ALTI_CFG_OS_LINUX)
    SInt sAllocCoreCount;
    SInt sAllocCoreIndex;
    SInt sAllocNUMACount;
    SInt sAllocNUMAIndex;

    SInt i, j;
    SInt sMore = 0;

    
    /* Take minimum of the count of NUMA nodes */
    sAllocNUMACount = IDL_MIN(mNUMACount, aNUMACount);
    IDE_TEST(sAllocNUMACount <= 0);

    /* Take minimum of the count of CPU cores */
    sAllocCoreCount = IDL_MIN(mSystemCPUCount, aCPUCount);
    IDE_TEST(sAllocCoreCount <= 0);
    mAvailableCPUCount = sAllocCoreCount; 

    if ( sAllocCoreCount > 1 )
    {
        sAllocCoreCount = mAvailableCPUCount / sAllocNUMACount;
        sMore = mAvailableCPUCount % sAllocNUMACount;
    }
     
    mProcessPset.clear();

    for(i = 0; i < sAllocNUMACount; i++)
    {
        /* �� NUMA Node���� Ž�� */
        sAllocCoreIndex = IDT_EMPTY;
        sAllocNUMAIndex = mNUMACount - (i % sAllocNUMACount) - 1;

        if( mNUMAPsets[sAllocNUMAIndex].getCPUCount() > 0 )
        {
            for(j=0; j< sAllocCoreCount; j++)
            {
                /* Node�� ���� CPU���� Ž�� */
                if( mProcessPset.getCPUCount() < mProcessPset.getAvailableCPUCount() )
                {
                    if(sAllocCoreIndex == IDT_EMPTY )
                    {
                        sAllocCoreIndex = mNUMAPsets[sAllocNUMAIndex].findLastCPU();
                    }
                    else
                    {
                        sAllocCoreIndex = mNUMAPsets[sAllocNUMAIndex].findPrevCPU(sAllocCoreIndex);
                    }

                    if( sAllocCoreIndex == IDT_EMPTY )
                    {
                        break;
                       
                    }

                    mProcessPset.addCPU(sAllocCoreIndex);
                    mCPUCores[sAllocCoreIndex].mInUse = ID_TRUE;

                }
            }

            if ( (sMore > 0) && 
                 (mProcessPset.getCPUCount() < mProcessPset.getAvailableCPUCount()) )
            {
                        
                sAllocCoreIndex = mNUMAPsets[sAllocNUMAIndex].findPrevCPU(sAllocCoreIndex);

                if( sAllocCoreIndex != IDT_EMPTY)
                {
                    mProcessPset.addCPU(sAllocCoreIndex);
                    mCPUCores[sAllocCoreIndex].mInUse = ID_TRUE;
                    sMore --;
                }
            }
        }
        else
        {
            /* continue */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
#else
    PDL_UNUSED_ARG(aNUMACount);
    PDL_UNUSED_ARG(aCPUCount);

    return IDE_SUCCESS;
#endif
}

/*
 * initialize�� ���� �۵��� �Ѵ�.
 */
idtCPUSet::idtCPUSet(const SInt aFill)
{
    initialize(aFill);
}

/*
 * CPU Set�� ���� �ý��ۿ� ������ CPU ��
 * ���̼������� �㰡�� CPU���� set�Ѵ�
 */
void idtCPUSet::fill(void)
{
    copyFrom(mProcessPset);
}

/*
 * CPU Set�� ��� ����� 0���� �����.
 */
void idtCPUSet::clear(void)
{
    mCPUCount = 0;
    idlOS::memset(mPSet, 0, IDT_PSET_SIZE);
}

/*
 * aFill�� IDT_FILL�̸� fill�ϰ� IDT_EMPTY�̸� clear�Ѵ�.
 * IDT_FILL�� (SInt(0))��, IDT_EMPTY�� ((SInt)(-1))�� ����.
 */
void idtCPUSet::initialize(const SInt aFill)
{
    switch(aFill)
    {
    case IDT_FILL:
        fill();
        break;
    case IDT_EMPTY:
        clear();
        break;
    default:
        IDE_DASSERT(0);
        clear();
        break;
    }
}

/*
 * aCPUNo�� �ش��ϴ� CPU�� set�Ѵ�.
 */
void idtCPUSet::addCPU(const SInt aCPUNo)
{
    SInt    sIndex;
    SInt    sOffset;
    ULong   sDelta;

    IDE_TEST( (aCPUNo < 0) || ((aCPUNo >= mAvailableCPUCount) && (aCPUNo >= mSystemCPUCount)) );
    IDE_TEST( mCPUCount >= mAvailableCPUCount );
    calcIndexAndDelta(aCPUNo, sIndex, sOffset, sDelta);

    if((mPSet[sIndex] & sDelta) == ID_ULONG(0))
    {
        mPSet[sIndex] |= sDelta;
        mCPUCount++;
    }
    else
    {
        /* Do nothing */
    }

    return;

    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_CPUNO, 
                            aCPUNo, 
                            mSystemCPUCount));
    return;
}

/*
 * aCPUNo�� �ش��ϴ� CPU�� clear�Ѵ�.
 */
void idtCPUSet::removeCPU(const SInt aCPUNo)
{
    SInt    sIndex;
    SInt    sOffset;
    ULong   sDelta;

    IDE_TEST( (aCPUNo < 0) || ((aCPUNo >= mAvailableCPUCount) && (aCPUNo >= mSystemCPUCount)) );
    calcIndexAndDelta(aCPUNo, sIndex, sOffset, sDelta);

    if((mPSet[sIndex] & sDelta) != ID_ULONG(0))
    {
        mPSet[sIndex] &= ~(sDelta);
        mCPUCount--;
    }
    else
    {
        /* Do nothing */
    }

    return;

    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_CPUNO, 
                            aCPUNo, 
                            mSystemCPUCount));
    return;

}

/*
 * aCPUNo�� set�Ǿ� ������ clear, clear�����̸� set�Ѵ�.
 */
void idtCPUSet::toggleCPU(const SInt aCPUNo)
{
    SInt    sIndex;
    SInt    sOffset;
    ULong   sDelta;

    IDE_TEST( (aCPUNo < 0) || ((aCPUNo >= mAvailableCPUCount) && (aCPUNo >= mSystemCPUCount)) );
    calcIndexAndDelta(aCPUNo, sIndex, sOffset, sDelta);

    if((mPSet[sIndex] & sDelta) == ID_ULONG(0))
    {
        mPSet[sIndex] |= sDelta;
        mCPUCount++;
    }
    else
    {
        mPSet[sIndex] &= ~(sDelta);
        mCPUCount--;
    }

    return;

    IDE_EXCEPTION_END;
 
    IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_CPUNO, 
                            aCPUNo, 
                            mSystemCPUCount));
    return;

}

/*
 * aNUMANo NUMA ��忡 �ش��ϴ� CPU�� ��� set�Ѵ�.
 */
void idtCPUSet::addNUMA(const SInt aNUMANo)
{
    IDE_TEST( (aNUMANo < 0) || (aNUMANo >= mNUMACount) );

    mergeFrom(mNUMAPsets[aNUMANo]);
   
    return;
    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_NUMA_NODE_NO, 
                            aNUMANo,
                            mNUMACount));
    return;
}

/*
 * aNUMANo NUMA ��忡 �ش��ϴ� CPU�� ��� clear�Ѵ�.
 */
void idtCPUSet::removeNUMA(const SInt aNUMANo)
{
    SInt   i;

    IDE_TEST( (aNUMANo < 0) || (aNUMANo >= mNUMACount) );
    mCPUCount = 0;
    for(i = 0; i < IDT_PSET_MAX_INDEX; i++)
    {
        mPSet[i] &= ~(mNUMAPsets[aNUMANo].mPSet[i]);
        mCPUCount += acpBitCountSet64(mPSet[i]);
    }
   
    return;
    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_NUMA_NODE_NO, 
                            aNUMANo, 
                            mNUMACount));
    return;
}

/*
 * aCPUNo�� ���� CPU Set�� ���ԵǾ� ������ ID_TRUE�� �����Ѵ�.
 * �ƴϸ� ID_FALSE�� �����Ѵ�.
 */
idBool idtCPUSet::find(const SInt aCPUNo)
{
    SInt    sIndex;
    SInt    sOffset;
    ULong   sDelta;
    idBool  sRet;

    IDE_TEST( (aCPUNo < 0) || ((aCPUNo >= mAvailableCPUCount) && (aCPUNo >= mSystemCPUCount)) );
    calcIndexAndDelta(aCPUNo, sIndex, sOffset, sDelta);

    sRet = ((mPSet[sIndex] & sDelta) == ID_ULONG(0))?
        ID_FALSE : ID_TRUE;

    return sRet;

    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_CPUNO, 
                            aCPUNo, 
                            mSystemCPUCount));
    return ID_FALSE;
}

/*
 * CPU Set�� �����Ǿ��ִ� CPU ������ �����Ѵ�.
 */
SInt idtCPUSet::getCPUCount(void)
{
    return mCPUCount;
}

/*
 * CPU Set�� ������ CPU���� NUMA ��� �� ����
 * ����ϰ� �ִ°� �����Ѵ�.
 */
SInt idtCPUSet::getNUMACount(void)
{
    SInt   sRet = 0;
    SInt   i;

    for(i = 0; i < IDT_MAX_NUMA_NODES; i++)
    {
        if( getCPUCountInNUMA(i) > 0 )
        {
            sRet++;
        }
    }
    return sRet;
}

/*
 * CPU Set�� ������ CPU �� aNUMANo�� ���ϴ�
 * CPU�� ������ �����Ѵ�.
 */
SInt idtCPUSet::getCPUCountInNUMA(const SInt aNUMANo)
{
    SInt   sRet = IDT_EMPTY;
    SInt   i;
    ULong  sDelta;

    IDE_TEST( (aNUMANo < 0) || (aNUMANo >= mNUMACount) );
    for(i = 0; i < IDT_PSET_MAX_INDEX; i++)
    {
        sDelta = mPSet[i] & mNUMAPsets[aNUMANo].mPSet[i];
        sRet +=  acpBitCountSet64(sDelta);
    }
   
    return sRet;
    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_NUMA_NODE_NO, 
                            aNUMANo, 
                            mNUMACount));
    return sRet;

}

/*
 * ���� CPU Set�� aNUMANo�� �ش��ϴ� CPU�� �ִ°� Ȯ���Ѵ�.
 */
idBool idtCPUSet::isInNUMA(const SInt aNUMANo)
{
    IDE_TEST( (aNUMANo < 0) || (aNUMANo >= mNUMACount) );

    return implies(mNUMAPsets[aNUMANo]);

    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_NUMA_NODE_NO, 
                            aNUMANo, 
                            mNUMACount));
    return ID_FALSE;
}

/*
 * aCPUSet�� ������ CPU Set�� ���� �ν��Ͻ��� �����ؿ´�.
 * *this�� �����Ѵ�.
 */
const idtCPUSet& idtCPUSet::copyFrom(const idtCPUSet& aCPUSet)
{
    idlOS::memcpy(mPSet, aCPUSet.mPSet, IDT_PSET_SIZE);
    mCPUCount = aCPUSet.mCPUCount;

    return *this;
}

/*
 * aCPUSet�� ������ CPU Set�� ���� �ν��Ͻ��� ������
 * CPU Set�� OR�Ͽ� ��ģ��.
 * *this�� �����Ѵ�.
 */
const idtCPUSet& idtCPUSet::mergeFrom(const idtCPUSet& aCPUSet)
{
    SInt    i;

    mCPUCount = 0;
    for(i = 0; i < IDT_PSET_MAX_INDEX; i++)
    {
        mPSet[i] |= aCPUSet.mPSet[i];
        mCPUCount += acpBitCountSet64(mPSet[i]);
    }
    return *this;
}

/*
 * CPU Set�� ������ CPU �� logical ID��
 * ���� ���� CPU ��ȣ�� �����Ѵ�.
 */
SInt idtCPUSet::findFirstCPU(void)
{
    SInt    sRet = IDT_EMPTY;
    SInt    sOffset;
    SInt    i;

    for(i = 0; i < IDT_PSET_MAX_INDEX; i++)
    {
        if(mPSet[i] == 0)
        {
            continue;
        }
        else
        {
            sOffset = acpBitFfs64(mPSet[i]);
            sRet = i * 64 + sOffset;
            break;
        }
    }

    return sRet;
}

/* 
 * CPU Set�� �����Ǿ��ְ� aCPUNo���� logical ID�� ū CPU ��
 * logical ID�� ���� ���� CPU ��ȣ�� �����Ѵ�.
 * �ش� CPU�� Set ���ο� ������ IDT_EMPTY�� �����Ѵ�.
 */
SInt idtCPUSet::findNextCPU(const SInt aCPUNo)
{
    SInt    sRet = IDT_EMPTY;
    SInt    sIndex;
    SInt    sOffset;
    ULong   sDelta;
    SInt    i;

    IDE_TEST( (aCPUNo < 0) || ((aCPUNo >= mAvailableCPUCount) && (aCPUNo >= mSystemCPUCount)) );
    calcIndexAndDelta(aCPUNo, sIndex, sOffset, sDelta);
 
    if ( sOffset != 63 )
    {
        sDelta = mPSet[sIndex] >> (sOffset + 1);
        sDelta = sDelta << (sOffset + 1);
    }
    else if( sIndex != IDT_PSET_MAX_INDEX )
    {
        sIndex++;
        sDelta = mPSet[sIndex];
    }
    else
    {
        sDelta = 0;
    }

    if(sDelta != ID_ULONG(0))
    {
        sOffset = acpBitFfs64(sDelta);
        sRet = sIndex * 64 + sOffset;
    }
    else
    {
        for(i = sIndex + 1 ; i < IDT_PSET_MAX_INDEX ; i++)
        {
            if( mPSet[i] == ID_ULONG(0) )
            {
                continue;
            }
            else
            {
                sOffset = acpBitFfs64(mPSet[i]);
                sRet = i * 64 + sOffset;
                break;
            }
        }
    }
    return sRet;

    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_CPUNO, 
                            aCPUNo, 
                            mSystemCPUCount));
    return sRet;
}
/*
 * CPU Set�� ������ CPU �� logical ID��
 * ���� ū CPU ��ȣ�� �����Ѵ�.
 */
SInt idtCPUSet::findLastCPU(void)
{
    SInt    sRet = IDT_EMPTY;
    SInt    sOffset;
    SInt    i;

    for(i = IDT_PSET_MAX_INDEX -1 ; i >= 0 ; i--)
    {
        if(mPSet[i] == 0)
        {
            continue;
        }
        else
        {
            sOffset = acpBitFls64(mPSet[i]);
            sRet = i * 64 + sOffset;
            break;
        }
    }

    return sRet;
}
/* 
 * CPU Set�� �����Ǿ��ְ� aCPUNo���� logical ID�� ���� CPU ��
 * logical ID�� ���� ū CPU ��ȣ�� �����Ѵ�.
 * �ش� CPU�� Set ���ο� ������ IDT_EMPTY�� �����Ѵ�.
 */
SInt idtCPUSet::findPrevCPU(const SInt aCPUNo)
{
    SInt    sRet = IDT_EMPTY;
    SInt    sIndex;
    SInt    sOffset;
    ULong   sDelta;
    SInt    i;

    IDE_TEST( (aCPUNo < 0) || ((aCPUNo >= mAvailableCPUCount) && (aCPUNo >= mSystemCPUCount)) );
    calcIndexAndDelta(aCPUNo, sIndex, sOffset, sDelta);
 
    if( sOffset != 0 )
    {
        sDelta = mPSet[sIndex] << ((sizeof(ULong) * 8) - sOffset);
        sDelta = sDelta >> ((sizeof(ULong) * 8) - sOffset);
    }
    else if (sIndex != 0)
    {
        sIndex--;
        sDelta = mPSet[sIndex];
    }
    else
    {
        sDelta = 0;
    }

    if(sDelta != ID_ULONG(0))
    {
        sOffset = acpBitFls64(sDelta);
        sRet = sIndex * 64 + sOffset;
    }
    else
    {
        for(i = sIndex - 1 ; i >= 0 ; i--)
        {
            if( mPSet[i] == ID_ULONG(0) )
            {
                continue;
            }
            else
            {
                sOffset = acpBitFls64(mPSet[i]);
                sRet = i * 64 + sOffset;
                break;
            }
        }
    }
    return sRet;

    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_CPUNO, 
                            aCPUNo, 
                            mSystemCPUCount));
    return sRet;
}



/*
 * CPU Set�� �����Ǿ� �ְ� aNUMANo�� ���ϴ� CPU ��
 * logical ID�� ���� ���� CPU ��ȣ�� �����Ѵ�.
 */
SInt idtCPUSet::findFirstNUMA(const SInt aNUMANo)
{
    SInt sRet = IDT_EMPTY;

    IDE_TEST( (aNUMANo < 0) || (aNUMANo >= mNUMACount) );

    sRet = mNUMAPsets[aNUMANo].findFirstCPU();
    while( sRet != IDT_EMPTY )
    {
        if (find(sRet) == ID_TRUE )
        {
            break;
        }
        sRet = mNUMAPsets[aNUMANo].findNextCPU(sRet);
    }
    return sRet;
    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_NUMA_NODE_NO,
                            aNUMANo, 
                            mNUMACount));
    return sRet;
}

/*
 * CPU Set�� �����Ǿ� �ְ� aNUMANo�� ���� ������,
 * aCPUNo���� logical ID�� ū CPU ��
 * logical ID�� ���� ���� CPU ��ȣ�� �����Ѵ�.
 * �ش� CPU�� Set ���ο� ������ IDT_EMPTY�� �����Ѵ�.
 */
SInt idtCPUSet::findNextNUMA(const SInt aCPUNo, const SInt aNUMANo)
{
    SInt sRet = IDT_EMPTY;

    IDE_TEST_RAISE( (aCPUNo < 0) || (aCPUNo >= mSystemCPUCount),
                    ECPUINVALID);
    IDE_TEST_RAISE((aNUMANo < 0) || (aNUMANo >= mNUMACount), 
                   ENODEINVALID);

    sRet = mNUMAPsets[aNUMANo].findNextCPU(aCPUNo);
    while( sRet != IDT_EMPTY )
    {
        if (find(sRet) == ID_TRUE )
        {
            break;
        }
        sRet = mNUMAPsets[aNUMANo].findNextCPU(sRet);
    }
 
    return sRet;
    IDE_EXCEPTION(ECPUINVALID)
    {
        IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_CPUNO, 
                                aCPUNo, 
                                mSystemCPUCount));
    }
    IDE_EXCEPTION(ENODEINVALID)
    {
        IDE_SET(ideSetErrorCode(idERR_IGNORE_INVALID_NUMA_NODE_NO,
                                aNUMANo,
                                mNUMACount));
    }
    IDE_EXCEPTION_END;
    return sRet;
}

/*
 * ���� �ν��Ͻ��� aCPUSet�� �����Ѱ� ���Ѵ�.
 * �����ϸ� ID_TRUE��, �ƴϸ� ID_FALSE�� �����Ѵ�.
 */
idBool idtCPUSet::compare(const idtCPUSet& aCPUSet)
{
    idBool sRet;

    if(mCPUCount == aCPUSet.mCPUCount)
    {
        sRet = (idlOS::memcmp(mPSet, aCPUSet.mPSet, IDT_PSET_SIZE) == 0)?
            ID_TRUE : ID_FALSE;
    }
    else
    {
        sRet = ID_FALSE;
    }

    return sRet;
}

/*
 * aCPUSet�� ���� �ν��Ͻ��� �κ������ΰ� Ȯ���Ѵ�.
 * �κ������̶�� ID_TRUE��, �ƴϸ� ID_FALSE�� �����Ѵ�.
 */
idBool idtCPUSet::implies(const idtCPUSet& aCPUSet)
{
    idBool sRet;
    SInt   i;

    if(mCPUCount >= aCPUSet.mCPUCount)
    {
        for(i = 0; i < IDT_PSET_MAX_INDEX; i++)
        {
            if( (mPSet[i] | aCPUSet.mPSet[i]) != mPSet[i] )
            {
                sRet = ID_FALSE;
                break;
            }
            else
            {
                /* Do nothing */
            }
            sRet = ID_TRUE;
        }
    }
    else
    {
        sRet = ID_FALSE;
    }

    return sRet;

}
 
/*
 * ���� �ν��Ͻ��� ��� ��� ��
 * CPU Set 1�� CPU Set 2�� ���������� ���Ե� CPU Set�鸸��
 * ���� instance�� set�Ѵ�.
 * *this�� �����Ѵ�.
 */
const idtCPUSet& idtCPUSet::makeIntersectionFrom(const idtCPUSet& aCPUSet1,
                                                 const idtCPUSet& aCPUSet2)
{
    SInt i;

    clear();
    for(i = 0; i < IDT_PSET_MAX_INDEX; i++)
    {
        mPSet[i] = aCPUSet1.mPSet[i] & aCPUSet2.mPSet[i];
        mCPUCount += acpBitCountSet64(mPSet[i]);
    }
    return *this;
}

/*
 * ���� �ν��Ͻ��� ��� ��� ��
 * CPU Set 1�� CPU Set 2�� ���Ե� CPU Set���� ���
 * ���� instance�� set�Ѵ�.
 * *this�� �����Ѵ�.
 */
const idtCPUSet& idtCPUSet::makeUnionFrom(const idtCPUSet& aCPUSet1,
                                          const idtCPUSet& aCPUSet2)
{
    SInt i;

    clear();
    for(i = 0; i < IDT_PSET_MAX_INDEX; i++)
    {
        mPSet[i] = aCPUSet1.mPSet[i] | aCPUSet2.mPSet[i];
        mCPUCount += acpBitCountSet64(mPSet[i]);
    }
    return *this;
}

/*
 * ���� CPU Set�� ���ڿ��� ��ȯ�Ͽ� aString�� �����Ѵ�.
 * aString�� �ִ� ���̴� aLen�̸� �׻� NUL('\0')�� ������.
 */
void idtCPUSet::dumpCPUsToString(SChar* aString, const size_t aLen)
{
    SInt    sPrevCoreID = IDT_EMPTY;       /* ������ core id�� ���� */
    SInt    i;
    size_t  sTotalLen;
    size_t  sLen;
    idBool  sIsCont = ID_FALSE;           /* ���� core id�� ���������� Ȯ���ϱ����� */
    idBool  sCoreIDs[IDT_MAX_CPU_CORES];  /* cpuid�� coreid ������ ���� */
   
    sTotalLen = aLen;
    idlOS::memset(sCoreIDs, 0, IDT_MAX_CPU_CORES*sizeof(idBool));

    sortCoreID(sCoreIDs);

    for(i=0; i<IDT_MAX_CPU_CORES; i++)
    {
         if( sCoreIDs[i] == ID_TRUE )
         {
             if( sPrevCoreID == IDT_EMPTY )
             {
                 /* ���� ���
                  * aString �� i �߰� */
                 sLen = idlOS::snprintf(aString, sTotalLen, "%d", i);
                 IDE_TEST( aLen <= sLen );
                 aString += sLen;
                 sTotalLen -= sLen;
             }
             else if( i - sPrevCoreID == 1 )
             {
                 /* ���� core id �� ���� */

                 if( sIsCont == ID_FALSE )
                 {
                     /* ���� ����
                      * aString �� '-' �߰� */
                     sLen = idlOS::snprintf(aString, sTotalLen, "-");
                     IDE_TEST( aLen <= sLen );
                     aString += sLen;
                     sTotalLen -= sLen;

                 }
                 else
                 {
                     /* ���� �� */
                 }
                 sIsCont = ID_TRUE;
             }
             else
             {
                 /* ���� core id�� ���� ���� */

                 if( sIsCont == ID_TRUE )
                 {
                     /* ���� core id ���� �����̿���, ������ ������ �ƴ�
                      * astring�� 'sPrevCoreID, i' �߰� */
                     sLen = idlOS::snprintf(aString, sTotalLen, "%d, %d", sPrevCoreID, i);
                 }
                 else
                 {
                     /* ������ ������ �ƴϿ���, ���ݵ� ������ �ƴ�
                      * astring�� ', i' �߰�*/
                     sLen = idlOS::snprintf(aString, sTotalLen, ", %d", i);
                 }
                 IDE_TEST( aLen <= sLen );
                 aString += sLen;
                 sTotalLen -= sLen;

                 sIsCont = ID_FALSE;
             }
                 
             sPrevCoreID = i;
         }
    }
    if( sIsCont == ID_TRUE )
    {
        /* �������̴� ������ �� ����ؾ���
         * aString �� 'sPrevCoreID' ���*/
        sLen = idlOS::snprintf(aString, sTotalLen, "%d", sPrevCoreID);
        IDE_TEST( aLen <= sLen );
        aString += sLen;
        sTotalLen -= sLen;
    }

    return;
    IDE_EXCEPTION_END;
    return;
}

/*
 * ���� CPU Set�� ���ڿ��� ��ȯ�Ͽ� aString�� �����Ѵ�.
 * aString�� �ִ� ���̴� aLen�̸� �׻� NUL('\0')�� ������.
 */
void idtCPUSet::dumpCPUsToHexString(SChar* aString, const size_t aLen)
{
    ULong   sCoreIDSet[IDT_PSET_MAX_INDEX];
    ULong   sDelta;
    SInt    sIndex;
    SInt    sOffset;
    SInt    i;
    size_t  sLen;
    size_t  sTotalLen;
    idBool  sCoreIDs[IDT_MAX_CPU_CORES];  /* cpuid�� coreid ������ ���� */
  
    sLen = 0;
    sTotalLen = aLen;
    idlOS::memset(sCoreIDSet, 0, IDT_PSET_SIZE);
    idlOS::memset(sCoreIDs, 0, IDT_MAX_CPU_CORES*sizeof(idBool));

    sortCoreID(sCoreIDs);

    for(i=0; i<IDT_MAX_CPU_CORES; i++)
    {
        if(sCoreIDs[i] == ID_TRUE)
        {
            calcIndexAndDelta(i, sIndex, sOffset, sDelta);
            sCoreIDSet[sIndex] |= sDelta;
        }
    }

    for(i=IDT_PSET_MAX_INDEX-1; i>=0; i--)
    {
        if( sLen == 0)
        {
            if( sCoreIDSet[i] != 0 )
            {
                sLen = idlOS::snprintf(aString, sTotalLen, "%016lX", sCoreIDSet[i]);
                IDE_TEST( aLen <= sLen );
                aString += sLen;
                sTotalLen -= sLen;
            }
            else
            {
                /* Do nothing */
            }
        }
        else
        {
            sLen = idlOS::snprintf(aString, sTotalLen, "%016lX", sCoreIDSet[i]);
            IDE_TEST( aLen <= sLen );
            aString += sLen;
            sTotalLen -= sLen;
        }
    }
    
    return;
    IDE_EXCEPTION_END;
    return;
}

void idtCPUSet::convertToPhysicalPset(acp_pset_t* aPSet)
{
    SInt sCPUNo;
    SInt sCoreID;

    ACP_PSET_ZERO(aPSet);

    sCPUNo = findFirstCPU();

    while(sCPUNo != IDT_EMPTY)
    {
        sCoreID = mCPUCores[sCPUNo].mCoreID;
        ACP_PSET_SET(aPSet, sCoreID);

        sCPUNo = findNextCPU(sCPUNo);
    }
}

/*
 * CPU Set�� ������ CPU�� core ID ������ �����Ͽ�
 * aCoreIDs�� �����Ѵ�.
 */
void idtCPUSet::sortCoreID(idBool* aCoreIDs)
{
    SInt   sRet = IDT_EMPTY;
    SInt   sIndex;

    sRet = findFirstCPU();
    while( sRet != IDT_EMPTY )
    {
        sIndex = mCPUCores[sRet].mCoreID;
        aCoreIDs[sIndex] = ID_TRUE;
        sRet = findNextCPU(sRet);
    }
}
void idtCPUSet::calcIndexAndDelta(const SInt    aCPUNo,
                                  SInt&         aIndex,
                                  SInt&         aOffset,
                                  ULong&        aDelta)
{
    IDE_DASSERT(aCPUNo <  IDT_MAX_CPU_CORES);
    IDE_DASSERT(aCPUNo >= 0);

    aIndex  = aCPUNo / (sizeof(ULong) * 8);
    aOffset = aCPUNo % (sizeof(ULong) * 8);
    aDelta  = (ID_ULONG(1) << aOffset);
}

/*
 * �����忡 CPU ���� ���� bind�� �� �ִ°��� �����Ѵ�.
 * Linux������ ID_TRUE��, ��Ÿ �ü�������� ID_FALSE�� �����Ѵ�.
 * Linux�� ������ �ü�������� �� �����忡 CPU ���� ���� bind�Ϸ���
 * root ������ �ʿ��ϴ�.
 */
idBool idtCPUSet::canSetMultipleCPUs(void)
{
#if defined(ALTI_CFG_OS_LINUX)
    return ID_TRUE;
#else
    return ID_FALSE;
#endif
}

/* �ý��ۿ� ��ġ�Ǿ� �ִ� ��ü CPU ������ �����Ѵ�. */
SInt idtCPUSet::getSystemCPUCount(void)
{
    return mSystemCPUCount;
}

/*
 * �ý��ۿ� ��ġ�Ǿ� �ִ� CPU �� ���̼����� �㰡�Ǿ�
 * ���� IN_USE=YES�� CPU�� ������ �����Ѵ�.
 */
SInt idtCPUSet::getAvailableCPUCount(void)
{
    return mAvailableCPUCount;
}

/*
 * �ý��ۿ� ��ġ�Ǿ� �ִ� NUMA ��� ������ �����Ѵ�.
 */
SInt idtCPUSet::getSystemNUMACount(void)
{
    return mNUMACount;
}

/*
 * ���� �������� affinity�� CPU Set���� �����Ѵ�.
 */
IDE_RC idtCPUSet::bindThread(void)
{
    acp_pset_t sPSet;

    convertToPhysicalPset(&sPSet);
    IDE_TEST(acpPsetBindThread(&sPSet) != ACP_RC_SUCCESS);
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_BINDTHREAD_FAILED));
    return IDE_FAILURE;
}

/*
 * ���� �������� affinity�� �����Ѵ�.
 * Linux������ ���̼����� ���Ͽ� ��� ������ CPU Core�� ���ѵǾ� ������
 * ���ѵ� CPU Set�� ���� �����带 bind�Ѵ�.
 */

IDE_RC idtCPUSet::unbindThread(void)
{
#if defined(ALTI_CFG_OS_LINUX)
    IDE_TEST(mProcessPset.bindThread() != ACP_RC_SUCCESS);
#else
    IDE_TEST(acpPsetUnbindThread() != ACP_RC_SUCCESS);
#endif
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_UNBINDTHREAD_FAILED,
                            errno));

    return IDE_FAILURE;
}
     
/*
 * ���� ���μ����� affinity�� CPU Set���� �����Ѵ�.
 */
IDE_RC idtCPUSet::bindProcess(void)
{
    acp_pset_t sPSet;

    convertToPhysicalPset(&sPSet);
    IDE_TEST(acpPsetBindProcess(&sPSet) != ACP_RC_SUCCESS);
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_BIND_PROCESS_FAILED));
    return IDE_FAILURE;
}

/*
 * ���� ���μ����� affinity�� �����Ѵ�.
 * Linux������ ���̼����� ���Ͽ� ��� ������ CPU Core�� ���ѵǾ� ������
 * ���ѵ� CPU Set�� ���� ���μ����� bind�Ѵ�.
 */
IDE_RC idtCPUSet::unbindProcess(void)
{
    IDE_TEST(acpPsetUnbindProcess() != ACP_RC_SUCCESS);
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    IDE_SET(ideSetErrorCode(idERR_IGNORE_UNBIND_PROCESS_FAILED,
                            errno));
    return IDE_FAILURE;
}

struct idtCPUCoreStat
{
    SInt    mCoreID;
    SInt    mSocketID;
    SInt    mLogicalID;
    SChar   mModelName[128];
    SChar   mInUse[4];

    void setInfo(const idtCPUCore& aCoreInfo)
    {
        mCoreID         = aCoreInfo.mCoreID;
        mSocketID       = aCoreInfo.mSocketID;
        mLogicalID      = aCoreInfo.mLogicalID;

        idlOS::strcpy(mInUse, (aCoreInfo.mInUse == ID_TRUE)? "YES":"N0");
        idlOS::strcpy(mModelName, aCoreInfo.mModelName);
    }
};

static iduFixedTableColDesc gCPUCoreColDesc[] = 
{
    {
        (SChar*)"LOGICAL_ID",
        IDU_FT_OFFSETOF(idtCPUCoreStat, mLogicalID),
        sizeof(SInt),
        IDU_FT_TYPE_INTEGER,
        NULL,
        0, 0, NULL
    },
    {
        (SChar*)"NUMA_ID",
        IDU_FT_OFFSETOF(idtCPUCoreStat, mSocketID),
        sizeof(SInt),
        IDU_FT_TYPE_INTEGER,
        NULL,
        0, 0, NULL
    },
    {
        (SChar*)"CORE_ID",
        IDU_FT_OFFSETOF(idtCPUCoreStat, mCoreID),
        sizeof(SInt),
        IDU_FT_TYPE_INTEGER,
        NULL,
        0, 0, NULL
    },
    {
        (SChar*)"IN_USE",
        IDU_FT_OFFSETOF(idtCPUCoreStat, mInUse),
        4,
        IDU_FT_TYPE_VARCHAR,
        NULL,
        0, 0, NULL
    },
    {
        (SChar*)"MODEL_NAME",
        IDU_FT_OFFSETOF(idtCPUCoreStat, mModelName),
        128,
        IDU_FT_TYPE_VARCHAR,
        NULL,
        0, 0, NULL
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0, NULL // for internal use
    }

};

static IDE_RC buildRecordForCPUCore(idvSQL*       /* aSQL */,
        void        *aHeader,
        void        * /* aDumpObj */,
        iduFixedTableMemory *aMemory)
{
    SInt            i;
    idtCPUCoreStat  sCPUs;

    for(i = 0; i < idtCPUSet::mSystemCPUCount; i++)
    {
        sCPUs.setInfo(idtCPUSet::mCPUCores[i]);
        IDE_TEST( iduFixedTable::buildRecord(aHeader,
                    aMemory,
                    (void *)&sCPUs  )
                != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

iduFixedTableDesc gCPUCoreTableDesc =
{
    (SChar *)"X$CPUCORE",
    buildRecordForCPUCore,
    gCPUCoreColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

struct idtCPUNUMAStat
{
    SInt    mNUMAID;
    SChar   mCPUIDs[128];
    SChar   mCPUIDsHex[128];

    void setInfo(const SInt aNUMAID, idtCPUSet& aNUMASet)
    {
        mNUMAID = aNUMAID;
        aNUMASet.dumpCPUsToString(mCPUIDs, 128);
        aNUMASet.dumpCPUsToHexString(mCPUIDsHex, 128);
    }
};

static iduFixedTableColDesc gNUMAStatColDesc[] = 
{
    {
        (SChar*)"NUMA_ID",
        IDU_FT_OFFSETOF(idtCPUNUMAStat, mNUMAID),
        sizeof(SInt),
        IDU_FT_TYPE_INTEGER,
        NULL,
        0, 0, NULL
    },
    {
        (SChar*)"CPUSET",
        IDU_FT_OFFSETOF(idtCPUNUMAStat, mCPUIDs),
        128,
        IDU_FT_TYPE_VARCHAR,
        NULL,
        0, 0, NULL
    },
    {
        (SChar*)"CPUSETHEX",
        IDU_FT_OFFSETOF(idtCPUNUMAStat, mCPUIDsHex),
        128,
        IDU_FT_TYPE_VARCHAR,
        NULL,
        0, 0, NULL
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0, NULL // for internal use
    }

};

static IDE_RC buildRecordForNUMAStat(idvSQL*       /* aSQL */,
        void        *aHeader,
        void        * /* aDumpObj */,
        iduFixedTableMemory *aMemory)
{
    SInt            i;
    idtCPUNUMAStat  sCPUs;

    for(i = 0; i < idtCPUSet::mNUMACount; i++)
    {
        sCPUs.setInfo(i, idtCPUSet::mNUMAPsets[i]);
        IDE_TEST( iduFixedTable::buildRecord(aHeader,
                    aMemory,
                    (void *)&sCPUs  )
                != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

iduFixedTableDesc gNUMAStatTableDesc =
{
    (SChar *)"X$NUMASTAT",
    buildRecordForNUMAStat,
    gNUMAStatColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};



