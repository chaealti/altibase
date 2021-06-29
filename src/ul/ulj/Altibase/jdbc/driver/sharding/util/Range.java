/*
 * Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

package Altibase.jdbc.driver.sharding.util;

import java.io.Serializable;
import java.util.Comparator;

/**
 * ������ ��Ÿ���� Ŭ����<br>
 * �� Ŭ������ ����Ϸ��� �ݵ�� Comparator�� �����ؾ� �Ѵ�.
 */
public final class Range<T> implements Serializable
{
    private static final long   serialVersionUID = 8140790962014453934L;
    private final Comparator<T> mComparator;
    private final T             mMinimum;
    private final T             mMaximum;
    private transient int       mHashCode;
    private transient String    mToString;

    /**
     * mMinimum���� mMaximum�� ���̿� �ش��ϴ� Range��ü�� �����Ѵ�.
     *
     * @param <T> ������ ���� ������Ʈ Ÿ��
     * @param aFrom ������ ������ ��Ÿ���� ��
     * @param aTo ������ ���� ��Ÿ���� ��
     * @return ���� ��ü
     * @throws IllegalArgumentException ������Ʈ�� null�� ���
     * @throws ClassCastException ������Ʈ�� Comparable�� �����ϰ� ���� ���� ���
     */
    public static <T extends Comparable<T>> Range<T> between(T aFrom, T aTo)
    {
        return between(aFrom, aTo, null);
    }

    /**
     * ù��° Range�� max���� mMaximum�� ���̿� �ش��ϴ� Range��ü�� �����Ѵ�.
     *
     * @param <T> ������ ���� ������Ʈ Ÿ��
     * @param aFrom ������ ������ ��Ÿ���� Range��ü
     * @param aTo ������ ���� ��Ÿ���� ��
     * @return ���� ��ü
     * @throws IllegalArgumentException ������Ʈ�� null�� ���
     * @throws ClassCastException ������Ʈ�� Comparable�� �����ϰ� ���� ���� ���
     */
    public static <T extends Comparable<T>> Range<T> between(Range<T> aFrom, T aTo)
    {
        if (aFrom.getMaximum() != null && aFrom.isEndedBy(aTo))
        {
            return between(aFrom.getMinimum(), aFrom.getMaximum());
        }
        return between(aFrom.getMaximum(), aTo, null);
    }

    public static Range getNullRange()
    {
        return between(null, null, null);
    }

    /**
     * �Ű������� ������ Comparator�� ����� Range��ü�� �����Ѵ�.
     *
     * @param <T> ������ ���� ������Ʈ Ÿ��
     * @param aFrom ������ ������ ��Ÿ���� ��
     * @param aTo ������ ���� ��Ÿ���� ��
     * @param aComparator ������ ���� ����� Comparator��ü, null�� ��쿡�� natural ordering�� ����Ѵ�.
     * @return ���� ��ü
     * @throws IllegalArgumentException ������Ʈ�� null�� ���
     * @throws ClassCastException ������Ʈ�� natural ordering�� ����ϰ� Comparable�� �����ϰ� ���� ���� ���.
     */
    public static <T> Range<T> between(T aFrom, T aTo, Comparator<T> aComparator)
    {
        return new Range<T>(aFrom, aTo, aComparator);
    }

    /**
     * Range��ü ����
     *
     * @param aFrom ù��° ������
     * @param aTo �ι�° ������
     * @param aComparator Comparator ��ü, null�� ��쿡�� natural ordering
     */
    @SuppressWarnings("unchecked")
    private Range(T aFrom, T aTo, Comparator<T> aComparator)
    {
        if (aComparator == null)
        {
            aComparator = ComparableComparator.INSTANCE;
        }
        if (aFrom != null)
        {
            if (aComparator.compare(aFrom, aTo) < 1)
            {
                this.mMinimum = aFrom;
                this.mMaximum = aTo;
            }
            else
            {
                this.mMinimum = aTo;
                this.mMaximum = aFrom;
            }
        }
        else
        {
            this.mMinimum = null;
            this.mMaximum = aTo;
        }

        this.mComparator = aComparator;
    }

    /**
     * ������ �ּҰ��� �����Ѵ�.
     *
     * @return ���� �ּҰ�
     */
    public T getMinimum()
    {
        return mMinimum;
    }

