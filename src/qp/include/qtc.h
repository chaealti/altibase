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
 * $Id: qtc.h 88800 2020-10-06 10:29:32Z hykim $
 *
 * Description :
 *     QP layer�� MT layer�� �߰��� ��ġ�ϴ� layer��
 *     ����� MT interface layer ������ �Ѵ�.
 *
 *     ���⿡�� QP ��ü�� ���� ���������� ���Ǵ� �Լ��� ���ǵǾ� �ִ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QTC_H_
# define _O_QTC_H_ 1

# include <mt.h>
# include <qtcDef.h>
# include <qtcRid.h>
# include <qtcCache.h>
# include <qmsParseTree.h>

class qtc
{
public:
    // Zero Dependencies�� Global�ϰ� �����Ͽ�
    // �ʿ��� ������, Zero Dependencies�� ������ �ʵ��� �Ѵ�.
    static qcDepInfo         zeroDependencies;

    static smiCallBackFunc   rangeFuncs[];
    static mtdCompareFunc    compareFuncs[];
    
    /*************************************************************
     * Meta Table ������ ���� interface
     *************************************************************/

    // Minimum KeyRange CallBack
    static IDE_RC rangeMinimumCallBack4Mtd( idBool     * aResult,
                                            const void * aRow,
                                            void       *, /* aDirectKey */
                                            UInt        , /* aDirectKeyPartialSize */
                                            const scGRID aRid,
                                            void       * aData );

    static IDE_RC rangeMinimumCallBack4GEMtd( idBool     * aResult,
                                              const void * aColVal,
                                              void       *, /* aDirectKey */
                                              UInt        , /* aDirectKeyPartialSize */
                                              const scGRID aRid,
                                              void       * aData );

    static IDE_RC rangeMinimumCallBack4GTMtd( idBool     * aResult,
                                              const void * aColVal,
                                              void       *, /* aDirectKey */
                                              UInt        , /* aDirectKeyPartialSize */
                                              const scGRID aRid,
                                              void       * aData );

    static IDE_RC rangeMinimumCallBack4Stored( idBool     * aResult,
                                               const void * aColVal,
                                               void       *, /* aDirectKey */
                                               UInt        , /* aDirectKeyPartialSize */
                                               const scGRID aRid,
                                               void       * aData );

    // Maximum KeyRange CallBack
    static IDE_RC rangeMaximumCallBack4Mtd( idBool     * aResult,
                                            const void * aRow,
                                            void       *, /* aDirectKey */
                                            UInt        , /* aDirectKeyPartialSize */
                                            const scGRID aRid,
                                            void       * aData );

    static IDE_RC rangeMaximumCallBack4LEMtd( idBool     * aResult,
                                              const void * aColVal,
                                              void       *, /* aDirectKey */
                                              UInt        , /* aDirectKeyPartialSize */
                                              const scGRID aRid,
                                              void       * aData );

    static IDE_RC rangeMaximumCallBack4LTMtd( idBool     * aResult,
                                              const void * aColVal,
                                              void       *, /* aDirectKey */
                                              UInt        , /* aDirectKeyPartialSize */
                                              const scGRID aRid,
                                              void       * aData );

    static IDE_RC rangeMaximumCallBack4Stored( idBool     * aResult,
                                               const void * aColVal,
                                               void       *, /* aDirectKey */
                                               UInt        , /* aDirectKeyPartialSize */
                                               const scGRID aRid,
                                               void       * aData );

    /* PROJ-2433 */
    static IDE_RC rangeMinimumCallBack4IndexKey( idBool     * aResult,
                                                 const void * aRow,
                                                 void       * aDirectKey,
                                                 UInt         aDirectKeyPartialSize,
                                                 const scGRID aRid,
                                                 void       * aData );

    static IDE_RC rangeMaximumCallBack4IndexKey( idBool     * aResult,
                                                 const void * aRow,
                                                 void       * aDirectKey,
                                                 UInt         aDirectKeyPartialSize,
                                                 const scGRID aRid,
                                                 void       * aData );
    /* PROJ-2433 end */
    static SInt compareMinimumLimit( mtdValueInfo * aValueInfo1,
                                     mtdValueInfo * /* aValueInfo2 */ );
    
    static SInt compareMaximumLimit4Mtd( mtdValueInfo * aValueInfo1,
                                         mtdValueInfo * /* aValueInfo2 */ );

    static SInt compareMaximumLimit4Stored( mtdValueInfo * aValueInfo1,
                                            mtdValueInfo * /* aValueInfo2 */ );

private:

    /*************************************************************
     * Conversion Node ���� �� ������ ���� interface
     *************************************************************/

    // intermediate conversion node�� ����
    static IDE_RC initConversionNodeIntermediate( mtcNode** aConversionNode,
                                                  mtcNode*  aNode,
                                                  void*     aInfo );

    static IDE_RC appendDefaultArguments( qtcNode*     aNode,
                                          mtcTemplate* /*aTemplate*/,
                                          mtcStack*    /*aStack*/,
                                          SInt         /*aRemain*/,
                                          mtcCallBack* aCallBack );

    static IDE_RC addLobValueFuncForSP( qtcNode      * aNode,
                                        mtcTemplate  * aTemplate,
                                        mtcStack     * aStack,
                                        SInt           aRemain,
                                        mtcCallBack  * aCallBack );

    /*************************************************************
     * Estimate �� ���� ���� interface
     *************************************************************/

    // Node Tree ��ü�� ���� estimate�� ������.
    static IDE_RC estimateInternal( qtcNode*     aNode,
                                    mtcTemplate* aTemplate,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    mtcCallBack* aCallBack );

    // PROJ-1762 analytic function�� ���� over���� ����
    // estimate�� ������
    static IDE_RC estimate4OverClause( qtcNode*     aNode,
                                       mtcTemplate* aTemplate,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       mtcCallBack* aCallBack );

    static IDE_RC estimateAggregation( qtcNode     * aNode,
                                       mtcCallBack * aCallBack );


