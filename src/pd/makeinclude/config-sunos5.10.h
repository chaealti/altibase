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
 
/* -*- C++ -*- */
// $Id: config-sunos5.10.h 80576 2017-07-21 07:07:02Z yoonhee.kim $

// The following configuration file is designed to work for SunOS 5.10
// (Solaris 10) platforms using the SunC++ 5.x (Sun Studio 8), or g++
// compilers.

#ifndef ACE_CONFIG_H

// ACE_CONFIG_H is defined by one of the following #included headers.

// #include the SunOS 5.9 config, then add any SunOS 5.10 updates below.

// FlexLexer.h���� iostream�� include�ϴµ�,
// iostream���� stdexcept�� include�Ѵ�.
// stdexcept������
// _RWSTD_NO_EXCEPTIONS�� define �Ǿ� ������,
// _RWSTD_THROW_SPEC_NULL�� throw()�� �������� �ʴµ�,
// ������ stdexcept���� ������ �߻��Ѵ�.
// �̸� ���� ���� _RWSTD_NO_EXCEPTIONS�� undef �Ѵ�.
// _RWSTD_NO_EXCEPTIONS�� config-sunos5.5.h���� 1�� �����Ѵ�.
// PDL_HAS_EXCEPTIONS�� ���ǵǾ� ������ _RWSTD_NO_EXCEPTIONS�� 
// �������� �ʴ´�. (config-sunos5.5.h����...)
// by kumdory
//#undef _RWSTD_NO_EXCEPTIONS
#define PDL_HAS_EXCEPTIONS 1

// SES�� seshx.cpp���� ifstream.h ��� ifstream�� include�ϱ� ���� ������.
#define USE_STD_IOSTREAM   1

#include "config-sunos5.9.h"

#define ACE_HAS_SCANDIR

#endif /* ACE_CONFIG_H */