    /**
     * ������ �ִ밪�� �����Ѵ�.
     *
     * @return ���� �ִ밪
     */
    public T getMaximum()
    {
        return mMaximum;
    }

    /**
     * �ش� ������Ʈ�� ������ ���ϴ��� ���θ� �����Ѵ�.<br>
     * ������ �ּҰ��� �ִ밪�� �����ؼ� �Ǵ��Ѵ�.(inclusive)
     * @param aElement ���� ������Ʈ��
     * @return �ش� ������Ʈ�� ������ ���ԵǾ� �ִ��� ����
     */
    public boolean contains(T aElement)
    {
        if (aElement == null)
        {
            return false;
        }
        return (mComparator.compare(aElement, mMinimum) > -1) &&
               (mComparator.compare(aElement, mMaximum) < 1);
    }

    /**
     * �ش� ������Ʈ�� ������ ���ϴ��� ���θ� �����Ѵ�.<br>
     * ������ �ּҰ��� ������ �ʰ� �ִ밪 ���� �������� �Ǵ��Ѵ�.(exclusive)
     * @param aElement ���� ������Ʈ��
     * @return �ش� ������Ʈ�� ������ ���ԵǾ� �ִ��� ����
     */
    public boolean containsEndedBy(T aElement)
    {
        if (aElement == null)
        {
            return false;
        }

        return mComparator.compare(aElement, mMaximum) < 0;
    }

    /**
     * �ش� ������Ʈ�� ������ ���ϴ��� ���θ� �����Ѵ�.<br>
     * ������ �ּҰ����ٴ� ũ�ų� ����(inclusive) �ִ밪 ���ٴ� ������ �Ǵ��Ѵ�.(exclusive)
     * @param aElement ���� ������Ʈ��
     * @return �ش� ������Ʈ�� ������ ���ԵǾ� �ִ��� ����
     */
    public boolean containsEqualAndLessThan(T aElement)
    {
        if (aElement == null)
        {
            return false;
        }

        if (mMinimum == null) // min���� null�� ��쿡�� max�� ��
        {
            return mComparator.compare(aElement, mMaximum) < 0;
        }

        return (mComparator.compare(aElement, mMinimum) > -1) &&
               (mComparator.compare(aElement, mMaximum) < 0);
    }

    /**
     * �ش� ���� ������ �ּҰ� ���� ������ ���θ� �Ǵ��Ѵ�(exclusive).
     * @param element �˻��� ��ü
     * @return true �ش� ������Ʈ�� �������� ���� ���
     */
    public boolean isAfter(T element)
    {
        if (element == null)
        {
            return false;
        }
        return mComparator.compare(element, mMinimum) < 0;
    }

    /**
     * �ش� ��ü�� ������ ���۵Ǵ��� ���θ� �Ǵ��Ѵ�(inclusive).
     *
     * @param element �˻��� ��ü
     * @return true ������ �ش� ������ �����ϴ� ���
     */
    public boolean isStartedBy(T element)
    {
        if (element == null)
        {
            return false;
        }
        return mComparator.compare(element, mMinimum) == 0;
    }

    /**
     * �ش� ��ü�� ������ �������� ���θ� �Ǵ��Ѵ�(inclusive).
     *
     * @param aElement �˻��� ��ü
     * @return true ������ �ش� ������ ������ ���
     */
    public boolean isEndedBy(T aElement)
    {
        if (aElement == null)
        {
            return false;
        }
        return mComparator.compare(aElement, mMaximum) == 0;
    }

    /**
     * �ش� ���� ������ �ִ밪 ���� ū ���θ� �Ǵ��Ѵ�(exclusive).
     * @param element �˻��� ��ü
     * @return true �ش� ������Ʈ�� �������� ū ���
     */
    public boolean isBefore(T element)
    {
        if (element == null)
        {
            return false;
        }
        return mComparator.compare(element, mMaximum) > 0;
    }

    /**
     * ���õ� ������ Range�� ���ԵǴ��� ���θ� �����Ѵ�(inclusive).
     *
     * @param aRange üũ�� Range��ü
     * @return true üũ�� Range��ü�� ���ԵǴ� ���
     * @throws RuntimeException ������ ���� �� ���� ���
     */
    public boolean containsRange(Range<T> aRange)
    {
        if (aRange == null)
        {
            return false;
        }
        return contains(aRange.mMinimum) && contains(aRange.mMaximum);
    }

