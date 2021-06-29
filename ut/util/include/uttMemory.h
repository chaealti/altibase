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
 * $Id: uttMemory.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

/***********************************************************************
 *
 * NAME
 *   uttMemory.h
 * 
 * DESCRIPTION
 *   Dynamic memory allocator.
 * 
 * PUBLIC FUNCTION(S)
 *   uttMemory( ULong BufferSize, ULong Align )
 *      BufferSize�� �޸� �Ҵ��� ���� �߰� ������ ũ�� �Ҵ�޴�
 *      �޸��� ũ��� BufferSize�� �ʰ��� �� �����ϴ�.
 *      Align�� �Ҵ�� �޸��� �����͸� Align�� ����� �Ұ���
 *      �����ϴ� ���Դϴ�. �⺻ ���� 8Bytes �Դϴ�.
 * 
 *   void* alloc( size_t Size )
 *      Size��ŭ�� �޸𸮸� �Ҵ��� �ݴϴ�.
 * 
 *   void clear( )
 *      �Ҵ���� ��� �޸𸮸� ���� �մϴ�.
 * 
 * NOTES
 * 
 * MODIFIED    (MM/DD/YYYY)
 *    assam     01/12/2000 - Created
 * 
 **********************************************************************/

#ifndef _O_UTTMEMORY_H_
# define _O_UTTMEMORY_H_  1

#include <idl.h>

typedef struct uttMemoryHeader {
    uttMemoryHeader* next;
    char*            buffer;
    ULong            length;
    ULong            cursor;
} uttMemoryHeader;

typedef struct uttMemoryStatus {
    uttMemoryHeader* savedCurrent;
    ULong            savedCursor;
} uttMemoryStatus;

class PDL_Proper_Export_Flag uttMemory {
 private:
    uttMemoryHeader* head_;
    uttMemoryHeader* current_;
    ULong            buffer_size_;
    
    void*  header( void );
    void*  extend( ULong bufferSize );
    void   release( uttMemoryHeader* Clue );
 public:
    uttMemory();
    uttMemory( ULong BufferSize );
    ~uttMemory( );
    void  init( ULong BufferSize = 0 );
    void* cralloc( size_t Size );
    void* alloc( size_t Size );
    
   /*----------------------------------------------------------- 
    * Use it only when s1 is defined as an array.
    * Never use it if s1 is a pointer allocated dynamically
    *----------------------------------------------------------*/
    //SInt   strcpy(SChar* s1, const SChar* s2);
    
   /*----------------------------------------------------------- 
    * Never use it if s1 is defined as an array.
    * Never use it if s1 is not null.
    *----------------------------------------------------------*/
    SChar* utt_strdup(const SChar* s);
    
    SInt  getStatus( uttMemoryStatus* Status );
    SInt  setStatus( uttMemoryStatus* Status );
    
    void  clear( void );
    
    void  freeAll( void );
    void  freeUnused( void );
};

#endif /* _O_UTTMEMORY_H_ */
