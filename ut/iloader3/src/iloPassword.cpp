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
 * $Id: iloPassword.cpp 87077 2020-03-31 00:11:00Z chkim $
 **********************************************************************/

#include <ilo.h>

SChar *
getpass(const SChar *prompt)
{
    static SChar    buf[MAX_PASS_LEN + 1];

    SChar         * ptr;
#if defined(L_ctermid)
    SChar           sTermPathname[L_ctermid + 1];
#endif
    FILE          * fp;
    SInt            c;

#if defined(VC_WIN32)

    DWORD flag;
    BOOL  isConsole;

    if ( (fp = stdin) == NULL )
        return NULL;

    isConsole = GetConsoleMode( GetStdHandle( STD_INPUT_HANDLE ), &flag );

    if ( isConsole )
    {
        (void)SetConsoleMode( GetStdHandle( STD_INPUT_HANDLE ),
                              flag & (~ENABLE_ECHO_INPUT) );
    }

    idlOS::fprintf( stdout, "%s", prompt );
    idlOS::fflush( stdout );

#else
    sigset_t       sig, sigsave;
    struct termios term, termsave;
    /* BUG-47741 HP-UX error with here document */
    SChar        * sControlTermPath = NULL;

# if defined(L_ctermid)
    sControlTermPath = sTermPathname;
# else
#  if defined(HP_HPUX) || defined(IA64_HP_HPUX)
    sControlTermPath = ttyname(0);
#  endif
# endif

    if ( (fp = idlOS::fopen(ctermid(sControlTermPath), "r+")) == NULL )
       return NULL;

    setbuf(fp, NULL);

    idlOS::sigemptyset(&sig);
    idlOS::sigaddset(&sig, SIGINT);
    idlOS::sigaddset(&sig, SIGTSTP);
    idlOS::sigprocmask(SIG_BLOCK, &sig, &sigsave);

    idlOS::tcgetattr(fileno(fp), &termsave);
    term = termsave;
    term.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    idlOS::tcsetattr(fileno(fp), TCSAFLUSH, &term);

    idlOS::fputs(prompt, fp);

#endif

    ptr = buf;
    while ( (c = getc(fp)) != EOF && c != '\n' )
    {
        if (ptr < &buf[MAX_PASS_LEN])
            *ptr++ = c;
    }
    *ptr = 0;

#if defined(VC_WIN32)

    if ( isConsole )
    {
        (void)SetConsoleMode( GetStdHandle( STD_INPUT_HANDLE ), flag );
        idlOS::fprintf( stdout, "\n" );
        idlOS::fflush( stdout );
    }

#else

    putc('\n', fp);

    idlOS::tcsetattr(fileno(fp), TCSAFLUSH, &termsave);

    idlOS::sigprocmask(SIG_SETMASK, &sigsave, NULL);
    idlOS::fclose(fp);

#endif

    return buf;
}