    /**
     * ������ aRange�� �������� ������ Ȯ���Ѵ�.
     *
     * @param aRange Ȯ���� ���� ����
     * @return true ������ �ּҰ��� aRange�� ���� max������ Ŭ��
     * @throws RuntimeException ������ ���� �� ���� ���
     */
    public boolean isAfterRange(Range<T> aRange)
    {
        if (aRange == null)
        {
            return false;
        }
        return isAfter(aRange.mMaximum);
    }

    /**
     * �ش� ������ �Ű������� ���� ������ ������ �Ǵ��� ���θ� �Ǵ��Ѵ�.<br>
     * (�� ������ ��� �ϳ��� ��Ұ� ���ԵǴ� ���)
     *
     * @param aOtherRange Ȯ���� ���� ��ü
     * @return true ������ ������ �ش� ������ ������ �Ǵ� ���
     * @throws RuntimeException ������ ���� �� ���� ���
     */
    public boolean isOverlappedBy(Range<T> aOtherRange)
    {
        if (aOtherRange == null)
        {
            return false;
        }
        return aOtherRange.contains(mMinimum) || aOtherRange.contains(mMaximum) ||
               contains(aOtherRange.mMinimum);
    }

    /**
     * ������ ������ �������� �����ϰ� �տ� �ִ��� ���θ� �Ǵ��Ѵ�.
     *
     * @param aOtherRange Ȯ���� ���� ��ü
     * @return true �ش� ������ ������ �������� �տ� ���� ���
     * @throws RuntimeException ������ ���� �� ���� ���
     */
    public boolean isBeforeRange(Range<T> aOtherRange)
    {
        if (aOtherRange == null)
        {
            return false;
        }
        return isBefore(aOtherRange.mMinimum);
    }

    /**
     * �ΰ��� ��ü�� �����Ϸ��� �ּҰ��� �ִ밪�� �����ؾ��ϸ� �� �� Comparator�� �������� ���õȴ�.
     *
     * @param aObject ���� ��ü
     * @return true �ΰ� ��ü�� �����Ҷ�
     */
    @Override
    public boolean equals(Object aObject)
    {
        if (aObject == this)
        {
            return true;
        }
        else if (aObject == null || aObject.getClass() != getClass())
        {
            return false;
        }
        else
        {
            @SuppressWarnings("unchecked")
            Range<T> range = (Range<T>)aObject;
            if (mMinimum == null)
            {
                return mMaximum.equals(range.mMaximum);
            }
            return mMinimum.equals(range.mMinimum) && mMaximum.equals(range.mMaximum);
        }
    }

    /**
     * ������ü�� ������ �ؽ��ڵ带 �����´�.
     *
     * @return �ؽ��ڵ� ��
     */
    @Override
    public int hashCode()
    {
        int sResult = mHashCode;
        if (mHashCode == 0)
        {
            sResult = 17;
            sResult = 37 * sResult + getClass().hashCode();
            sResult = 37 * sResult + mMinimum.hashCode();
            sResult = 37 * sResult + mMaximum.hashCode();
            mHashCode = sResult;
        }
        return sResult;
    }

    @Override
    public String toString()
    {
        String sResult = mToString;
        if (sResult == null)
        {
            StringBuilder sBuf = new StringBuilder();
            sBuf.append('[');
            sBuf.append(mMinimum);
            sBuf.append("..");
            sBuf.append(mMaximum);
            sBuf.append(']');
            sResult = sBuf.toString();
            mToString = sResult;
        }
        return sResult;
    }


    @SuppressWarnings({ "rawtypes", "unchecked" })
    private enum ComparableComparator implements Comparator
    {
        INSTANCE;
        /**
         * Comparable ���̽� ����(�̱������� �����ϱ� ���� enum���)
         *
         * @param aObj1 �񱳴�� 1
         * @param aObj2 �񱳴�� 2
         * @return negative  aObj1 < aObj2
         *         0         aObj1 = aObj2
         *         positive  aObj1 > aObj2
         */
        public int compare(Object aObj1, Object aObj2)
        {
            return ((Comparable)aObj1).compareTo(aObj2);
        }
    }

}
