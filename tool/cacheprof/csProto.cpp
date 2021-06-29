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
 

/*****************************************************************************
 * $Id: csProto.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 ****************************************************************************/

#include <csProto.h>
/* ------------------------------------------------
 *  �Ʒ��� �Լ��� ����ڰ� �������ϸ���
 *  �ϱ� ���� �����ϴ� ����� �ڵ� �߿�
 *  ���ԵǴ� �Լ�����Ʈ�̴�.
 *
 *  Atom�� ��ġ��, �Ʒ��� �Լ��� �ҷ����� �ʰ�,
 *  ���� csClear(), csStart(), csStop(), csSave(char *)�� 
 *  ��ġ�Ǿ� �����Լ��� �ҷ�����.
 
 * ----------------------------------------------*/

void cs_cache_clear()
{
}

void cs_cache_start()
{
}

void cs_cache_stop()
{
}

void cs_cache_save(char *aFile)
{
}

void __cs_cache_prefetch(void *aAddress)
{
}