    // ���� Node�� ������ �����Ͽ� �ڽ��� estimate �� ������.
    static IDE_RC estimateNode( qtcNode*     aNode,
                                mtcTemplate* aTemplate,
                                mtcStack*    aStack,
                                SInt         aRemain,
                                mtcCallBack* aCallBack );

    /*************************************************************
       TODO - estimateNode() �Լ��� private function����.
       ����� Private Function���� ������ �Լ��̴�.
       ����, qtc::XXX() �ܺο����� ȣ���ϰ� ������,
       �̴� Error �߻��� ���輺�� �����ϰ� �ִ�.
       �̹� ����ϰ� �ִٸ�, ���� �� �Լ� �� �ϳ��� ��� ��� �Ѵ�.
           - qtc::estimateNodeWithoutArgument()
           - qtc::estimateNodeWithArgument()
    *************************************************************/
    static IDE_RC estimateNode( qcStatement* aStatement,
                                       qtcNode*     aNode );

    // Constant Expression������ �˻�
    static IDE_RC isConstExpr( qcTemplate  * aTemplate,
                               qtcNode     * aNode,
                               idBool      * aResult );

    // Constant Expression�� ����
    static IDE_RC runConstExpr( qcStatement * aStatement,
                                qcTemplate  * aTemplate,
                                qtcNode     * aNode,
                                mtcTemplate * aMtcTemplate,
                                mtcStack    * aStack,
                                SInt          aRemain,
                                mtcCallBack * aCallBack,
                                qtcNode    ** aResultNode );

    // Constant Expression�� ���� ó���� ���� Constant Column������ ȸ��
    static IDE_RC getConstColumn( qcStatement * aStatement,
                                  qcTemplate  * aTemplate,
                                  qtcNode     * aNode );

    static idBool isSameModule( qtcNode* aNode1,
                                qtcNode* aNode2 );

    static idBool isSameModuleByName( qtcNode * aNode1,
                                      qtcNode * aNode2 );

    // BUG-28223 CASE expr WHEN .. THEN .. ������ expr�� subquery ���� �����߻� 
    static mtcNode* getCaseSubExpNode( mtcNode* aNode );

    // BUG-41243 Check if name-based argument passing is used
    static idBool  hasNameArgumentForPSM( qtcNode * aNode );
    
public:

    /*************************************************************
     * QP�� ���� �߰� Module
     *************************************************************/

    static mtfModule assignModule;
    static mtfModule columnModule;
    static mtfModule indirectModule;
    static mtdModule skipModule;
    static mtfModule dnf_notModule;
    static mtfModule subqueryModule;
    static mtfModule valueModule;
    static mtfModule passModule;
    static mtfModule hostConstantWrapperModule;
    static mtfModule subqueryWrapperModule;

    /*************************************************************
     * Stored Procedure �� ���� �߰� Module
     *************************************************************/

    static mtfModule spFunctionCallModule;
    static mtdModule spCursorModule;
    static mtfModule spCursorFoundModule;
    static mtfModule spCursorNotFoundModule;
    static mtfModule spCursorIsOpenModule;
    static mtfModule spCursorIsNotOpenModule;
    static mtfModule spCursorRowCountModule;
    static mtfModule spSqlCodeModule;
    static mtfModule spSqlErrmModule;

    // PROJ-1075
    // row/record/array data type ��� �߰�.
    static mtdModule spRowTypeModule;
    static mtdModule spRecordTypeModule;
    static mtdModule spArrTypeModule;

    // PROJ-1386
    // REF CURSOR type ��� �߰�.
    static mtdModule spRefCurTypeModule;

    /*************************************************************
     * Tuple Set�� Tuple ������ ���� Flag ����
     *************************************************************/

    static const ULong templateRowFlags[MTC_TUPLE_TYPE_MAXIMUM];

    // BUG-44382 clone tuple ���ɰ���
    static void setTupleColumnFlag( mtcTuple * aTuple,
                                    idBool     aCopyColumn,
                                    idBool     aMemsetRow );

    /*************************************************************
     * Dependencies ���� �Լ�
     *************************************************************/

    static void dependencySet( UShort aTupleID,
                               qcDepInfo * aOperand1 );

    // PROJ-1502 PARTITIONED DISK TABLE
    // partition�� ���� selection graph��
    // partition tuple�� ���� dependency�� ��������
    // �����ؾ� �Ѵ�.
    static void dependencyChange( UShort aSourceTupleID,
                                  UShort aDestTupleID,
                                  qcDepInfo * aOperand1,
                                  qcDepInfo * aResult );

    // dependencies�� ������.
    static void dependencySetWithDep( qcDepInfo * aOperand1,
                                      qcDepInfo * aOperand2 );

    // AND dependencies�� ����
    static void dependencyAnd( qcDepInfo * aOperand1,
                               qcDepInfo * aOperand2,
                               qcDepInfo * aResult );

    // OR dependencies�� ����.
    static IDE_RC dependencyOr( qcDepInfo * aOperand1,
                                qcDepInfo * aOperand2,
                                qcDepInfo * aResult );

    // PROJ-1413
    // aOperand1���� aTupleID�� dependency�� ����
    static void dependencyRemove( UShort      aTupleID,
                                  qcDepInfo * aOperand1,
                                  qcDepInfo * aResult );
    
    // dependencies�� �ʱ�ȭ��.
    static void dependencyClear( qcDepInfo * aOperand1 );

    // dependencies�� ������ ���� �Ǵ�
    static idBool dependencyEqual( qcDepInfo * aOperand1,
                                   qcDepInfo * aOperand2 );

    // dependencies�� �����ϴ� ���� �Ǵ�
    static idBool haveDependencies( qcDepInfo * aOperand1 );

    // dependencies�� ���ԵǴ����� ���θ� �Ǵ�
    static idBool dependencyContains( qcDepInfo * aSubject,
                                      qcDepInfo * aTarget );

    // MINUS dependencies�� ����
    static void dependencyMinus( qcDepInfo * aOperand1,
                                 qcDepInfo * aOperand2,
                                 qcDepInfo * aResult );

