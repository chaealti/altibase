/***********************************************************************
 * Copyright 1999-2007, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: ideCallback.h 76958 2016-08-31 07:49:13Z jake.jang $
 **********************************************************************/

/***********************************************************************
 * NAME
 *  ideCallback.h
 *
 * DESCRIPTION
 *  Trace Log relative Callbacks
 *
 * **********************************************************************/

#ifndef _O_IDE_CALLBACK_H_
#define _O_IDE_CALLBACK_H_ 1

#include <idTypes.h>
#include <ideErrorMgr.h>

// Trace ���� �ѱ涧 NULL �����͸� ���ϱ� ����.
#define IDE_PTR(a) ((vULong)(a))

// Trace ���� �ѱ涧 String(Char *) �����͸� ���ϱ� ����.
#define IDE_STR(a) ((a) == NULL ? "NULL" : (a))

#define IDE_CALLBACK_SEND_MSG(a) gCallbackForMessage(a, 1, ID_TRUE)
#define IDE_CALLBACK_SEND_SYM(a) gCallbackForMessage(a, 0, ID_TRUE)
#define IDE_CALLBACK_SEND_MSG_NOLOG(a) gCallbackForMessage(a, 1, ID_FALSE)
#define IDE_CALLBACK_SEND_SYM_NOLOG(a) gCallbackForMessage(a, 0, ID_FALSE)
#define IDE_CALLBACK_FATAL(a)    gCallbackForFatal((SChar *)idlVA::basename(__FILE__), __LINE__, (SChar *)a)

// PROJ-1681 ������ National CharacterSet ����
#define IDE_CALLBACK_SEND_NCHAR() gCallbackForNChar()

#define IDE_WARNING(mod, msg)      ideLog::log(mod, msg)
#define IDE_ERRLOG(mod)            ideLog::logErrorMsg(mod)

#if defined(VC_WIN32)
#define os_assert(a) ;
#else
#define os_assert(a) assert(a)
#endif

// ������Ƽ�� �ε��Ǳ� ������ IDE_ASSERT()�� ����ؼ��� �ȵ�!
#if !defined(SMALL_FOOTPRINT)

#define IDE_ASSERT(a)    if( IDL_LIKELY_TRUE( (a) )) { } else {         \
        ideLogError("IDE_ASSERT( "#a" ), ",                             \
                           ID_FALSE,                                    \
                           (SChar *)idlVA::basename(__FILE__),          \
                           __LINE__,                                    \
                           "errno=[%"ID_UINT32_FMT"]\n",                \
                           errno);                                      \
        (void)gCallbackForAssert((SChar *)idlVA::basename(__FILE__),    \
                                 __LINE__, ID_FALSE);                   \
        os_assert(0);                                                   \
        idlOS::exit(-1);}

/* PROJ-2617 */
#define IDE_FT_ASSERT( a )                                              \
    do {                                                                \
        if (IDL_LIKELY_TRUE( (a) )) { } else {                          \
            ideLogError("IDE_FT_ASSERT( "#a" )",                        \
                           ID_TRUE,                                    \
                           (SChar *)idlVA::basename(__FILE__),          \
                           __LINE__,                                    \
                           "errno=[%"ID_UINT32_FMT"]\n",                \
                           errno);                                      \
            if (gCallbackForAssert((SChar *)idlVA::basename(__FILE__),  \
                                   __LINE__, ID_TRUE) == ID_TRUE) {     \
                os_assert(0);                                           \
                idlOS::exit(-1);                                        \
            }                                                           \
            else {                                                      \
                ideNonLocalJumpForFaultTolerance();                     \
            }                                                           \
        }                                                               \
    } while (0)
#else /* defined(SMALL_FOOTPRINT) */

//��ũ�θ� �Լ��� ��ü�Ѵ�.
extern void ideAssert( const SChar * aSource,
                       idBool        aAcceptFaultTolerance, /* PROJ-2617 */
                       const SChar * aFile,
                       SInt aLine );
#define IDE_ASSERT(a)                                 \
    if (!(a)) {                                       \
        ideAssert( "IDE_ASSERT( "#a" )",              \
                   ID_FALSE, __FILE__, __LINE__);     \
    }                                                 \
    else { /* do nothing */ }

#define IDE_FT_ASSERT( a )                            \
    if (!(a)) {                                       \
        ideAssert( "IDE_FT_ASSERT( "#a" )",           \
                   ID_TRUE, __FILE__, __LINE__);      \
    }                                                 \
    else { /* do nothing */ }

#endif /* !defined(SMALL_FOOTPRINT) */

/* PROJ-2118 BUG Reporting - Message ��� Assert ��ũ�� �߰� */
//��ũ�θ� �Լ��� ��ü�Ѵ�.
extern void ideAssertMsg( const SChar * aSource,
                          idBool        aAcceptFaultTolerance, /* PROJ-2617 */
                          const SChar * aFile,
                          SInt          aLine,
                          const SChar * aFormat,
                          ... );
