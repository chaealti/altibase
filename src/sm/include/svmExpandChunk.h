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
 
#ifndef _O_SVM_EXPAND_CHUNK_H_
#define _O_SVM_EXPAND_CHUNK_H_ 1


/*
 * PROJ-1490 ����������Ʈ ����ȭ�� �޸𸮹ݳ�
 * 
 * �����ͺ��̽��� Ȯ���ϴ� �ּ� ������ ExpandChunk�� ������ ��ɵ��� ���� Ŭ����
 *
 * ExpandChunk�� ������ ������ ����.
 *
 * | Free List Info Page�� | Data Page �� .... |
 *
 * Data Page���� ���̺��̳� �ε����� ���� ������ ������ ����
 * Page�� �ʿ�� �ϴ� ��ü���� �����Ͱ� ����ȴ�.
 *
 * �ϳ��� ExpandChunk�� �� �տ��� Free List Info Page�� �����Ѵ�.
 * Free List Info Page���� Data Page�� 1:1�� ���εǴ� Slot�� �����ϸ�,
 * �� Slot�� Free Page�� Next Free Page������ ��ϵȴ�.
 * Free List Info Page�� ���� ExpandChunk�ȿ� ���ϴ� Page�� ���� ���� �޶�����.
 *
 * �������, �ϳ��� Free List Info Page�� �� ���� ������ ��������
 * Next Free Page ID������ ������ �� �ִ� ��Ȳ�� �����غ���.
 * �ϳ��� ExpandChunk�� 4���� Data Page�� �����ϵ��� ����ڰ� Database �� �����ߴٸ�,
 * �ϳ��� ExpandChunk�� Free List Info Page�� 2��, �׸��� Data Page �� 4����
 * ��ϵȴ�.
 * 
 * �̷��� ExpandChunk �� �ΰ� �ִٸ�, Data Page���� Page ID�� ������ ����.
 * �Ʒ� ���ÿ��� FLI-Page �� Free List Info Page�� �ǹ��ϰ�,
 * D-Page�� Data Page�� �ǹ��Ѵ�.
 * 
 * < ExpandChunk #0 >
 *   | FLI-Page#0 | FLI-Page#1 | D-Page#2 | D-Page#3 | D-Page#4  | D-Page#5  |
 * < ExpandChunk #1 >
 *   | FLI-Page#6 | FLI-Page#7 | D-Page#8 | D-Page#9 | D-Page#10 | D-Page#11 |
 *
 * ���� �������� �ϳ��� Free List Info Page ��
 * �� Data Page�� Slot�� ����� �� �����Ƿ�,
 * D-Page #2,3 �� Next Free Page ID �� FLI-Page#0��,
 * D-Page #4,5 �� Next Free Page ID �� FLI-Page#1��,
 * D-Page #8,9 �� Next Free Page ID �� FLI-Page#6��,
 * D-Page #10,11 �� Next Free Page ID �� FLI-Page#7�� ��ϵȴ�.
 *
 * [ ���� ]
 * �� Ŭ�������� �Լ��� �����̸� �ڿ� No�� ���� ����,
 * 0���� �����Ͽ� 1�� �����ϴ� �Ϸ� ��ȣ�� �����ϸ� �ȴ�.
 */


// Free List Info(FLI) Page ���� �ϳ��� Slot�� ǥ���Ѵ�.
// FLI Page���� Slot�� �� Next Free Page�� �����ϴµ� ���ȴ�.
typedef struct svmFLISlot
{
    // �� Slot�� 1:1 ��Ī�Ǵ� Page��
    // Database Free Page�� ���   ==> ���� Free Page�� ID
    // Table Allocated Page�� ��� ==> SVM_FLI_ALLOCATED_PID
    //
    // ���� �⵿�� Restart Recovery�� �ʿ����� ���� ��쿡 ���ؼ�,
    // Free Page�� Allocated Page�� �����ϱ� ���� �뵵�� ���� �� �ִ�.
    scPageID mNextFreePageID;  
} svmFLISlot ;


// �ϳ��� Free Page�� Next Free Page ID�� ����� Slot�� ũ��
#define SVM_FLI_SLOT_SIZE  ( ID_SIZEOF(svmFLISlot) )

