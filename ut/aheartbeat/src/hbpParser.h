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
 
#ifndef _HBP_PARSER_H_
#define _HBP_PARSER_H_ 1

#include <hbp.h>


/* string �� delimiter�� ����Ͽ� �ϳ��� token�� ��� �Լ�
 *
 * aSrcStr (input) - HBP_setting_file���� �о�� string ( \n ���� )
 * aOffset (output) - ���� token�� ������ ��ġ 
 * aDstStr (output) - token������Ǵ� ��
 */
acp_sint32_t hbpGetToken( acp_char_t       *aSrcStr,
                          acp_sint32_t      aSrcLength,
                          acp_sint32_t    *aOffset,
                          acp_char_t      *aDstStr );


/* aHBPInfo�� �о�� host �Ľ��ؼ� �ִ� �Լ�
 * 
 * aStr (input) - HBP_setting_file���� �о�� string ( \n ���� )
 * aHBPInfo (output) - �� Array�� setting ������ �ִ´�.
 */
ACI_RC hbpAddHostToHBPInfo( acp_char_t* aStr, HBPInfo* aHBPInfo );

acp_size_t hbpGetInformationStrLen( acp_char_t *aLine );

acp_size_t hbpGetEqualOffset( acp_char_t *aLine, acp_size_t aLineLen );

ACI_RC hbpParseUserInfo( acp_char_t *aLine,
                         acp_char_t *aUserName,
                         acp_char_t *aPassWord );

/* does aString read from setting file have information?
 * aStr (input) - HBP_setting_file���� �о�� string ( \n ���� )
 */
acp_bool_t hbpIsInformationLine( acp_char_t* aStr );

/* HBP_setting_file���� status�� �̾� StatusArray�� �ִ� �Լ�
 *
 * aFilePtr   (input)     - status ���� �����͸� �Ѱ��ش�. 
 * aCount     (output)     - status count+ 1 (column..)
 * aInfoArray (output)    - HBP ������ ����ִ� Array.
 */
ACI_RC hbpSetHBPInfoArray( acp_std_file_t   *aFilePtr,
                           HBPInfo          *aInfoArray,
                           acp_sint32_t     *aCount );
/* HBP_setting_file���� status�� �̾� print
 *
 * aFilePtr   (input)     - status ���� �����͸� �Ѱ��ش�.
 * aCount     (input)     - status count+1 (column..)
 * */
void hbpPrintInfo( HBPInfo* aInfoArray, acp_sint32_t aCount );

/* read File and initialize HBPInfo
 *
 * aHBPInfo       (output)    - HBP ������ ����ִ� Array.
 * aCount         (output)    - status count+ 1 (column..)
 */
ACI_RC hbpInitializeHBPInfo( HBPInfo *aHBPInfo, acp_sint32_t *aCount );

void hbpFinalizeHBPInfo( HBPInfo  *aHBPInfo, acp_sint32_t aCount );

void logMessage( ace_msgid_t aMessageId, ... );

/* set user info
 * aUserName    (input)
 * aPassWord    (input)
 */
ACI_RC hbpSetUserInfo( acp_char_t *aUserName, acp_char_t *aPassWord );

acp_bool_t hbpIsValidIPFormat(acp_char_t * aIP);


/* Env */

/* get My HBP ID from Environment variable
 *
 * aID       (output)    - HBP ID
 */
ACI_RC hbpGetMyHBPID( acp_sint32_t *aID );

void hbpGetVersionInfo( acp_char_t *aBannerContents, acp_sint32_t *aBannerLen );
#endif