    // Dependencies�� ���Ե� Dependency ���� ȹ��
    static SInt getCountBitSet( qcDepInfo * aOperand1 );

    // Dependencies�� ���Ե� Join Operator ���� ȹ��
    static SInt getCountJoinOperator( qcDepInfo * aOperand );

    // Dependencies�� �����ϴ� ���� depenency�� ȹ���Ѵ�.
    static SInt getPosFirstBitSet( qcDepInfo * aOperand1 );

    // ���� ��ġ�κ��� ������ �����ϴ� dependency�� ȹ���Ѵ�.
    static SInt getPosNextBitSet( qcDepInfo * aOperand1,
                                  UInt   aPos );

    // Set If Join Operator Exists
    static void dependencyAndJoinOperSet( UShort      aTupleID,
                                          qcDepInfo * aOperand1 );

    // AND of (Dependencies & Join Oper)�� ����
    static void dependencyJoinOperAnd( qcDepInfo * aOperand1,
                                       qcDepInfo * aOperand2,
                                       qcDepInfo * aResult );

    // Dependencies & Join Operator Status �� ��� ��Ȯ�� ������ ���� �Ǵ�
    static idBool dependencyJoinOperEqual( qcDepInfo * aOperand1,
                                           qcDepInfo * aOperand2 );

    // dep ������ join operator �� ������ �ݴ��� dep �� ����
    static void getJoinOperCounter( qcDepInfo  * aOperand1,
                                    qcDepInfo  * aOperand2 );

    // �ϳ��� table �� ���� join operator �� both �� ��
    static idBool isOneTableOuterEachOther( qcDepInfo   * aDepInfo );

    // dependency table tuple id ���� ����
    static UInt getDependTable( qcDepInfo  * aDepInfo,
                                UInt         aIdx );

    // dependency join operator ���� ����
    static UChar getDependJoinOper( qcDepInfo  * aDepInfo,
                                    UInt         aIdx );

    /*************************************************************
     * ��Ÿ ���� �Լ�
     *************************************************************/

    // CallBack ������ �̿��� Memory �Ҵ� �Լ�
    static IDE_RC alloc( void* aInfo,
                         UInt  aSize,
                         void** aMemPtr);

    // PROJ-1362
    static IDE_RC getOpenedCursor( mtcTemplate*     aTemplate,
                                   UShort           aTableID,
                                   smiTableCursor** aCursor,
                                   UShort         * aOrgTableID,
                                   idBool*          aFound );

    // BUG-40427 ���ʷ� Open�� LOB Cursor�� ���, qmcCursor�� ���
    static IDE_RC addOpenedLobCursor( mtcTemplate  * aTemplate,
                                      smLobLocator   aLocator );

    /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
    static idBool isBaseTable( mtcTemplate * aTemplate,
                               UShort        aTable );

    static IDE_RC closeLobLocator( smLobLocator  aLocator );

    //PROJ-1583 large geometry 
    static UInt   getSTObjBufSize( mtcCallBack * aCallBack );

    // ���ο� Tuple Row�� �Ҵ�޴´�.
    static IDE_RC nextRow( iduVarMemList * aMemory, //fix PROJ-1596
                           qcStatement   * aStatement,
                           qcTemplate    * aTemplate,
                           ULong           aFlag );

    // PROJ-1583 large geometry
    static IDE_RC nextLargeConstColumn( iduVarMemList * aMemory,
                                        qtcNode       * aNode,
                                        qcStatement   * aStatement,
                                        qcTemplate    * aTemplate,
                                        UInt            aSize );

    // ���ο� Column�� �Ҵ�޴´�.
    static IDE_RC nextColumn( iduVarMemList * aMemory, //fix PROJ-1596
                              qtcNode       * aNode,
                              qcStatement   * aStatement,
                              qcTemplate    * aTemplate,
                              ULong           aFlag,
                              UInt            aColumns );

    // Table�� ���� ������ �Ҵ�޴´�.
    static IDE_RC nextTable( UShort          *aRow,
                             qcStatement     *aStatement,
                             qcmTableInfo    *aTableInfo,
                             idBool           aIsDiskTable,
                             UInt             aIsNullableFlag); // PR-13597

    // PROJ-1358 Internal Tuple�� ������ Ȯ���Ѵ�.
    static IDE_RC increaseInternalTuple ( qcStatement * aStatement,
                                          UShort        aIncreaseCount );

    // PROJ-1473 Tuple�� columnLocate����� ���� �ʱ�ȭ
    static IDE_RC allocAndInitColumnLocateInTuple( iduVarMemList * aMemory,
                                                   mtcTemplate   * aTemplate,
                                                   UShort          aRowID );
    
    /*************************************************************
     * Filter ó�� �Լ�
     *************************************************************/

    // Filter ó���� ���� CallBack �Լ��� �⺻ ����
    static IDE_RC smiCallBack( idBool       * aResult,
                               const void   * aRow,
                               void         *, /* aDirectKey */
                               UInt          , /* aDirectKeyPartialSize */
                               const scGRID   aRid,
                               void         * aData );

    // Filter ó���� ���� CallBack���� CallBack�� AND ó���� ����
    static IDE_RC smiCallBackAnd( idBool       * aResult,
                                  const void   * aRow,
                                  void         *, /* aDirectKey */
                                  UInt          , /* aDirectKeyPartialSize */
                                  const scGRID   aRid,
                                  void         * aData );

    // �ϳ��� Filter�� ���� CallBack������ ����
    static void setSmiCallBack( qtcSmiCallBackData* aData,
                                qtcNode*            aNode,
                                qcTemplate*         aTemplate,
                                UInt                aTable );

    // �������� Filter CallBack�� ����
    static void setSmiCallBackAnd( qtcSmiCallBackDataAnd* aData,
                                   qtcSmiCallBackData*    aArgu1,
                                   qtcSmiCallBackData*    aArgu2,
                                   qtcSmiCallBackData*    aArgu3 );

    /*************************************************************
     * STRING Value ���� �Լ�
     *************************************************************/

