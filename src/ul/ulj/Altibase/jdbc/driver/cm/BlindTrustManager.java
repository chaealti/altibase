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

package Altibase.jdbc.driver.cm;

import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;

import javax.net.ssl.X509TrustManager;

/**
 * PROJ-2474 Truststore �������� bypass �ϴ� simple TrustManager</br>
 * singleton �������� ������
 * @author yjpark
 *
 */
class BlindTrustManager implements X509TrustManager
{
    private static X509TrustManager mSingleton = new BlindTrustManager();

    public static X509TrustManager getInstance()
    {
        return mSingleton;
    }
    
    private BlindTrustManager() { }  
    
    public void checkClientTrusted(X509Certificate[] aChain, String aAuthType) throws CertificateException
    {
        // ������ ������ ���� �ʰ� �׳� �Ѿ��.
    }

    public void checkServerTrusted(X509Certificate[] aChain, String aAuthType) throws CertificateException
    {
        // ������ ������ ���� �ʰ� �׳� �Ѿ��.
    }

    public X509Certificate[] getAcceptedIssuers()
    {
        return null;
    }
}
