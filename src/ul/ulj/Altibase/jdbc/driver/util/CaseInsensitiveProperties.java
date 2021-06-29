/*
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

package Altibase.jdbc.driver.util;

import java.util.Enumeration;
import java.util.Iterator;
import java.util.Map;
import java.util.Properties;

/**
 * key ������ ��ҹ��ڸ� �������� �ʴ� <tt>Properties</tt> Ŭ����.
 */
public class CaseInsensitiveProperties extends Properties
{
    private static final long serialVersionUID = 1720730086780858495L;

    public CaseInsensitiveProperties()
    {
    }

    public CaseInsensitiveProperties(CaseInsensitiveProperties aProp)
    {
        this.defaults = aProp;
    }

    public CaseInsensitiveProperties(Properties aProp)
    {
        if (aProp instanceof CaseInsensitiveProperties)
        {
            this.defaults = aProp;
        }
        else if (aProp != null)
        {
            // key�� lowercase�� �Ǿ��ִ� Properties ����
            Properties sProp = createProperties();
            Enumeration sKeys = aProp.keys();
            while (sKeys.hasMoreElements())
            {
                String sKey = (String) sKeys.nextElement();
                String sValue = aProp.getProperty(sKey);

                sProp.setProperty(sKey, sValue);
            }

            this.defaults = sProp;
        }
    }

    protected Properties createProperties()
    {
        return new CaseInsensitiveProperties();
    }

    public synchronized boolean containsKey(Object aKey)
    {
        return super.containsKey(caseInsentiveKey(aKey));
    }

    public synchronized Object get(Object aKey)
    {
        return super.get(caseInsentiveKey(aKey));
    }

    public synchronized Object put(Object aKey, Object aValue)
    {
        return super.put(caseInsentiveKey(aKey), aValue);
    }

    public synchronized Object remove(Object aKey)
    {
        return super.remove(caseInsentiveKey(aKey));
    }

    private String caseInsentiveKey(Object aKey)
    {
        String sCaseInsensitiveKey;
        if (aKey == null)
        {
            sCaseInsensitiveKey = null;
        }
        else
        {
            sCaseInsensitiveKey = aKey.toString().trim().toLowerCase();
        }
        return sCaseInsensitiveKey;
    }

    public synchronized void putAll(Map aMap)
    {
        if (aMap instanceof CaseInsensitiveProperties)
        {
            super.putAll(aMap);
        }
        else
        {
            Iterator sIt = aMap.keySet().iterator();
            while (sIt.hasNext())
            {
                Object sKey = sIt.next();
                put(sKey, aMap.get(sKey));
            }
        }
    }

    /**
     * Map�� �ִ� ������ Preperty�� �߰��Ѵ�.
     * ��, Map�� key ������ ���� �� �ִ� ���� �̹� �ִٸ� �߰����� �ʴ´�.
     *
     * @param aMap �߰��� �� ���� ���� ��ü
     */
    public synchronized void putAllExceptExist(Map aMap)
    {
        Iterator sIt = aMap.keySet().iterator();
        while (sIt.hasNext())
        {
            Object sKey = sIt.next();
            if (getProperty(caseInsentiveKey(sKey)) == null)
            {
                put(sKey, aMap.get(sKey));
            }
        }
    }

    public AltibaseProperties getDefaults()
    {
        return (AltibaseProperties)defaults;
    }

    @Override
    public String getProperty(String aKey)
    {
        return super.getProperty(caseInsentiveKey(aKey));
    }
}