    // �־��� String���κ��� CHAR type value�� ����
    static void setCharValue( mtdCharType* aValue,
                              mtcColumn*   aColumn,
                              SChar*       aString );

    // �־��� String���κ��� VARCHAR type value�� ����
    static void setVarcharValue( mtdCharType* aValue,
                                 mtcColumn*   aColumn,
                                 SChar*       aString,
                                 SInt         aLength );

    /*************************************************************
     * Meta�� ���� Key Range ���� �Լ�
     *************************************************************/

    // Meta KeyRange�� �ʱ�ȭ
    static void initializeMetaRange( smiRange * aRange,
                                     UInt       aCompareType );

    // �� �÷��� ���� Meta KeyRange�� ������.
    static void setMetaRangeColumn( qtcMetaRangeColumn* aRangeColumn,
                                    const mtcColumn*    aColumnDesc,
                                    const void*         aValue,
                                    UInt                aOrder,
                                    UInt                aColumnIdx );

    // PROJ-1502 PARTITIONED DISK TABLE
    // keyrange column�� �����Ѵ�.
    static void changeMetaRangeColumn( qtcMetaRangeColumn* aRangeColumn,
                                       const mtcColumn*    aColumnDesc,
                                       UInt                aColumnIdx );

    static void setMetaRangeIsNullColumn(qtcMetaRangeColumn* aRangeColumn,
                                         const mtcColumn*    aColumnDesc,
                                         UInt                aColumnIdx );

    // �� �÷��� ���� KeyRange�� ��ü KeyRange�� �߰���.
    static void addMetaRangeColumn( smiRange*           aRange,
                                    qtcMetaRangeColumn* aRangeColumn );

    // ��� Meta KeyRange �Ϸ� �� ó���� ������.
    static void fixMetaRange( smiRange* aRange );

    /*************************************************************
     * Parsing �ܰ迡���� ó��
     *************************************************************/

    // Parsing �Ϸ� ���� ó���� ����
    static IDE_RC fixAfterParsing( qcTemplate    * aTemplate );


    // Column�� Data Type ������ ������.
    static IDE_RC createColumn( qcStatement*    aStatement,
                                qcNamePosition* aPosition,
                                mtcColumn**     aColumn,
                                UInt            aArguments,
                                qcNamePosition* aPrecision,
                                qcNamePosition* aScale,
                                SInt            aScaleSign,
                                idBool          aIsForSP );

    // Column�� Data Type ������ ������(Module������ �״�� �̿���).
    static IDE_RC createColumn( qcStatement*    aStatement,
                                mtdModule  *    aMtdModule,
                                mtcColumn**     aColumn,
                                UInt            aArguments,
                                qcNamePosition* aPrecision,
                                qcNamePosition* aScale,
                                SInt            aScaleSign );

    static IDE_RC createColumn4TimeStamp( qcStatement*    aStatement,
                                          mtcColumn**     aColumn );

    /* PROJ-2422 srid */
    static IDE_RC changeColumn4SRID( qcStatement             * aStatement,
                                     struct qdExtColumnAttr  * aColumnAttr,
                                     mtcColumn               * aColumn );
    
    // PROJ-2002 Column Security
    // encrypt column������ ��ȯ
    static IDE_RC changeColumn4Encrypt( qcStatement             * aStatement,
                                        struct qdExtColumnAttr  * aColumnAttr,
                                        mtcColumn               * aColumn );
    
    // decrypt column������ ��ȯ
    static IDE_RC changeColumn4Decrypt( mtcColumn    * aColumn );
 
    /*  PROJ-1584 DML Return Clause */
    static IDE_RC changeColumn4Decrypt( mtcColumn    * aSrcColumn,
                                        mtcColumn    * aDestColumn );
    
    // OR Node�� ������
    static IDE_RC addOrArgument( qcStatement* aStatement,
                                 qtcNode**    aNode,
                                 qtcNode**    aArgument1,
                                 qtcNode**    aArgument2 );

    // AND Node�� ������
    static IDE_RC addAndArgument( qcStatement* aStatement,
                                  qtcNode**    aNode,
                                  qtcNode**    aArgument1,
                                  qtcNode**    aArgument2 );

    // BUG-36125 LNNVL�� ó����
    static IDE_RC lnnvlNode( qcStatement* aStatement,
                             qtcNode*     aNode );

    // NOT�� ó���ϱ� ���� ���� Node���� Counter �����ڷ� ��ü��.
    static IDE_RC notNode( qcStatement* aStatement,
                           qtcNode**    aNode );

    // PRIOR keyword�� ó����.
    static IDE_RC priorNode( qcStatement* aStatement,
                             qtcNode**    aNode );

    // String���κ��� �����ڸ� ���� Node�� ������.
    static IDE_RC makeNode( qcStatement*    aStatement,
                            qtcNode**       aNode,
                            qcNamePosition* aPosition,
                            const UChar*    aOperator,
                            UInt            aOperatorLength );

    // PROJ-1075 array type member function�� ����� makeNode
    static IDE_RC makeNodeForMemberFunc( qcStatement     * aStatement,
                                         qtcNode        ** aNode,
                                         qcNamePosition  * aPositionStart,
                                         qcNamePosition  * aPositionEnd,
                                         qcNamePosition  * aUserNamePos,
                                         qcNamePosition  * aTableNamePos,
                                         qcNamePosition  * aColumnNamePos,
                                         qcNamePosition  * aPkgNamePos );

    // Module �����κ��� Node�� ������.
    static IDE_RC makeNode( qcStatement*    aStatement,
                            qtcNode**       aNode,
                            qcNamePosition* aPosition,
                            mtfModule*      aModule );

    // Node�� Argument�� �߰���.
    static IDE_RC addArgument( qtcNode** aNode,
                               qtcNode** aArgument );
    
    // Node�� within argument�� �߰���.
    static IDE_RC addWithinArguments( qcStatement  * aStatement,
                                      qtcNode     ** aNode,
                                      qtcNode     ** aArguments );
    