#define IDE_ASSERT_MSG( a, format, ... )              \
    if (!(a)) {                                       \
        ideAssertMsg( "IDE_ASSERT_MSG( "#a" )",       \
                      ID_FALSE, __FILE__, __LINE__,   \
                      format, ##__VA_ARGS__ );        \
    }                                                 \
    else { /* do nothing */ }

/* PROJ-2617 */
#define IDE_FT_ASSERT_MSG( a, format, ... )           \
    if (!(a)) {                                       \
        ideAssertMsg( "IDE_FT_ASSERT_MSG( "#a" )",    \
                      ID_TRUE, __FILE__, __LINE__,    \
                      format, ##__VA_ARGS__ );        \
    }                                                 \
    else { /* do nothing */ }


#if defined(DEBUG)
#define IDE_DASSERT(a)     IDE_ASSERT(a)
#define IDE_DASSERT_MSG( a, format, ... )            \
    IDE_ASSERT_MSG( a, format, ##__VA_ARGS__ )
#else
#define IDE_DASSERT(a) ;
#define IDE_DASSERT_MSG( a, format, ... ) ;
#endif

#define IDE_BUFFER_SIZE          (8 * 1024)

typedef IDE_RC (*ideCallbackForMessage)(const SChar *, SInt, idBool);
typedef void   (*ideCallbackForFatal)(SChar *file, SInt linenum, SChar *msg);
typedef void   (*ideCallbackForWarning)(SChar *file, SInt linenum, SChar *msg);
typedef void   (*ideCallbackForErrlog)(SChar *file, SInt linenum);
typedef idBool (*ideCallbackForAssert)(SChar *file,
                                       SInt   linenum,
                                       idBool aAcceptFaultTolerance); /* PROJ-2617 */
typedef idBool (*ideCallbackForDumpStack)(); //fix PROJ-1749
typedef IDE_RC (*ideCallbackForNChar)();

void ideFinalizeCallbackFunctions( void );

void ideSetCallbackFunctions(
    ideCallbackForMessage,
    ideCallbackForFatal,
    ideCallbackForWarning,
    ideCallbackForErrlog);

void ideSetCallbackMessage  (ideCallbackForMessage);
void ideSetCallbackFatal    (ideCallbackForFatal);
void ideSetCallbackWarning  (ideCallbackForWarning);
void ideSetCallbackErrlog   (ideCallbackForErrlog);
void ideSetCallbackAssert   (ideCallbackForAssert);
void ideSetCallbackDumpStack(ideCallbackForDumpStack); //fix PROJ-1749
void ideSetCallbackNChar    (ideCallbackForNChar);

extern ideCallbackForMessage   gCallbackForMessage;
extern ideCallbackForFatal     gCallbackForFatal;
extern ideCallbackForWarning   gCallbackForWarning;
extern ideCallbackForErrlog    gCallbackForErrlog;
extern ideCallbackForAssert    gCallbackForAssert;
extern ideCallbackForDumpStack gCallbackForDumpStack; //fix PROJ-1749
extern ideCallbackForNChar     gCallbackForNChar;

/* BUG-39946 */
IDE_RC ideNullCallbackFuncForMessage(const SChar *aMsg, SInt aCRFlag, idBool aLogMsg);
void ideNullCallbackFuncForFatal(SChar *aFile, SInt aLine, SChar *aMsg);
void ideNullCallbackFuncForWarning(SChar *aFile, SInt aLine, SChar *aMsg);
void ideNullCallbackFuncForErrlog(SChar *aFile, SInt aLine);
idBool ideNullCallbackFuncForAssert(SChar *aFile,
                                    SInt   aLine,
                                    idBool aAcceptFaultTolerance); /* PROJ-2617 */
idBool ideNullCallbackFuncForDumpStack();
IDE_RC ideNullCallbackFuncForNChar();

#ifdef DEBUG
#define IDE_DEBUG(msg)                                          \
    {                                                           \
        ideMsgLog   *sMsgLog = ideLog::getMsgLog(IDE_SERVER);   \
        FILE        *sLogFile = sMsgLog->getLogFile();          \
        idlOS::fprintf(sLogFile, msg);                          \
        idlOS::fflush(sLogFile);                                \
    }
#else
#define IDE_DEBUG(msg)  ;
#endif

/* -------------------------------------------------
 *  Source Code Info Output Control
 * -----------------------------------------------*/

#define IDE_SOURCE_INFO_SERVER  0x0001
#define IDE_SOURCE_INFO_CLIENT  0x0002

#endif /* _O_IDE_CALLBACK_H_ */