// Expand Chunk
class svmExpandChunk
{
private:
    // �ϳ��� Free List Info Page�� ����� �� �ִ� Slot�� ��
    static UInt  mFLISlotCount;


    // Ư�� Data Page�� ���� Expand Chunk�� �˾Ƴ���.
    static scPageID getExpandChunkPID( svmTBSNode * aTBSNode,
                                       scPageID     aDataPageID );


    // �ϳ��� Expand Chunk�ȿ��� Free List Info Page�� ������
    // ������ �������鿡 ���� ������� 0���� 1�� �����ϴ� ��ȣ�� �ű�
    // Data Page No �� �����Ѵ�.
    static vULong getDataPageNoInChunk( svmTBSNode * aTBSNode,
                                        scPageID     aDataPageID );
    
    
public :
    svmExpandChunk();
    ~svmExpandChunk();
    
        
    // Expand Chunk�����ڸ� �ʱ�ȭ�Ѵ�.
    static IDE_RC initialize(svmTBSNode * aTBSNode);


    // Ư�� Data Page�� Next Free Page ID�� ����� Slot�� �����ϴ�
    // Free List Info Page�� �˾Ƴ���.
    static scPageID getFLIPageID( svmTBSNode * aTBSNode,
                                  scPageID     aDataPageID );
    
    // FLI Page ������ Data Page�� 1:1�� ���εǴ� Slot�� offset�� ����Ѵ�.
    static UInt getSlotOffsetInFLIPage( svmTBSNode * aTBSNode,
                                        scPageID     aDataPageID );
    
    // ExpandChunk�� Page���� �����Ѵ�.
    static IDE_RC setChunkPageCnt( svmTBSNode * aTBSNode,
                                   vULong       aChunkPageCnt );
    
    // Expand Chunk�����ڸ� �ı��Ѵ�.
    static IDE_RC destroy(svmTBSNode * aTBSNode);
    
    // Next Free Page ID�� �����Ѵ�.
    // ���� Ʈ������� ���� Page�� Next Free Page ID��
    // ���ÿ� ������ ������ �ʵ��� ���ü� ��� �ؾ� �Ѵ�.
    static IDE_RC setNextFreePage( svmTBSNode * aTBSNode,
                                   scPageID     aPageID,
                                   scPageID     aNextFreePageID );
    
    // Next Free Page ID�� �����´�.
    static IDE_RC getNextFreePage( svmTBSNode * aTBSNode,
                                   scPageID     aPageID,
                                   scPageID *   aNextFreePageID );

    // �ּ��� Ư�� Page�� ��ŭ ������ �� �ִ� Expand Chunk�� ���� ����Ѵ�.
    static vULong getExpandChunkCount( svmTBSNode * aTBSNode,
                                       vULong       aRequiredPageCnt );

    // Data Page������ ���θ� �Ǵ��Ѵ�.
    static idBool isDataPageID( svmTBSNode * aTBSNode,
                                scPageID     aPageID );

    // FLI Page������ ���θ� �Ǵ��Ѵ�.
    static idBool isFLIPageID( svmTBSNode * aTBSNode, 
                               scPageID     aPageID );


    // �ϳ��� Chunk�� Free List Info Page�� ���� �����Ѵ�.
    static UInt getChunkFLIPageCnt(svmTBSNode * aTBSNode);

    // �����ͺ��̽� Page�� Free Page���� ���θ� �����Ѵ�.
    static IDE_RC isFreePageID(svmTBSNode * aTBSNode,
                               scPageID     aPageID,
                               idBool *     aIsFreePage );

    /* BUG-32461 [sm-mem-resource] add getPageState functions to 
     * smmExpandChunk module */
    static IDE_RC getPageState( svmTBSNode     * aTBSNode,
                                scPageID         aPageID,
                                svmPageState   * aPageState );
};


/* �ϳ��� Chunk�� Free List Info Page�� ���� �����Ѵ�.
 */
inline UInt svmExpandChunk::getChunkFLIPageCnt(svmTBSNode * aTBSNode)
{
    IDE_DASSERT( aTBSNode->mChunkPageCnt != 0 );
    IDE_DASSERT( aTBSNode->mChunkFLIPageCnt != 0 );
    
    return aTBSNode->mChunkFLIPageCnt;
}

#endif /* _O_SVM_EXPAND_CHUNK_H_ */