    // Quantify Expression�� ���� ������ �׿� Node�� ������
    static IDE_RC modifyQuantifiedExpression( qcStatement* aStatement,
                                              qtcNode**    aNode );
    // Value�� ���� Node�� ������.
    static IDE_RC makeValue( qcStatement*    aStatement,
                             qtcNode**       aNode,
                             const UChar*    aTypeName,
                             UInt            aTypeLength,
                             qcNamePosition* aPosition,
                             const UChar*    aValue,
                             UInt            aValueLength,
                             mtcLiteralType  aLiteralType );

    // BUG-38952 type null
    static IDE_RC makeNullValue( qcStatement     * aStatement,
                                 qtcNode        ** aNode,
                                 qcNamePosition  * aPosition );

    // Procedure ������ ���� Node�� ������.
    static IDE_RC makeProcVariable( qcStatement*     aStatement,
                                    qtcNode**        aNode,
                                    qcNamePosition*  aPosition,
                                    mtcColumn*       aColumn,
                                    UInt             aProcVarOp );

    // To Fix PR-11391
    // Internal Procedure ������ ���� Node�� ������.
    static IDE_RC makeInternalProcVariable( qcStatement*    aStatement,
                                            qtcNode**       aNode,
                                            qcNamePosition* aPosition,
                                            mtcColumn*      aColumn,
                                            UInt            aProcVarOp );

    // Host ������ ���� Node�� ������.
    static IDE_RC makeVariable( qcStatement*    aStatement,
                                qtcNode**       aNode,
                                qcNamePosition* aPosition );

    // Column�� ���� Node�� ������.
    static IDE_RC makeColumn( qcStatement*    aStatement,
                              qtcNode**       aNode,
                              qcNamePosition* aUserPosition,
                              qcNamePosition* aTablePosition,
                              qcNamePosition* aColumnPosition,
                              qcNamePosition* aPkgPosition );

    // Asterisk Target Column�� ���� Node�� ������.
    static IDE_RC makeTargetColumn( qtcNode* aNode,
                                    UShort   aTable,
                                    UShort   aColumn );

    // �ش� Tuple�� �ִ� Offset�� �������Ѵ�.
    static void resetTupleOffset( mtcTemplate* aTemplate, UShort aTupleID );

    // Intermediate Tuple�� ���� ������ �Ҵ�
    static IDE_RC allocIntermediateTuple( qcStatement* aStatement,
                                          mtcTemplate* aTemplate,
                                          UShort       aTupleID,
                                          SInt         aColCount );

    // List Expression�� ���� ������ �������Ѵ�.
    static IDE_RC changeNode( qcStatement*    aStatement,
                              qtcNode**       aNode,
                              qcNamePosition* aPosition,
                              qcNamePosition* aPositionEnd );

    // Module�� ���� List Expression�� ���� ������ �������Ѵ�.
    static IDE_RC changeNodeByModule( qcStatement*    aStatement,
                                      qtcNode**       aNode,
                                      mtfModule*      aModule,
                                      qcNamePosition* aPosition,
                                      qcNamePosition* aPositionEnd );

    // PROJ-2533 array ������ ����� changeNode
    static IDE_RC changeNodeForArray( qcStatement*    aStatement,
                                      qtcNode**       aNode,
                                      qcNamePosition* aPosition,
                                      qcNamePosition* aPositionEnd,
                                      idBool          aIsBracket );

    // PROJ-1075 array type member function�� ����� changeNode
    static IDE_RC changeNodeForMemberFunc( qcStatement    * aStatement,
                                           qtcNode       ** aNode,
                                           qcNamePosition * aPositionStart,
                                           qcNamePosition * aPositionEnd,
                                           qcNamePosition * aUserNamePos,
                                           qcNamePosition * aTableNamePos,
                                           qcNamePosition * aColumnNamePos,
                                           qcNamePosition * aPkgNamePos,
                                           idBool           aIsBracket /* PROJ-2533 */ );
    // PROJ-2533 column�� ���� changeNode
    static IDE_RC changeColumn( qtcNode        ** aNode,
                                qcNamePosition  * aPositionStart,
                                qcNamePosition  * aPositionEnd,
                                qcNamePosition  * aUserNamePos,
                                qcNamePosition  * aTableNamePos,
                                qcNamePosition  * aColumnNamePos,
                                qcNamePosition  * aPkgNamePos);

    /* BUG-40279 lead, lag with ignore nulls */
    static IDE_RC changeIgnoreNullsNode( qtcNode     * aNode,
                                         idBool      * aChanged );

    // BUG-41631 RANK() Within Group Aggregation.
    static IDE_RC changeWithinGroupNode( qcStatement * aStatement,
                                         qtcNode     * aNode,
                                         qtcOver     * aOver );

    // BUG-41631 RANK() Within Group Aggregation.
    static idBool canHasOverClause( qtcNode * aNode );

    // BIGINT Value�� �����Ѵ�.
    static IDE_RC getBigint( SChar*          aStmtText,
                             SLong*          aValue,
                             qcNamePosition* aPosition );

    /*************************************************************
     * Validation �ܰ迡���� ó��
     *************************************************************/

    // SET�� Target���� ǥ���� ���� internal column�� ������.
    static IDE_RC makeInternalColumn( qcStatement* aStatement,
                                      UShort       aTable,
                                      UShort       aColumn,
                                      qtcNode**    aNode);

    // Invalid View�� ���� ó��
    static void fixAfterValidationForCreateInvalidView (
        qcTemplate* aTemplate );

    // Validation �Ϸ� ���� ó��
    static IDE_RC fixAfterValidation( iduVarMemList * aMemory, //fix PROJ-1596
                                      qcTemplate    * aTemplate );

    // Argument�� ������� ���� estimate ó��
    static IDE_RC estimateNodeWithoutArgument( qcStatement* aStatement,
                                               qtcNode*     aNode );

    // �ٷ� ���� Arguemnt���� ����� estimate ó��
    static IDE_RC estimateNodeWithArgument( qcStatement* aStatement,
                                            qtcNode*     aNode );

    // PROJ-1718 Window function�� ���� estimate ó��
    static IDE_RC estimateWindowFunction( qcStatement * aStatement,
                                          qmsSFWGH    * aSFWGH,
                                          qtcNode     * aNode );

