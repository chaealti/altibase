#!/bin/sh

fileName=$1  #�������� ���ϸ� $1���ڷ� ����.

count=0				#count �ʱ�ȭ
fileSize=`cat $fileName | wc -l`	#���ϻ�����
echo "fileSize : " $fileSize
loop=`expr $fileSize \* 3`			#mix Ƚ��

today=`date +"%d"`
second=`date +"%S"`
lineNum=`expr $today + $second` #������ ���� �ʱ�ȭ

while [ $count -lt $loop ]
do
	cp $fileName tempFile
	lineNum=`expr $count \* 3 + $lineNum`	#�ٲٷ��� ���μ� ���
	oddCheck=`expr $lineNum \% $fileSize \% 2`
	lineNum=`expr $lineNum \% $fileSize`
	
	if [ $lineNum -lt 2 ]
	then
		    lineNum=`expr $lineNum + 2`
	fi
		
	head -$lineNum $fileName | tail -1 >> $fileName
    sed ''$lineNum'd' $fileName > tempFile
	mv tempFile $fileName
	count=`expr $count + 1`
done

#path=`echo ${ATAF_TEST_CASE//\//\\\/}`
#sed 's/\.\./'$path'/g' $fileName > tempFile
#mv tempFile $fileName