    // ��ü Node Tree�� Traverse�ϸ� estimate ó��
    static IDE_RC estimate( qtcNode*     aNode,
                            qcTemplate*  aTemplate,
                            qcStatement* aStatement,
                            qmsQuerySet* aQuerySet,
                            qmsSFWGH*    aSFWGH,
                            qmsFrom*     aFrom );

    /* PROJ-1353 */
    static IDE_RC estimateNodeWithSFWGH( qcStatement * aStatement,
                                         qmsSFWGH    * aSFWGH,
                                         qtcNode     * aNode );

    // Conversion Node�� �����Ѵ�.
    static IDE_RC makeConversionNode( qtcNode*         aNode,
                                      qcStatement*     aStatement,
                                      qcTemplate*      aTemplate,
                                      const mtdModule* aModule );

    // ORDER BY indicator�� ���� ������ ������.
    static IDE_RC getSortColumnPosition( qmsSortColumns* aSortColumn,
                                         qcTemplate*     aTemplate );

    // �� qtcNode�� ���� expression�� ǥ���ϴ��� �˻�
    static IDE_RC isEquivalentExpression( qcStatement     * aStatement,
                                          qtcNode         * aNode1,
                                          qtcNode         * aNode2,
                                          idBool          * aIsTrue);

    // �� predicate�� ���� predicate���� �˻�
    static IDE_RC isEquivalentPredicate( qcStatement * aStatement,
                                         qtcNode     * aPredicate1,
                                         qtcNode     * aPredicate2,
                                         idBool      * aResult );

    static IDE_RC isEquivalentExpressionByName( qtcNode * aNode1,
                                                qtcNode * aNode2,
                                                idBool  * aIsEquivalent );

    // column node�� stored function node�� ��ȯ
    static IDE_RC changeNodeFromColumnToSP( qcStatement * aStatement,
                                            qtcNode     * aNode,
                                            mtcTemplate * aTemplate,
                                            mtcStack    * aStack,
                                            SInt          aRemain,
                                            mtcCallBack * aCallBack,
                                            idBool      * aFindColumn );

    // BUG-16000
    // Equal������ ������ Ÿ������ �˻�
    static idBool isEquiValidType( qtcNode     * aNode,
                                   mtcTemplate * aTemplate );

    // PROJ-1413
    static IDE_RC registerTupleVariable( qcStatement    * aStatement,
                                         qcNamePosition * aPosition );

    // Constant Expression�� ���� ó��
    static IDE_RC preProcessConstExpr( qcStatement * aStatement,
                                       qtcNode     * aNode,
                                       mtcTemplate * aTemplate,
                                       mtcStack    * aStack,
                                       SInt          aRemain,
                                       mtcCallBack * aCallBack );

    // column ���� ��ġ ���� ( Target, Group By, Having ... )
    static IDE_RC setColumnExecutionPosition( mtcTemplate * aTemplate,
                                              qtcNode     * aNode,
                                              qmsSFWGH    * aColumnSFWGH,
                                              qmsSFWGH    * aCurrentSFWGH );

    /* PROJ-2462 ResultCache */
    static void checkLobAndEncryptColumn( mtcTemplate * aTemplate,
                                          mtcNode     * aNode );

    // BUG-45745
    static idBool isSameType( mtcColumn * aColumn1,
                              mtcColumn * aColumn2 );
    
    /*************************************************************
     * Optimization �ܰ迡���� ó��
     *************************************************************/

    // Optimization �Ϸ� ���� ó��
    static IDE_RC fixAfterOptimization( qcStatement * aStatement );
    
    // PRIOR Node�� Table ID�� ������.
    static IDE_RC priorNodeSetWithNewTuple( qcStatement* aStatement,
                                            qtcNode**    aNode,
                                            UShort       aOriginalTable,
                                            UShort       aTable );

    // Assign�� ���� Node�� ������.
    static IDE_RC makeAssign( qcStatement * aStatement,
                              qtcNode     * aNode,
                              qtcNode     * aArgument );

    // Indirect ó���� ���� Node�� ������.
    static IDE_RC makeIndirect( qcStatement* aStatement,
                                qtcNode*     aNode,
                                qtcNode*     aArgument );

    // Pass Node�� ������
    static IDE_RC makePassNode( qcStatement * aStatement,
                                qtcNode     * aCurrentNode,
                                qtcNode     * aArgumentNode,
                                qtcNode    ** aPassNode );

    // Constant Wrapper Node�� ������.
    static IDE_RC makeConstantWrapper( qcStatement * aStatement,
                                       qtcNode     * aNode );

    // Node Tree������ Host Constant Expression�� ã��,
    // �̿� ���Ͽ� Constant Wrapper Node�� ������.
    static IDE_RC optimizeHostConstExpression( qcStatement * aStatement,
                                               qtcNode     * aNode );

    // Subquery Wrapper Node�� ������.
    static IDE_RC makeSubqueryWrapper( qcStatement * aStatement,
                                       qtcNode     * aSubqueryNode,
                                       qtcNode    ** aWrapperNode );

    // PROJ-1502 PARTITIONED DISK TABLE
    // Node Tree�� �����ϸ鼭 partitioned table�� ��带
    // partition�� ���� ������.
    static IDE_RC cloneQTCNodeTree4Partition(
        iduVarMemList * aMemory,
        qtcNode*        aSource,
        qtcNode**       aDestination,
        UShort          aSourceTable,
        UShort          aDestTable,
        idBool          aContainRootsNext );

    // Node Tree�� ������.
    static IDE_RC cloneQTCNodeTree( iduVarMemList * aMemory, //fix PROJ-1596
                                    qtcNode       * aSource,
                                    qtcNode      ** aDestination,
                                    idBool          aContainRootsNext,
                                    idBool          aConversionClear,
                                    idBool          aConstCopy,
                                    idBool          aConstRevoke );

    static IDE_RC copyNodeTree( qcStatement *   aStatement,
                                qtcNode*        aSource,
                                qtcNode**       aDestination,
                                idBool          aContainRootsNext,
                                idBool          aSubqueryCopy );

    static IDE_RC copySubqueryNodeTree( qcStatement  * aStatement,
                                        qtcNode      * aSource,
                                        qtcNode     ** aDestination );
    
    // DNF Filter�� ���� �ʿ��� Node�� ������.
    static IDE_RC copyAndForDnfFilter( qcStatement* aStatement,
                                       qtcNode*     aSource,
                                       qtcNode**    aDestination,
                                       qtcNode**    aLast );

    // PROJ-1486
    // �� Node�� ���� ����� Data Type�� ��ġ��Ŵ
    static IDE_RC makeSameDataType4TwoNode( qcStatement * aStatement,
                                            qtcNode     * aNode1,
                                            qtcNode     * aNode2 );

    // PROJ-2582 recursive with
    static IDE_RC makeLeftDataType4TwoNode( qcStatement * aStatement,
                                            qtcNode     * aNode1,
                                            qtcNode     * aNode2 );

    // PROJ-2582 recursive with
    static IDE_RC makeRecursiveTargetInfo( qcStatement * aStatement,
                                           qtcNode     * aWithNode,
                                           qtcNode     * aViewNode,
                                           qcmColumn   * aColumnInfo );
    
    // template�� ���� dataSize���� ������ ���ο� size��ŭ �����Ѵ�.
    static IDE_RC getDataOffset( qcStatement * aStatement,
                                 UInt          aSize,
                                 UInt        * aOffsetPtr );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // qcStatement�� scan decision factor�� add, remove �ϴ� �Լ���
    static IDE_RC addSDF( qcStatement           * aStatement,
                          qmoScanDecisionFactor * aSDF );

    static IDE_RC removeSDF( qcStatement           * aStatement,
                             qmoScanDecisionFactor * aSDF );

    // PROJ-1405 ROWNUM
    // ������ constant value�� ���� �����´�.
    static idBool getConstPrimitiveNumberValue( qcTemplate  * aTemplate,
                                                qtcNode     * aNode,
                                                SLong       * aNumberValue );

    /*************************************************************
     * Binding �ܰ迡���� ó��
     *************************************************************/

    // Binding ���� ó�� �۾�
    static IDE_RC setVariableTupleRowSize( qcTemplate    * aTemplate );

    // aNode�� ��� node tree���� �ƴ��� �˾ƿ´�. (ORDER BY��)
    static idBool isConstNode4OrderBy( qtcNode * aNode );

    // BUG-25594
    // aNode�� ��� node tree���� �ƴ��� �˾ƿ´�. (like pattern��)
    static idBool isConstNode4LikePattern( qcStatement * aStatement,
                                           qtcNode     * aNode,
                                           qcDepInfo   * aOuterDependencies );

    /*************************************************************
     * Execution �ܰ迡���� ó��
     *************************************************************/

    // Aggregation Node�� �ʱ�ȭ
    static IDE_RC initialize( qtcNode*    aNode,
                              qcTemplate* aTemplate );

    // Aggregation Node�� ���� ����
    static IDE_RC aggregate( qtcNode*    aNode,
                             qcTemplate* aTemplate );

    // Aggregation Node�� ���� ����
    static IDE_RC aggregateWithInfo( qtcNode*    aNode,
                                     void*       aInfo,
                                     qcTemplate* aTemplate );

    // Aggregation Node�� ���� �۾� ����
    static IDE_RC finalize( qtcNode*    aNode,
                            qcTemplate* aTemplate );

    // �ش� Node�� ������ ����
    static IDE_RC calculate( qtcNode*    aNode,
                             qcTemplate* aTemplate );

    // �ش� Node�� ���� ����� ������ �Ǵ�
    static IDE_RC judge( idBool*     aResult,
                         qtcNode*    aNode,
                         qcTemplate* aTemplate );

    /* PROJ-2209 DBTIMEZONE */
    static IDE_RC setDatePseudoColumn( qcTemplate * aTemplate );

    // ���� ������ System Time�� ȹ����. fix BUG-14394
    static IDE_RC sysdate( mtdDateType* aDate );

    // Subquery Node�� Plan �ʱ�ȭ
    static IDE_RC initSubquery( mtcNode*     aNode,
                                mtcTemplate* aTemplate );

    // Subquery Node�� Plan ����
    static IDE_RC fetchSubquery( mtcNode*     aNode,
                                 mtcTemplate* aTemplate,
                                 idBool*      aRowExist );

    // Subquery Node�� Plan ����
    static IDE_RC finiSubquery( mtcNode*     aNode,
                                mtcTemplate* aTemplate );

    /* PROJ-2248 Subquery caching */
    static IDE_RC setCalcSubquery( mtcNode     * aNode,
                                   mtcTemplate * aTemplate );

    static IDE_RC setSubAggregation( qtcNode * aNode );

    // PROJ-1502 PARTITIONED DISK TABLE
    static IDE_RC estimateConstExpr( qtcNode     * aNode,
                                     qcTemplate  * aTemplate,
                                     qcStatement * aStatement );

    // PROJ-1502 PARTITIONED DISK TABLE
    static idBool isConstValue( qcTemplate  * aTemplate,
                                qtcNode     * aNode );

    static idBool isHostVariable( qcTemplate  * aTemplate,
                                  qtcNode     * aNode );
    
    // BUG-15746
    // ColumnNode�� ���� Į�� ������ ȹ���Ͽ�
    // BindNode�� �����Ǵ� Bind Param ������ ������
    // ( setDescribeParamInfo4Where()���� �� �Լ��� ȣ�� ) 
    static IDE_RC setBindParamInfoByNode( qcStatement * aStatement,
                                          qtcNode     * aColumnNode,
                                          qtcNode     * aBindNode );
    
    // where������ (Į��)(�񱳿�����)(bind) ���¸� ã��
    // bindParamInfo�� Į�� ������ ������ 
    static IDE_RC setDescribeParamInfo4Where( qcStatement * aStatement,
                                              qtcNode     * aNode );

    // BUG-34231
    static idBool isQuotedName( qcNamePosition * aPosition );

    /* PROJ-2208 Multi Currency */
    static IDE_RC getNLSCurrencyCallback( mtcTemplate * aTemplate,
                                          mtlCurrency * aCurrency );

    // loop count
    static IDE_RC getLoopCount( qcTemplate  * aTemplate,
                                qtcNode     * aLoopNode,
                                SLong       * aCount );

    static IDE_RC addKeepArguments( qcStatement  * aStatement,
                                    qtcNode     ** aNode,
                                    qtcKeepAggr  * aKeep );

    static IDE_RC changeKeepNode( qcStatement  * aStatement,
                                  qtcNode     ** aNode );

    /* PROJ-2632 */
    static IDE_RC estimateSerializeFilter( qcStatement          * aStatement,
                                           mtcNode              * aNode,
                                           UInt                 * aCount );

    static IDE_RC allocateSerializeFilter( qcStatement          * aStatement,
                                           UInt                   aCount,
                                           mtxSerialFilterInfo ** aInfo );

    static IDE_RC recursiveSerializeFilter( qcStatement          * aStatement,
                                            mtcNode              * aNode,
                                            mtxSerialFilterInfo  * aFense,
                                            mtxSerialFilterInfo ** aInfo );

    static IDE_RC recursiveConversionSerializeFilter( qcStatement          * aStatement,
                                                      mtcNode              * aNode,
                                                      mtxSerialFilterInfo  * aFense,
                                                      mtxSerialFilterInfo ** aInfo );

    static IDE_RC checkSerializeFilterType( qcTemplate           * aTemplate,
                                            mtcNode              * aNode,
                                            mtcNode             ** aArgument,
                                            mtxSerialExecuteFunc * aExecute,
                                            UChar                * aEntryType );

    static IDE_RC checkNodeModuleForNormal( qcTemplate           * aTemplate,
                                            mtcNode              * aNode,
                                            mtcNode             ** aArgument,
                                            mtxSerialExecuteFunc * aExecute,
                                            UChar                * aEntryType );

    static IDE_RC checkNodeModuleForEtc( qcTemplate           * aTemplate,
                                         mtcNode              * aNode,
                                         mtcNode             ** aArgument,
                                         mtxSerialExecuteFunc * aExecute,
                                         UChar                * aEntryType );

    static IDE_RC checkConversionSerializeFilterType( qcTemplate           * aTemplate,
                                                      mtcNode              * aNode,
                                                      mtcNode              * aArgument,
                                                      mtxSerialExecuteFunc * aExecute,
                                                      UChar                * aEntryType );

    static IDE_RC adjustSerializeFilter( UChar                  aType,
                                         mtxSerialFilterInfo  * aPrev,
                                         mtxSerialFilterInfo ** aInfo );

    static IDE_RC setConversionSerializeFilter( UChar                  aType,
                                                mtcNode              * aNode,
                                                mtxSerialExecuteFunc   aExecute,
                                                mtxSerialFilterInfo  * aFense,
                                                mtxSerialFilterInfo ** aInfo );

    static IDE_RC setParentSerializeFilter( UChar                  aType,
                                            UChar                  aCount,
                                            mtcNode              * aNode,
                                            mtxSerialExecuteFunc   aExecute,
                                            mtxSerialFilterInfo  * aLeft,
                                            mtxSerialFilterInfo  * aRight,
                                            mtxSerialFilterInfo  * aFense,
                                            mtxSerialFilterInfo ** aInfo );

    static IDE_RC setSerializeFilter( UChar                  aType,
                                      UChar                  aCount,
                                      mtcNode              * aNode,
                                      mtxSerialExecuteFunc   aExecute,
                                      mtxSerialFilterInfo  * aLeft,
                                      mtxSerialFilterInfo  * aRight,
                                      mtxSerialFilterInfo  * aFense,
                                      mtxSerialFilterInfo ** aInfo );
    
};

class qtcNodeI
{
/***********************************************************************
 *
 * qtcNode�� ���� ���� ������ �Լ���
 *
 *      by kumdory, 2005-04-25
 ***********************************************************************/

public:

    static inline idBool isListNode( qtcNode * aNode )
    {
        return ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                 == MTC_NODE_OPERATOR_LIST ) ? ID_TRUE : ID_FALSE;
    }

    static inline void linkNode( qtcNode * aPrev, qtcNode * aNext )
    {
        if( aNext == NULL )
        {
            aPrev->node.next = NULL;
        }
        else
        {
            aPrev->node.next = (mtcNode *)&(aNext->node);
        }
    }

    static inline qtcNode* nextNode( qtcNode * aNode )
    {
        return (qtcNode *)(aNode->node.next);
    }

    static inline qtcNode* argumentNode( qtcNode * aNode )
    {
        return (qtcNode *)(aNode->node.arguments);
    }

    static inline idBool isOneColumnSubqueryNode( qtcNode * aNode )
    {
        if( aNode == NULL || aNode->node.arguments == NULL )
        {
            return ID_FALSE;
        }
        else if( aNode->node.arguments->next == NULL )
        {
            return ID_TRUE;
        }
        else
        {
            return ID_FALSE;
        }
    }

    static inline idBool isColumnArgument( qcStatement * aStatement,
                                           qtcNode     * aNode )
    {
        if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
              == MTC_NODE_OPERATOR_LIST )
        {
            aNode = (qtcNode *)(aNode->node.arguments);
            while( aNode != NULL )
            {
                if( QTC_IS_COLUMN( aStatement, aNode ) == ID_TRUE )
                {
                    // Nothing To Do
                }
                else
                {
                    return ID_FALSE;
                }
                aNode = (qtcNode *)(aNode->node.next);
            }
            return ID_TRUE;
        }
        else
        {
            if( QTC_IS_COLUMN( aStatement, aNode ) == ID_TRUE )
            {
                return ID_TRUE;
            }
            else
            {
                return ID_FALSE;
            }
        }
    }
};

#endif /* _O_QTC_H_ */
