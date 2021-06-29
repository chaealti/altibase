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
 

/***********************************************************************
 * $Id: mtfLike.cpp 90192 2021-03-12 02:01:03Z jayce.park $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtl.h>
#include <mtf.h>
#include <mtk.h>
#include <mtv.h>
#include <mtuProperty.h>

extern mtdModule mtdChar;
extern mtdModule mtdVarchar;
extern mtdModule mtdEchar;
extern mtdModule mtdEvarchar;
extern mtdModule mtdBit;
extern mtdModule mtdBinary;
extern mtdModule mtdClob;

extern mtfModule mtfNotLike;

extern mtfModule mtfLike;

//fix for BUG-15930
extern mtlModule mtlAscii;

static mtcName mtfLikeFunctionName[1] = {
    { NULL, 4, (void*)"LIKE" }
};

static IDE_RC mtfLikeEstimate( mtcNode*     aNode,
                               mtcTemplate* aTemplate,
                               mtcStack*    aStack,
                               SInt         aRemain,
                               mtcCallBack* aCallBack );

static IDE_RC mtfLikeEstimateCharFast( mtcNode*     aNode,
                                       mtcTemplate* aTemplate,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       mtcCallBack* aCallBack );

static IDE_RC mtfLikeEstimateBitFast( mtcNode*     aNode,
                                      mtcTemplate* aTemplate,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      mtcCallBack* aCallBack );

static IDE_RC mtfLikeEstimateXlobLocatorFast( mtcNode*     aNode,
                                              mtcTemplate* aTemplate,
                                              mtcStack*    aStack,
                                              SInt         aRemain,
                                              mtcCallBack* aCallBack );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
static IDE_RC mtfLikeEstimateClobValueFast( mtcNode     * aNode,
                                            mtcTemplate * aTemplate,
                                            mtcStack    * aStack,
                                            SInt          aRemain,
                                            mtcCallBack * aCallBack );

// PROJ-2002 Column Security
static IDE_RC mtfLikeEstimateEcharFast( mtcNode*     aNode,
                                        mtcTemplate* aTemplate,
                                        mtcStack*    aStack,
                                        SInt         aRemain,
                                        mtcCallBack* aCallBack );

//fix for PROJ-1571
static IDE_RC convertChar2Bit( void     * aValue,
                               void     * aToken,
                               UInt       aTokenLength,
                               idBool     aFiller    );

static IDE_RC nextCharWithBuffer( mtcLobBuffer    * aBuffer,
                                  mtcLobCursor    * aCursor,
                                  mtcLobCursor    * aCursorPrev,
                                  mtcTemplate     * aTemplate );

static IDE_RC compareCharWithBuffer( mtcLobBuffer       * aBuffer,
                                     const mtcLobCursor * aCursor,
                                     const UChar        * aFormat,
                                     idBool             * aIsSame,
                                     mtcTemplate     * aTemplate );

static IDE_RC nextCharWithBufferMB( mtcLobBuffer    * aBuffer,
                                    mtcLobCursor    * aCursor,
                                    mtcLobCursor    * aCursorPrev,
                                    const mtlModule * aLanguage,
                                    mtcTemplate     * aTemplate );

static IDE_RC compareCharWithBufferMB( mtcLobBuffer       * aBuffer,
                                       const mtcLobCursor * aCursor,
                                       const UChar        * aFormat,
                                       UChar                aFormatCharSize,
                                       idBool             * aIsSame,
                                       const mtlModule    * aLanguage,
                                       mtcTemplate     * aTemplate );

static IDE_RC mtfLikeReadLob( mtcLobBuffer  * aBuffer,
                              mtcTemplate   * aTemplate );

static IDE_RC classifyFormatString( const UChar       * aFormat,
                                    UShort              aFormatLen,
                                    const UChar       * aEscape,
                                    UShort              aEscapeLen,
                                    mtcLikeFormatInfo * aFormatInfo,
                                    mtcCallBack       * aCallBack,
                                    const mtlModule   * aLanguage );

static IDE_RC classifyFormatStringMB( const UChar       * aFormat,
                                      UShort              aFormatLen,
                                      const UChar       * aEscape,
                                      UShort              aEscapeLen,
                                      mtcLikeFormatInfo * aFormatInfo,
                                      mtcCallBack       * aCallBack,
                                      const mtlModule   * aLanguage );

static IDE_RC getMoreInfoFromPattern( const UChar      * aFormat,
                                      UShort             aFormatLen,
                                      const UChar      * aEscape,
                                      UShort             aEscapeLen,
                                      mtcLikeBlockInfo * aBlockInfo,
                                      UInt             * aBlockCnt,
                                      UChar            * sNewString,
                                      const mtlModule  * /*aLanguage*/ );

static IDE_RC getMoreInfoFromPatternMB( const UChar      * aFormat,
                                        UShort             aFormatLen,
                                        const UChar      * aEscape,
                                        UShort             aEscapeLen,
                                        mtcLikeBlockInfo * aBlockInfo,
                                        UInt             * aBlockCnt,
                                        UChar            * aNewString,
                                        const mtlModule  * aLanguage );

IDE_RC mtfLikeFormatInfo( mtcNode            * aNode,
                          mtcTemplate        * aTemplate,
                          mtcStack           * aStack,
                          mtcLikeFormatInfo ** aFormatInfo,
                          UShort             * aFormatLen,
                          mtcCallBack        * aCallBack );

mtfModule mtfLike = {
    3|MTC_NODE_OPERATOR_RANGED|MTC_NODE_COMPARISON_TRUE|
        MTC_NODE_FILTER_NEED|
        MTC_NODE_PRINT_FMT_MISC,
    ~0,
    0.05,  // TODO : default selectivity 
    mtfLikeFunctionName,
    &mtfNotLike,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    mtfLikeEstimate
};

static IDE_RC mtfExtractRange( mtcNode      * aNode,
                               mtcTemplate  * aTemplate,
                               mtkRangeInfo * aInfo,
                               smiRange*      aRange );

static IDE_RC mtfExtractRange4Bit( mtcNode      * aNode,
                                   mtcTemplate  * aTemplate,
                                   mtkRangeInfo * aInfo,
                                   smiRange*      aRange );

// PROJ-2002 Column Security
static IDE_RC mtfExtractRange4Echar( mtcNode      * aNode,
                                     mtcTemplate  * aTemplate,
                                     mtkRangeInfo * aInfo,
                                     smiRange*      aRange );

IDE_RC mtfLikeCalculate( mtcNode*     aNode,
                         mtcStack*    aStack,
                         SInt         aRemain,
                         void*        aInfo,
                         mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateNormal( mtcNode*     aNode,
                               mtcStack*    aStack,
                               SInt         aRemain,
                               void*        aInfo,
                               mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateNormalFast( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateEqualFast( mtcNode*     aNode,
                                  mtcStack*    aStack,
                                  SInt         aRemain,
                                  void*        aInfo,
                                  mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateIsNotNullFast( mtcNode*     aNode,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      void*        aInfo,
                                      mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateLengthFast( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateOnePercentFast( mtcNode*     aNode,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       void*        aInfo,
                                       mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateMB( mtcNode*     aNode,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           void*        aInfo,
                           mtcTemplate* aTemplate );

//fix for BUG-15930
IDE_RC mtfLikeCalculateMBNormal( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateMBNormalFast( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateLengthMBFast( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculate4XlobLocator( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate );

// PROJ-1755
IDE_RC mtfLikeCalculate4XlobLocatorNormal( mtcNode*     aNode,
                                           mtcStack*    aStack,
                                           SInt         aRemain,
                                           void*        aInfo,
                                           mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculate4XlobLocatorNormalFast( mtcNode*     aNode,
                                               mtcStack*    aStack,
                                               SInt         aRemain,
                                               void*        aInfo,
                                               mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateEqual4XlobLocatorFast( mtcNode*     aNode,
                                              mtcStack*    aStack,
                                              SInt         aRemain,
                                              void*        aInfo,
                                              mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateIsNotNull4XlobLocatorFast( mtcNode*     aNode,
                                                  mtcStack*    aStack,
                                                  SInt         aRemain,
                                                  void*        aInfo,
                                                  mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateLength4XlobLocatorFast( mtcNode*     aNode,
                                               mtcStack*    aStack,
                                               SInt         aRemain,
                                               void*        aInfo,
                                               mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateOnePercent4XlobLocatorFast( mtcNode*     aNode,
                                                   mtcStack*    aStack,
                                                   SInt         aRemain,
                                                   void*        aInfo,
                                                   mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculate4XlobLocatorMB( mtcNode*     aNode,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       void*        aInfo,
                                       mtcTemplate* aTemplate );

// BUG-16276
IDE_RC mtfLikeCalculate4XlobLocatorMBNormal( mtcNode*     aNode,
                                             mtcStack*    aStack,
                                             SInt         aRemain,
                                             void*        aInfo,
                                             mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculate4XlobLocatorMBNormalFast( mtcNode*     aNode,
                                                 mtcStack*    aStack,
                                                 SInt         aRemain,
                                                 void*        aInfo,
                                                 mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateLength4XlobLocatorMBFast( mtcNode*     aNode,
                                                 mtcStack*    aStack,
                                                 SInt         aRemain,
                                                 void*        aInfo,
                                                 mtcTemplate* aTemplate );

// PROJ-2002 Column Security
IDE_RC mtfLikeCalculate4Echar( mtcNode*     aNode,
                               mtcStack*    aStack,
                               SInt         aRemain,
                               void*        aInfo,
                               mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculate4EcharNormal( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculate4EcharNormalFast( mtcNode*     aNode,
                                         mtcStack*    aStack,
                                         SInt         aRemain,
                                         void*        aInfo,
                                         mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateEqual4EcharFast( mtcNode*     aNode,
                                        mtcStack*    aStack,
                                        SInt         aRemain,
                                        void*        aInfo,
                                        mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateIsNotNull4EcharFast( mtcNode*     aNode,
                                            mtcStack*    aStack,
                                            SInt         aRemain,
                                            void*        aInfo,
                                            mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateLength4EcharFast( mtcNode*     aNode,
                                         mtcStack*    aStack,
                                         SInt         aRemain,
                                         void*        aInfo,
                                         mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateOnePercent4EcharFast( mtcNode*     aNode,
                                             mtcStack*    aStack,
                                             SInt         aRemain,
                                             void*        aInfo,
                                             mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculate4EcharMB( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculate4EcharMBNormal( mtcNode*     aNode,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       void*        aInfo,
                                       mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculate4EcharMBNormalFast( mtcNode*     aNode,
                                           mtcStack*    aStack,
                                           SInt         aRemain,
                                           void*        aInfo,
                                           mtcTemplate* aTemplate );

IDE_RC mtfLikeCalculateLength4EcharMBFast( mtcNode*     aNode,
                                           mtcStack*    aStack,
                                           SInt         aRemain,
                                           void*        aInfo,
                                           mtcTemplate* aTemplate );

idBool mtfCheckMatchBlock( mtcLikeBlockInfo ** aBlock,
                           mtcLikeBlockInfo *  aBlockFence,
                           UChar            ** aString,
                           UChar            *  aStringFence );

idBool mtfCheckMatchBlockMB( mtcLikeBlockInfo ** aBlock,
                             mtcLikeBlockInfo *  aBlockFence,
                             UChar            ** aString,
                             UChar            *  aStringFence,
                             const mtlModule  *  aLanguage );

IDE_RC mtfCheckMatchBlockForLOB( mtcLobBuffer     *  aBuffer,
                                 UInt             *  aOffset,
                                 UInt                aLobLength,
                                 mtcLikeBlockInfo ** aBlock,
                                 mtcLikeBlockInfo *  aBlockFence,
                                 idBool           *  aIsTrue );

IDE_RC mtfCheckMatchBlockMBForLOB( mtcLobBuffer     *  aBuffer,
                                   UInt             *  aOffset,
                                   UInt                aLobLength,
                                   mtcLikeBlockInfo ** aBlock,
                                   mtcLikeBlockInfo *  aBlockFence,
                                   const mtlModule  *  aLanguage,
                                   idBool           *  aIsTrue );

IDE_RC mtfLikeCalculateOnePass( UChar            * aString,
                                UChar            * aStringFence,
                                mtcLikeBlockInfo * aBlock,
                                UInt               aBlockCnt,
                                mtdBooleanType   * aResult );

IDE_RC mtfLikeCalculateMBOnePass( UChar            * aString,
                                  UChar            * aStringFence,
                                  mtcLikeBlockInfo * aBlock,
                                  UInt               aBlockCnt,                                         
                                  mtdBooleanType   * aResult,
                                  const mtlModule  * aLanguage );

IDE_RC mtfLikeCalculate4XlobLocatorOnePass( mtdClobLocatorType  aLocator,
                                            UInt                aLobLength,
                                            mtcLikeBlockInfo  * aBlock,
                                            UInt                aBlockCnt,
                                            mtdBooleanType    * aResult );

IDE_RC mtfLikeCalculate4XlobLocatorMBOnePass( mtdClobLocatorType  aLocator,
                                              UInt                aLobLength,
                                              mtcLikeBlockInfo  * aBlock,
                                              UInt                aBlockCnt,
                                              mtdBooleanType    * aResult,
                                              UInt                aMaxCharSize,
                                              const mtlModule   * aLanguage,
                                              mtcTemplate       * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
IDE_RC mtfLikeCalculate4ClobValue( mtcNode     * aNode,
                                   mtcStack    * aStack,
                                   SInt          aRemain,
                                   void        * aInfo,
                                   mtcTemplate * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
IDE_RC mtfLikeCalculate4ClobValueMB( mtcNode     * aNode,
                                     mtcStack    * aStack,
                                     SInt          aRemain,
                                     void        * aInfo,
                                     mtcTemplate * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
IDE_RC mtfLikeCalculate4ClobValueNormal( mtcNode     * aNode,
                                         mtcStack    * aStack,
                                         SInt          aRemain,
                                         void        * aInfo,
                                         mtcTemplate * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
IDE_RC mtfLikeCalculate4ClobValueMBNormal( mtcNode     * aNode,
                                           mtcStack    * aStack,
                                           SInt          aRemain,
                                           void        * aInfo,
                                           mtcTemplate * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
IDE_RC mtfLikeCalculate4ClobValueNormalFast( mtcNode     * aNode,
                                             mtcStack    * aStack,
                                             SInt          aRemain,
                                             void        * aInfo,
                                             mtcTemplate * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
IDE_RC mtfLikeCalculateEqual4ClobValueFast( mtcNode     * aNode,
                                            mtcStack    * aStack,
                                            SInt          aRemain,
                                            void        * aInfo,
                                            mtcTemplate * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
IDE_RC mtfLikeCalculateIsNotNull4ClobValueFast( mtcNode     * aNode,
                                                mtcStack    * aStack,
                                                SInt          aRemain,
                                                void        * aInfo,
                                                mtcTemplate * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
IDE_RC mtfLikeCalculateLength4ClobValueFast( mtcNode     * aNode,
                                             mtcStack    * aStack,
                                             SInt          aRemain,
                                             void        * aInfo,
                                             mtcTemplate * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
IDE_RC mtfLikeCalculateOnePercent4ClobValueFast( mtcNode     * aNode,
                                                 mtcStack    * aStack,
                                                 SInt          aRemain,
                                                 void        * aInfo,
                                                 mtcTemplate * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
IDE_RC mtfLikeCalculate4ClobValueMBNormalFast( mtcNode     * aNode,
                                               mtcStack    * aStack,
                                               SInt          aRemain,
                                               void        * aInfo,
                                               mtcTemplate * aTemplate );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
IDE_RC mtfLikeCalculateLength4ClobValueMBFast( mtcNode     * aNode,
                                               mtcStack    * aStack,
                                               SInt          aRemain,
                                               void        * aInfo,
                                               mtcTemplate * aTemplate );

const mtcExecute mtfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeDefaultLike,
    mtfExtractRange
};

const mtcExecute mtfExecuteNormal = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculateNormal,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeDefaultLike,
    mtfExtractRange
};

const mtcExecute mtfExecute4Bit = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeDefaultLike,
    mtfExtractRange4Bit
};

const mtcExecute mtfExecute4BitNormal = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculateNormal,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeDefaultLike,
    mtfExtractRange4Bit
};

//fix for BUG-15930
const mtcExecute mtfExecuteMB = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculateMB,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeDefaultLike,
    mtfExtractRange   // MB�� ��� mtfExtractRange�ȿ��� �б���
};

//fix for BUG-15930
const mtcExecute mtfExecuteMBNormal = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculateMBNormal,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeDefaultLike,
    mtfExtractRange   // MB�� ��� mtfExtractRange�ȿ��� �б���
};

// PROJ-1755
const mtcExecute mtfExecute4XlobLocator = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4XlobLocator,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,  // lob�� index�� ����
    mtk::extractRangeNA
};

// BUG-16276
const mtcExecute mtfExecute4XlobLocatorMB = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4XlobLocatorMB,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,  // lob�� index�� ����
    mtk::extractRangeNA
};

// PROJ-1755
const mtcExecute mtfExecute4XlobLocatorNormal = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4XlobLocatorNormal,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,  // lob�� index�� ����
    mtk::extractRangeNA
};

// BUG-16276
const mtcExecute mtfExecute4XlobLocatorMBNormal = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4XlobLocatorMBNormal,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,  // lob�� index�� ����
    mtk::extractRangeNA
};

// PROJ-2002 Column Security
const mtcExecute mtfExecute4Echar = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4Echar,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeDefaultLike4Echar,
    mtfExtractRange4Echar   // MB�� ��� mtfExtractRange�ȿ��� �б���
};

const mtcExecute mtfExecute4EcharMB = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4EcharMB,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeDefaultLike4Echar,
    mtfExtractRange4Echar   // MB�� ��� mtfExtractRange�ȿ��� �б���
};

// PROJ-2002 Column Security
const mtcExecute mtfExecute4EcharNormal = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4EcharNormal,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeDefaultLike4Echar,
    mtfExtractRange4Echar   // MB�� ��� mtfExtractRange�ȿ��� �б���
};

const mtcExecute mtfExecute4EcharMBNormal = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4EcharMBNormal,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeDefaultLike4Echar,
    mtfExtractRange4Echar   // MB�� ��� mtfExtractRange�ȿ��� �б���
};

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
const mtcExecute mtfExecute4ClobValue = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4ClobValue,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,  // lob�� index�� ����
    mtk::extractRangeNA
};

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
const mtcExecute mtfExecute4ClobValueMB = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4ClobValueMB,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,  // lob�� index�� ����
    mtk::extractRangeNA
};

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
const mtcExecute mtfExecute4ClobValueNormal = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4ClobValueNormal,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,  // lob�� index�� ����
    mtk::extractRangeNA
};

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
const mtcExecute mtfExecute4ClobValueMBNormal = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtfLikeCalculate4ClobValueMBNormal,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,  // lob�� index�� ����
    mtk::extractRangeNA
};

IDE_RC mtfLikeEstimate( mtcNode*     aNode,
                        mtcTemplate* aTemplate,
                        mtcStack*    aStack,
                        SInt         aRemain,
                        mtcCallBack* aCallBack )
{
    extern mtdModule mtdBoolean;

    mtcNode          * sNode;
    mtcNode          * sFormatNode;
    mtcNode          * sEscapeNode;

    // fix for PROJ-1571
    mtcColumn        * sIndexColumn;

    const mtdModule  * sModules[3];

    SInt               sModuleId;
    UInt               sPrecision;
    mtcLikeFormatInfo *sFormatInfo;

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( (aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK) < 2 ) ||
                    ( (aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK) > 3 ),
                    ERR_INVALID_FUNCTION_ARGUMENT );

    sNode = aNode->arguments;
    sFormatNode = sNode->next;
    sEscapeNode = sFormatNode->next;

    if( ( sNode->lflag & MTC_NODE_COMPARISON_MASK ) ==
        MTC_NODE_COMPARISON_TRUE )
    {
        sNode->lflag &= ~(MTC_NODE_INDEX_MASK);
    }
    aNode->lflag &= ~(MTC_NODE_INDEX_MASK);
    aNode->lflag |= sNode->lflag & MTC_NODE_INDEX_MASK;
    for( sNode  = aNode->arguments->next;
         sNode != NULL;
         sNode  = sNode->next )
    {
        sNode->lflag &= ~(MTC_NODE_INDEX_MASK);
    }

    //IDE_TEST( mtdBoolean.estimate( aStack[0].column, 0, 0, 0 )
    //          != IDE_SUCCESS );
    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     & mtdBoolean,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    // BUG-40992 FATAL when using _prowid
    // ������ ��� mtcStack �� column ���� �̿��ϸ� �ȴ�.
    sIndexColumn     = aStack[1].column;

    // BUG-22611
    // switch-case�� UInt ������ �������� �ι� �̻� ���� ���� ������ ����
    // ex )  case MTD_BIT_ID: ==> (UInt)-7 
    //       case MTD_VARBIT_ID: ==> (UInt)-8
    // ���� SInt ������ Ÿ�� ĳ���� �ϵ��� ������ 
    sModuleId = (SInt)sIndexColumn->module->id;

    switch ( sModuleId )
    {
        case MTD_CLOB_LOCATOR_ID:
        {
            if ( ( aNode->lflag & MTC_NODE_REESTIMATE_MASK ) == MTC_NODE_REESTIMATE_FALSE )
            {
                sModules[0] = &mtdVarchar;
                sModules[1] = sModules[0];
                sModules[2] = sModules[0];

                IDE_TEST( mtf::makeConversionNodes( aNode,
                                                    aNode->arguments->next,
                                                    aTemplate,
                                                    aStack + 2,
                                                    aCallBack,
                                                    sModules + 1 )
                          != IDE_SUCCESS );

                if ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                {
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecute4XlobLocator;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecute4XlobLocatorMB;
                    }
                }
                else
                {
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecute4XlobLocatorNormal;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecute4XlobLocatorMBNormal;
                    }                    
                }                
            }
            else
            {
                // PROJ-1755
                // format string�� ���� ���� �Լ��� �����Ѵ�.
                IDE_TEST( mtfLikeEstimateXlobLocatorFast( aNode,
                                                          aTemplate,
                                                          aStack,
                                                          aRemain,
                                                          aCallBack )
                          != IDE_SUCCESS );
            }
            break;
        }

        case MTD_CLOB_ID:
        {
            if ( aTemplate->isBaseTable( aTemplate, aNode->arguments->table ) == ID_TRUE )
            {
                if ( ( aNode->lflag & MTC_NODE_REESTIMATE_MASK ) == MTC_NODE_REESTIMATE_FALSE )
                {
                    IDE_TEST( mtf::getLobFuncResultModule( &sModules[0],
                                                           sIndexColumn->module )
                              != IDE_SUCCESS );
                    sModules[1] = &mtdVarchar;
                    sModules[2] = sModules[1];

                    IDE_TEST( mtf::makeConversionNodes( aNode,
                                                        aNode->arguments,
                                                        aTemplate,
                                                        aStack + 1,
                                                        aCallBack,
                                                        sModules )
                              != IDE_SUCCESS );

                    if ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                    {
                        if( sIndexColumn->language == &mtlAscii )
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column] =
                                mtfExecute4XlobLocator;
                        }
                        else
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column] =
                                mtfExecute4XlobLocatorMB;
                        }
                    }
                    else
                    {
                        if( sIndexColumn->language == &mtlAscii )
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column] =
                                mtfExecute4XlobLocatorNormal;
                        }
                        else
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column] =
                                mtfExecute4XlobLocatorMBNormal;
                        }

                    }
                }
                else
                {
                    // PROJ-1755
                    // format string�� ���� ���� �Լ��� �����Ѵ�.
                    IDE_TEST( mtfLikeEstimateXlobLocatorFast( aNode,
                                                              aTemplate,
                                                              aStack,
                                                              aRemain,
                                                              aCallBack )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                if ( (aNode->lflag & MTC_NODE_REESTIMATE_MASK) == MTC_NODE_REESTIMATE_FALSE )
                {
                    sModules[0] = &mtdClob;
                    sModules[1] = &mtdVarchar;
                    sModules[2] = &mtdVarchar;

                    IDE_TEST( mtf::makeConversionNodes( aNode,
                                                        aNode->arguments,
                                                        aTemplate,
                                                        aStack + 1,
                                                        aCallBack,
                                                        sModules )
                              != IDE_SUCCESS );

                    if ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                    {
                        if ( sIndexColumn->language == &mtlAscii )
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column] =
                                mtfExecute4ClobValue;
                        }
                        else
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column] =
                                mtfExecute4ClobValueMB;
                        }
                    }
                    else
                    {
                        if ( sIndexColumn->language == &mtlAscii )
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column] =
                                mtfExecute4ClobValueNormal;
                        }
                        else
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column] =
                                mtfExecute4ClobValueMBNormal;
                        }
                    }
                }
                else
                {
                    // PROJ-1755
                    // format string�� ���� ���� �Լ��� �����Ѵ�.
                    IDE_TEST( mtfLikeEstimateClobValueFast( aNode,
                                                            aTemplate,
                                                            aStack,
                                                            aRemain,
                                                            aCallBack )
                              != IDE_SUCCESS );
                }
            }
            break;
        }

        case MTD_BIT_ID:
        case MTD_VARBIT_ID:
        {
            if ( ( aNode->lflag & MTC_NODE_REESTIMATE_MASK ) == MTC_NODE_REESTIMATE_FALSE )
            {
                IDE_TEST( mtf::getLikeModule( &sModules[0],
                                              aStack[1].column->module,
                                              aStack[2].column->module )
                          != IDE_SUCCESS );
                sModules[1] = sModules[0];
                sModules[2] = sModules[0];

                IDE_TEST( mtf::makeConversionNodes( aNode,
                                                    aNode->arguments,
                                                    aTemplate,
                                                    aStack + 1,
                                                    aCallBack,
                                                    sModules )
                          != IDE_SUCCESS );

                if ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                {
                    aTemplate->rows[aNode->table].execute[aNode->column] =
                        mtfExecute4Bit;
                }
                else
                {
                    aTemplate->rows[aNode->table].execute[aNode->column] =
                        mtfExecute4BitNormal;
                }                
            }
            else
            {
                // PROJ-1755
                // format string�� ���� ���� �Լ��� �����Ѵ�.
                IDE_TEST( mtfLikeEstimateBitFast( aNode,
                                                  aTemplate,
                                                  aStack,
                                                  aRemain,
                                                  aCallBack )
                          != IDE_SUCCESS );
            }
            break;
        }

        case MTD_ECHAR_ID:
        case MTD_EVARCHAR_ID:
        {
            if ( ( aNode->lflag & MTC_NODE_REESTIMATE_MASK ) == MTC_NODE_REESTIMATE_FALSE )
            {
                IDE_TEST( mtf::getLikeModule( &sModules[0],
                                              aStack[1].column->module,
                                              aStack[2].column->module )
                          != IDE_SUCCESS );
                sModules[1] = sModules[0];

                // escape ���ڴ� ���� Ÿ������ ��ȯ�Ѵ�.
                if ( sModules[0] == &mtdEchar )
                {
                    sModules[2] = &mtdChar;
                }
                else
                {
                    sModules[2] = &mtdVarchar;
                }

                IDE_TEST( mtf::makeConversionNodes( aNode,
                                                    aNode->arguments,
                                                    aTemplate,
                                                    aStack + 1,
                                                    aCallBack,
                                                    sModules )
                          != IDE_SUCCESS );

                if ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                {
                    //fix for BUG-15930
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecute4Echar;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecute4EcharMB;
                    }
                }
                else
                {
                    //fix for BUG-15930
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecute4EcharNormal;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecute4EcharMBNormal;
                    }                    
                }
            }
            else
            {
                // format string�� ���� ���� �Լ��� �����Ѵ�.
                IDE_TEST( mtfLikeEstimateEcharFast( aNode,
                                                    aTemplate,
                                                    aStack,
                                                    aRemain,
                                                    aCallBack )
                          != IDE_SUCCESS );
            }
            break;
        }

        default:
        {
            if ( ( aNode->lflag & MTC_NODE_REESTIMATE_MASK ) == MTC_NODE_REESTIMATE_FALSE )
            {
                IDE_TEST( mtf::getLikeModule( &sModules[0],
                                              aStack[1].column->module,
                                              aStack[2].column->module )
                          != IDE_SUCCESS );
                sModules[1] = sModules[0];
                sModules[2] = sModules[0];

                IDE_TEST( mtf::makeConversionNodes( aNode,
                                                    aNode->arguments,
                                                    aTemplate,
                                                    aStack + 1,
                                                    aCallBack,
                                                    sModules )
                          != IDE_SUCCESS );

                if ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                {
                    //fix for BUG-15930
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecute;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecuteMB;
                    }
                }                
                else
                {
                    //fix for BUG-15930
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecuteNormal;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column] =
                            mtfExecuteMBNormal;
                    }
                }                
            }
            else
            {
                // PROJ-1755
                // format string�� ���� ���� �Լ��� �����Ѵ�.
                IDE_TEST( mtfLikeEstimateCharFast( aNode,
                                                   aTemplate,
                                                   aStack,
                                                   aRemain,
                                                   aCallBack )
                          != IDE_SUCCESS );
            }
            break;
        }
    }

    if ( aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo == NULL)
    {
        sPrecision = MTC_LIKE_PATTERN_MAX_SIZE;
    }
    else
    {
        // BUG-37057 ������ ���̴� ���ڿ��� ���̸� �Ѿ�� ����.
        sFormatInfo = (mtcLikeFormatInfo*)
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo;

        sPrecision = sFormatInfo->patternSize;
    }

    IDE_TEST( mtc::initializeColumn( aStack[0].column + 1,
                                     & mtdBinary,
                                     1,
                                     sPrecision * ID_SIZEOF(mtcLikeBlockInfo),
                                     0 )
              != IDE_SUCCESS );

    IDE_TEST( mtc::initializeColumn( aStack[0].column + 2,
                                     & mtdBinary,
                                     1,
                                     sPrecision * ID_SIZEOF(UChar),
                                     0 )
              != IDE_SUCCESS );

    // PROJ-1755
    if( ( MTC_NODE_IS_DEFINED_VALUE( sFormatNode ) == ID_TRUE )
        &&
        ( ( ( aTemplate->rows[sFormatNode->table].lflag & MTC_TUPLE_TYPE_MASK )
            == MTC_TUPLE_TYPE_CONSTANT ) ||
          ( ( aTemplate->rows[sFormatNode->table].lflag & MTC_TUPLE_TYPE_MASK )
            == MTC_TUPLE_TYPE_INTERMEDIATE ) ) )
    {
        if( sEscapeNode == NULL )
        {
            // format�� ����̰� esacpe���ڰ� ���� ���

            aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
            aNode->lflag |= MTC_NODE_REESTIMATE_TRUE;
        }
        else
        {
            if( ( MTC_NODE_IS_DEFINED_VALUE( sEscapeNode ) == ID_TRUE )
                &&
                ( ( ( aTemplate->rows[sEscapeNode->table].lflag & MTC_TUPLE_TYPE_MASK )
                    == MTC_TUPLE_TYPE_CONSTANT ) ||
                  ( ( aTemplate->rows[sEscapeNode->table].lflag & MTC_TUPLE_TYPE_MASK )
                    == MTC_TUPLE_TYPE_INTERMEDIATE ) ) )
            {
                // format�� ����̰� escape���ڵ� ����� ���

                aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
                aNode->lflag |= MTC_NODE_REESTIMATE_TRUE;
            }
            else
            {
                // format�� ����̰� escape���ڰ� ����� �ƴ� ���

                aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
                aNode->lflag |= MTC_NODE_REESTIMATE_FALSE;
            }
        }
            
        // BUG-38070 undef type���� re-estimate���� �ʴ´�.
        if ( ( aTemplate->variableRow != ID_USHORT_MAX ) &&
             ( ( aNode->lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_EXIST ) )
        {
            if ( aTemplate->rows[aTemplate->variableRow].
                 columns->module->id == MTD_UNDEF_ID )
            {
                aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
                aNode->lflag |= MTC_NODE_REESTIMATE_FALSE;
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            // nothing to do
        }
    }
    else
    {
        // format�� ����� �ƴ� ���

        aNode->lflag &= ~MTC_NODE_REESTIMATE_MASK;
        aNode->lflag |= MTC_NODE_REESTIMATE_FALSE;
    }    

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC classifyFormatString( const UChar       * aFormat,
                             UShort              aFormatLen,
                             const UChar       * aEscape,
                             UShort              aEscapeLen,
                             mtcLikeFormatInfo * aFormatInfo,
                             mtcCallBack       * aCallBack,
                             const mtlModule   * aLanguage )
{
    UChar      sEscape = 0;
    idBool     sNullEscape;
    UChar    * sIndex;
    UChar    * sFence;
    UChar    * sPattern;
    UShort     sPatternSize;

    // escape ���� ����
    if( aEscapeLen < 1 )
    {
        sNullEscape = ID_TRUE;
    }
    else if( aEscapeLen == 1 )
    {
        sNullEscape = ID_FALSE;
        sEscape = *aEscape;
    }
    else
    {
        IDE_RAISE( ERR_INVALID_ESCAPE );
    }

    // '_', '%'�� ����
    sIndex = (UChar*) aFormat;
    sFence = sIndex + aFormatLen;
    
    sPattern = aFormatInfo->pattern;
    sPatternSize = 0;
    
    while ( sIndex < sFence )
    {
        if( (sNullEscape == ID_FALSE) && (*sIndex == sEscape) )
        {
            // To Fix PR-13004
            // ABR ������ ���Ͽ� ������Ų �� �˻��Ͽ��� ��
            sIndex++;
            
            // escape ������ ���,
            // escape ���� ���ڰ� '%','_' �������� �˻�
            IDE_TEST_RAISE( sIndex >= sFence, ERR_INVALID_LITERAL );
            
            // To Fix BUG-12578
            IDE_TEST_RAISE( (*sIndex != (UShort)'%') &&
                            (*sIndex != (UShort)'_') &&
                            (*sIndex != sEscape), // sEsacpe�� null�� �ƴ�
                            ERR_INVALID_LITERAL );

            // �Ϲ� ������ ���
            aFormatInfo->charCnt++;
        }
        else if( *sIndex == (UShort)'_' )
        {
            // Ư������ '_'�� ���            
            aFormatInfo->underCnt++;
        }
        else if( *sIndex == (UShort)'%' )
        {
            // Ư������ '%'�� ���
            aFormatInfo->percentCnt++;

            // BUGBUG
            // ������ '%'�� ������ ã�ƾ� ��
            // ex) 'aaa%%bb'�� %������ �������� 1����

            // BUG-20524
            // ù��° '%'�� ���Ͽ� �� ��ġ�� ����Ѵ�.
            if ( aFormatInfo->percentCnt == 1 )
            {
                aFormatInfo->firstPercent = sPattern;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // �Ϲ� ������ ���
            aFormatInfo->charCnt++;
        }

        // formatPattern�� ����
        *sPattern = *sIndex;
        sPatternSize++;
        
        sPattern++;
        sIndex++;
    }

    aFormatInfo->patternSize = sPatternSize;

    // �Ϲݹ���, '_', '%'�� ������ format string�� �з���
    if ( aFormatInfo->underCnt == 0 )
    {
        if ( aFormatInfo->percentCnt == 0 )
        {
            if ( aFormatInfo->charCnt == 0 )
            {
                // underCnt = 0, percentCnt = 0, charCnt = 0
                
                // case 1
                // null Ȥ�� ''
                aFormatInfo->type = MTC_FORMAT_NULL;
            }
            else
            {
                // underCnt = 0, percentCnt = 0, charCnt > 0
                
                // case 2
                // �Ϲݹ��ڷθ� ������
                aFormatInfo->type = MTC_FORMAT_NORMAL;
            }
        }
        else
        {
            if ( aFormatInfo->charCnt == 0 )
            {
                // underCnt = 0, percentCnt > 0, charCnt = 0
                
                // case 4
                // '%'�θ� ������
                aFormatInfo->type = MTC_FORMAT_PERCENT;
            }
            else
            {
                // underCnt = 0, percentCnt > 0, charCnt > 0
                
                // case 6
                // �Ϲݹ��ڿ� '%'�� ������
                if ( aFormatInfo->percentCnt == 1 )
                {
                    // case 6-1
                    // �Ϲݹ��ڿ� '%' 1���� ������
                    aFormatInfo->type = MTC_FORMAT_NORMAL_ONE_PERCENT;
                }
                else
                {
                    // case 6-2
                    // �Ϲݹ��ڿ� '%' 2���̻����� ������
                    aFormatInfo->type = MTC_FORMAT_NORMAL_MANY_PERCENT;
                }
            }
        }
    }
    else
    {
        if ( aFormatInfo->percentCnt == 0 )
        {
            if ( aFormatInfo->charCnt == 0 )
            {
                // underCnt > 0, percentCnt = 0, charCnt = 0
                
                // case 3
                // '_'�θ� ������
                aFormatInfo->type = MTC_FORMAT_UNDER;
            }
            else
            {
                // underCnt > 0, percentCnt = 0, charCnt > 0
                
                // case 5
                // �Ϲݹ��ڿ� '_'�� ������
                aFormatInfo->type = MTC_FORMAT_NORMAL_UNDER;
            }
        }
        else
        {
            if ( aFormatInfo->charCnt == 0 )
            {
                // underCnt > 0, percentCnt > 0, charCnt = 0
                
                // case 7
                // '_'�� '%'�� ������
                aFormatInfo->type = MTC_FORMAT_UNDER_PERCENT;
            }
            else
            {
                // underCnt > 0, percentCnt > 0, charCnt > 0
                
                // case 8
                // �Ϲݹ��ڿ� '_', '%'�� ������
                aFormatInfo->type = MTC_FORMAT_ALL;
            }
        }
    }

    // case 6-1�� '%'�� 1���� ��� �ΰ� ������ �����Ѵ�.
    if ( aFormatInfo->type == MTC_FORMAT_NORMAL_ONE_PERCENT )
    {
        // '%'�� 1���� ��� ������ ���� ǥ���� �� �ִ�.
        // [head]%[tail]
        
        // [head]
        aFormatInfo->head = aFormatInfo->pattern;
        aFormatInfo->headSize = aFormatInfo->firstPercent - aFormatInfo->pattern;
        
        // [tail]
        aFormatInfo->tail = aFormatInfo->firstPercent + 1;
        aFormatInfo->tailSize = sPatternSize - aFormatInfo->headSize - 1;
    }
    else
    {
        // Nothing to do.
    }

    // PROJ-1753 one pass like
    // case 5,6-2,8�� ���� ����ȭ
    if ( ( aFormatInfo->type == MTC_FORMAT_NORMAL_UNDER ) ||
         ( aFormatInfo->type == MTC_FORMAT_NORMAL_MANY_PERCENT ) ||
         ( aFormatInfo->type == MTC_FORMAT_ALL ) )
    {
        IDE_TEST( aCallBack->alloc( aCallBack->info,
                                    ID_SIZEOF( mtcLikeBlockInfo) * sPatternSize,
                                    (void**) & aFormatInfo->blockInfo )
                  != IDE_SUCCESS );
            
        IDE_TEST( aCallBack->alloc( aCallBack->info,
                                    ID_SIZEOF( UChar ) * sPatternSize,
                                    (void**) & aFormatInfo->refinePattern )
                  != IDE_SUCCESS );
            
        IDE_TEST( getMoreInfoFromPattern( aFormat,
                                          aFormatLen,
                                          aEscape,
                                          aEscapeLen,
                                          aFormatInfo->blockInfo,
                                          &(aFormatInfo->blockCnt),
                                          aFormatInfo->refinePattern,
                                          aLanguage )
                  != IDE_SUCCESS );
    }
    else
    {
        // BUG-35504
        // new like������ format length ������ �ִ�.
        IDE_TEST_RAISE( aFormatLen > MTC_LIKE_PATTERN_MAX_SIZE,
                        ERR_LONG_PATTERN );
    }
   
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_LONG_PATTERN );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_LONG_PATTERN )); 

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_INVALID_ESCAPE ));
    
    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE ));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC classifyFormatStringMB( const UChar       * aFormat,
                               UShort              aFormatLen,
                               const UChar       * aEscape,
                               UShort              aEscapeLen,
                               mtcLikeFormatInfo * aFormatInfo,
                               mtcCallBack       * aCallBack,
                               const mtlModule   * aLanguage )
{
    idBool     sNullEscape;
    UChar    * sIndex;
    UChar    * sIndexPrev;
    UChar    * sFence;
    UChar    * sPattern;
    UShort     sPatternSize;
    idBool     sEqual;
    idBool     sEqual1;
    idBool     sEqual2;
    idBool     sEqual3;
    UChar      sSize;
    
    if( aLanguage->id == MTL_UTF16_ID )
    {
        // escape ���� ����
        if( aEscapeLen < MTL_UTF16_PRECISION )
        {
            sNullEscape = ID_TRUE;
        }
        else if( aEscapeLen == MTL_UTF16_PRECISION )
        {
            sNullEscape = ID_FALSE;
        }
        else
        {
            IDE_RAISE( ERR_INVALID_ESCAPE );
        }        
    }
    else
    {    
        // escape ���� ����
        if( aEscapeLen < 1 )
        {
            sNullEscape = ID_TRUE;
        }
        else if( aEscapeLen == 1 )
        {
            sNullEscape = ID_FALSE;
        }
        else
        {
            IDE_RAISE( ERR_INVALID_ESCAPE );
        }
    }
    
    // '_', '%'�� ����
    sIndex     = (UChar*) aFormat;
    sIndexPrev = sIndex;
    sFence     = sIndex + aFormatLen;
    
    sPattern = aFormatInfo->pattern;
    sPatternSize = 0;
    
    while ( sIndex < sFence )
    {
        sSize =  mtl::getOneCharSize( sIndex,
                                      sFence,
                                      aLanguage );
        
        if( sNullEscape == ID_FALSE )
        {
            sEqual = mtc::compareOneChar( sIndex,
                                          sSize,
                                          (UChar*)aEscape,
                                          aEscapeLen );
        }
        else
        {
            sEqual = ID_FALSE;
        }

        if( sEqual == ID_TRUE )
        {
            // To Fix PR-13004
            // ABR ������ ���Ͽ� ������Ų �� �˻��Ͽ��� ��
            (void)mtf::nextChar( sFence,
                                 &sIndex,
                                 &sIndexPrev,
                                 aLanguage );
            
            sSize =  mtl::getOneCharSize( sIndex,
                                          sFence,
                                          aLanguage );

            // escape ������ ���,
            // escape ���� ���ڰ� '%','_' �������� �˻�
            sEqual1 = mtc::compareOneChar( sIndex,
                                           sSize,
                                           aLanguage->specialCharSet[MTL_PC_IDX],
                                           aLanguage->specialCharSize );
            
            sEqual2 = mtc::compareOneChar( sIndex,
                                           sSize,
                                           aLanguage->specialCharSet[MTL_UB_IDX],
                                           aLanguage->specialCharSize );
            
            sEqual3 = mtc::compareOneChar( sIndex,
                                           sSize,
                                           (UChar*)aEscape,
                                           aEscapeLen );
            
            // To Fix BUG-12578
            IDE_TEST_RAISE( (sEqual1 != ID_TRUE) &&
                            (sEqual2 != ID_TRUE) &&
                            (sEqual3 != ID_TRUE),
                            ERR_INVALID_LITERAL );

            // �Ϲ� ������ ���
            aFormatInfo->charCnt++;
        }
        else
        {
            sEqual = mtc::compareOneChar( sIndex,
                                          sSize,
                                          aLanguage->specialCharSet[MTL_UB_IDX],
                                          aLanguage->specialCharSize );
            
            if ( sEqual == ID_TRUE )
            {
                // Ư������ '_'�� ���            
                aFormatInfo->underCnt++;
            }
            else
            {
                sEqual = mtc::compareOneChar( sIndex,
                                              sSize,
                                              aLanguage->specialCharSet[MTL_PC_IDX],
                                              aLanguage->specialCharSize );
                            
                if ( sEqual == ID_TRUE )
                {
                    // Ư������ '%'�� ���
                    aFormatInfo->percentCnt++;
                    
                    // BUGBUG
                    // ������ '%'�� ������ ã�ƾ� ��
                    // ex) 'aaa%%bb'�� %������ �������� 1����
                    
                    // BUG-20524
                    // ù��° '%'�� ���Ͽ� �� ��ġ�� ����Ѵ�.
                    if ( aFormatInfo->percentCnt == 1 )
                    {
                        aFormatInfo->firstPercent = sPattern;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    // �Ϲ� ������ ���
                    aFormatInfo->charCnt++;
                }
            }
        }
        
        (void)mtf::nextChar( sFence,
                             &sIndex,
                             &sIndexPrev,
                             aLanguage );
        
        // formatPattern�� ����
        idlOS::memcpy( sPattern,
                       sIndexPrev,
                       sIndex - sIndexPrev );
        
        sPattern += sIndex - sIndexPrev;
        sPatternSize += sIndex - sIndexPrev;
    }

    aFormatInfo->patternSize = sPatternSize;

    // �Ϲݹ���, '_', '%'�� ������ format string�� �з���
    if ( aFormatInfo->underCnt == 0 )
    {
        if ( aFormatInfo->percentCnt == 0 )
        {
            if ( aFormatInfo->charCnt == 0 )
            {
                // underCnt = 0, percentCnt = 0, charCnt = 0
                
                // case 1
                // null Ȥ�� ''
                aFormatInfo->type = MTC_FORMAT_NULL;
            }
            else
            {
                // underCnt = 0, percentCnt = 0, charCnt > 0
                
                // case 2
                // �Ϲݹ��ڷθ� ������
                aFormatInfo->type = MTC_FORMAT_NORMAL;
            }
        }
        else
        {
            if ( aFormatInfo->charCnt == 0 )
            {
                // underCnt = 0, percentCnt > 0, charCnt = 0
                
                // case 4
                // '%'�θ� ������
                aFormatInfo->type = MTC_FORMAT_PERCENT;
            }
            else
            {
                // underCnt = 0, percentCnt > 0, charCnt > 0
                
                // case 6
                // �Ϲݹ��ڿ� '%'�� ������
                if ( aFormatInfo->percentCnt == 1 )
                {
                    // case 6-1
                    // �Ϲݹ��ڿ� '%' 1���� ������
                    aFormatInfo->type = MTC_FORMAT_NORMAL_ONE_PERCENT;
                }
                else
                {
                    // case 6-2
                    // �Ϲݹ��ڿ� '%' 2���̻����� ������
                    aFormatInfo->type = MTC_FORMAT_NORMAL_MANY_PERCENT;
                }
            }
        }
    }
    else
    {
        if ( aFormatInfo->percentCnt == 0 )
        {
            if ( aFormatInfo->charCnt == 0 )
            {
                // underCnt > 0, percentCnt = 0, charCnt = 0
                
                // case 3
                // '_'�θ� ������
                aFormatInfo->type = MTC_FORMAT_UNDER;
            }
            else
            {
                // underCnt > 0, percentCnt = 0, charCnt > 0
                
                // case 5
                // �Ϲݹ��ڿ� '_'�� ������
                aFormatInfo->type = MTC_FORMAT_NORMAL_UNDER;
            }
        }
        else
        {
            if ( aFormatInfo->charCnt == 0 )
            {
                // underCnt > 0, percentCnt > 0, charCnt = 0
                
                // case 7
                // '_'�� '%'�� ������
                aFormatInfo->type = MTC_FORMAT_UNDER_PERCENT;
            }
            else
            {
                // underCnt > 0, percentCnt > 0, charCnt > 0
                
                // case 8
                // �Ϲݹ��ڿ� '_', '%'�� ������
                aFormatInfo->type = MTC_FORMAT_ALL;
            }
        }
    }

    // case 6-1�� ��� '%'�� 1�� �� �ΰ� ������ �����Ѵ�.
    if ( aFormatInfo->type == MTC_FORMAT_NORMAL_ONE_PERCENT )
    {
        // '%'�� 1���� ��� ������ ���� ǥ���� �� �ִ�.
        // [head]%[tail]

        if( aLanguage->id != MTL_UTF16_ID )
        {
            // [head]
            aFormatInfo->head = aFormatInfo->pattern;
            aFormatInfo->headSize = aFormatInfo->firstPercent - 
                aFormatInfo->pattern;
            // [tail]
            aFormatInfo->tail = aFormatInfo->firstPercent + 1;
            aFormatInfo->tailSize = sPatternSize - 
                aFormatInfo->headSize - 1;
        }
        else
        {
            // PROJ-1579 NCHAR
            // UTF16������ '%'�� 2byte�̹Ƿ� �Ʒ��� ���� ����Ѵ�.

            // [head]
            aFormatInfo->head = aFormatInfo->pattern;
            aFormatInfo->headSize = aFormatInfo->firstPercent - 
                aFormatInfo->pattern;
            // [tail]
            aFormatInfo->tail = aFormatInfo->firstPercent + 2;
            aFormatInfo->tailSize = sPatternSize - 
                aFormatInfo->headSize - 2;
        }
    }
    else
    {
        // Nothing to do.
    }
    
    // PROJ-1753 one pass like
    // case 5,6-2,8�� ���� ����ȭ
    if ( ( aFormatInfo->type == MTC_FORMAT_NORMAL_UNDER ) ||
         ( aFormatInfo->type == MTC_FORMAT_NORMAL_MANY_PERCENT ) ||
         ( aFormatInfo->type == MTC_FORMAT_ALL ) )
    {
        IDE_TEST( aCallBack->alloc( aCallBack->info,
                                    ID_SIZEOF( mtcLikeBlockInfo ) * sPatternSize,
                                    (void**) & aFormatInfo->blockInfo )
                  != IDE_SUCCESS );
            
        IDE_TEST( aCallBack->alloc( aCallBack->info,
                                    ID_SIZEOF( UChar ) * sPatternSize,
                                    (void**) & aFormatInfo->refinePattern )
                  != IDE_SUCCESS );
            
        IDE_TEST( getMoreInfoFromPatternMB( aFormat,
                                            aFormatLen,
                                            aEscape,
                                            aEscapeLen,
                                            aFormatInfo->blockInfo,
                                            &(aFormatInfo->blockCnt),
                                            aFormatInfo->refinePattern,
                                            aLanguage )
                  != IDE_SUCCESS );
    }
    else
    {
        // BUG-35504
        // new like������ format length ������ �ִ�.
        IDE_TEST_RAISE( aFormatLen > MTC_LIKE_PATTERN_MAX_SIZE,
                        ERR_LONG_PATTERN );
    }
   
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_LONG_PATTERN );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_LONG_PATTERN )); 
    
    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_INVALID_ESCAPE ));
    
    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE ));
   
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeFormatInfo( mtcNode            * aNode,
                          mtcTemplate        * aTemplate,
                          mtcStack           * aStack,
                          mtcLikeFormatInfo ** aFormatInfo,
                          UShort             * aFormatLength,
                          mtcCallBack        * aCallBack)
{
    mtcLikeFormatInfo  * sFormatInfo = NULL;
    mtcNode            * sIndexNode;
    mtcColumn          * sIndexColumn;
    mtcNode            * sFormatNode;
    mtcNode            * sConvFormatNode;
    mtcColumn          * sFormatColumn;
    mtcNode            * sEscapeNode;
    mtcNode            * sConvEscapeNode;
    mtcColumn          * sEscapeColumn;
    const mtdCharType  * sCharFormat;
    const mtdEcharType * sEcharFormat;
    const mtdCharType  * sEscape;
    UChar              * sFormat;

    // BUG-40992 FATAL when using _prowid
    // ������ ��� mtcStack �� column ���� �̿��ϸ� �ȴ�.
    sIndexColumn     = aStack[1].column;

    sIndexNode       = aNode->arguments;
    sFormatNode      = sIndexNode->next;
    sEscapeNode      = sFormatNode->next;
    
    sConvFormatNode = mtf::convertedNode( sFormatNode,
                                          aTemplate );

    if( ( sConvFormatNode == sFormatNode ) &&
        ( ( aTemplate->rows[sFormatNode->table].lflag & MTC_TUPLE_TYPE_MASK )
          == MTC_TUPLE_TYPE_CONSTANT ) )
    {
        sFormatColumn = &(aTemplate->rows[sFormatNode->table].
                          columns[sFormatNode->column]);

        if ( ( sFormatColumn->module->id == MTD_ECHAR_ID ) ||
             ( sFormatColumn->module->id == MTD_EVARCHAR_ID ) )
        {
            // format�� ����̹Ƿ� �ݵ�� default policy�̴�.
            IDE_ASSERT_MSG( sFormatColumn->mColumnAttr.mEncAttr.mPolicy[0] == '\0',
                            "sFormatColumn->policy : %c \n",
                            sFormatColumn->mColumnAttr.mEncAttr.mPolicy[0] );
            
            sEcharFormat = (const mtdEcharType *)
                mtd::valueForModule(
                    (smiColumn*)&(sFormatColumn->column),
                    aTemplate->rows[sFormatNode->table].row,
                    MTD_OFFSET_USE,
                    mtdEchar.staticNull );
            
            sFormat = (UChar*) sEcharFormat->mValue;
            *aFormatLength = sEcharFormat->mCipherLength;
        }
        else
        {    
            sCharFormat = (const mtdCharType *)
                mtd::valueForModule(
                    (smiColumn*)&(sFormatColumn->column),
                    aTemplate->rows[sFormatNode->table].row,
                    MTD_OFFSET_USE,
                    mtdChar.staticNull );
            
            sFormat = (UChar*) sCharFormat->value;
            *aFormatLength = sCharFormat->length;
        }

        if ( sEscapeNode == NULL )
        {
            // format������ (sLikeFormatInfo) ������ ������ �Ҵ�
            IDE_TEST( aCallBack->alloc( aCallBack->info,
                                        ID_SIZEOF(mtcLikeFormatInfo),
                                        (void**) & sFormatInfo )
                      != IDE_SUCCESS );

            // formatInfo �ʱ�ȭ
            idlOS::memset( (void*)sFormatInfo,
                           0x00,
                           ID_SIZEOF(mtcLikeFormatInfo));
            sFormatInfo->type = MTC_FORMAT_ALL;
            
            // format pattern�� ������ ������ �Ҵ�
            if ( *aFormatLength > 0 )
            {
                IDE_TEST( aCallBack->alloc( aCallBack->info,
                                            *aFormatLength,
                                            (void**) & sFormatInfo->pattern )
                          != IDE_SUCCESS );

                IDE_TEST( aCallBack->alloc( aCallBack->info,
                                            ID_SIZEOF( mtcLikeBlockInfo) * (*aFormatLength),
                                            (void**) & sFormatInfo->blockInfo )
                          != IDE_SUCCESS );
            
                // PROJ-1755
                // format string�� �з�
                if ( sIndexColumn->language == &mtlAscii )
                {
                    IDE_TEST( classifyFormatString( sFormat,
                                                    (*aFormatLength),
                                                    NULL,
                                                    0,
                                                    sFormatInfo,
                                                    aCallBack,
                                                    sIndexColumn->language )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( classifyFormatStringMB( sFormat,
                                                      (*aFormatLength),
                                                      NULL,
                                                      0,
                                                      sFormatInfo,
                                                      aCallBack,
                                                      sIndexColumn->language )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            sConvEscapeNode = mtf::convertedNode( sEscapeNode,
                                                  aTemplate );
            
            if( ( sConvEscapeNode == sEscapeNode ) &&
                ( ( aTemplate->rows[sEscapeNode->table].lflag & MTC_TUPLE_TYPE_MASK )
                  == MTC_TUPLE_TYPE_CONSTANT ) )
            {
                sEscapeColumn = &(aTemplate->rows[sEscapeNode->table].
                                  columns[sEscapeNode->column]);
                
                sEscape = (const mtdCharType *)
                    mtd::valueForModule(
                        (smiColumn*)&(sEscapeColumn->column),
                        aTemplate->rows[sEscapeNode->table].row,
                        MTD_OFFSET_USE,
                        mtdChar.staticNull );

                // format������ (sLikeFormatInfo) ������ ������ �Ҵ�
                IDE_TEST( aCallBack->alloc( aCallBack->info,
                                            ID_SIZEOF(mtcLikeFormatInfo),
                                            (void**) & sFormatInfo )
                          != IDE_SUCCESS );
                
                // formatInfo �ʱ�ȭ
                idlOS::memset( (void*)sFormatInfo,
                               0x00,
                               ID_SIZEOF(mtcLikeFormatInfo));
                sFormatInfo->type = MTC_FORMAT_ALL;
                
                // format pattern�� ������ ������ �Ҵ�
                if ( (*aFormatLength) > 0 )
                {
                    IDE_TEST( aCallBack->alloc( aCallBack->info,
                                                (*aFormatLength),
                                                (void**) & sFormatInfo->pattern )
                              != IDE_SUCCESS );

                    IDE_TEST( aCallBack->alloc( aCallBack->info,
                                                ID_SIZEOF( mtcLikeBlockInfo) * (*aFormatLength),
                                                (void**) & sFormatInfo->blockInfo )
                              != IDE_SUCCESS );
                    
                    // PROJ-1755
                    // format string�� �з�
                    if ( sIndexColumn->language == &mtlAscii )
                    {
                        IDE_TEST( classifyFormatString( sFormat,
                                                        (*aFormatLength),
                                                        sEscape->value,
                                                        sEscape->length,
                                                        sFormatInfo,
                                                        aCallBack,
                                                        sIndexColumn->language )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        IDE_TEST( classifyFormatStringMB( sFormat,
                                                          (*aFormatLength),
                                                          sEscape->value,
                                                          sEscape->length,
                                                          sFormatInfo,
                                                          aCallBack,
                                                          sIndexColumn->language )
                                  != IDE_SUCCESS );
                    }
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    *aFormatInfo = sFormatInfo;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeEstimateCharFast( mtcNode*     aNode,
                                mtcTemplate* aTemplate,
                                mtcStack*    aStack,
                                SInt,
                                mtcCallBack* aCallBack )
{
    mtcColumn         * sIndexColumn;
    mtcLikeFormatInfo * sFormatInfo;
    UShort              sFormatLen;    

    // BUG-40992 FATAL when using _prowid
    // ������ ��� mtcStack �� column ���� �̿��ϸ� �ȴ�.
    sIndexColumn     = aStack[1].column;

    if ( mtfLikeFormatInfo( aNode, aTemplate, aStack, & sFormatInfo, &sFormatLen, aCallBack) == IDE_SUCCESS )
    {
        if ( sFormatInfo != NULL )
        {
            switch ( sFormatInfo->type )
            {
                case MTC_FORMAT_NORMAL:
                {
                    // search_value = format pattern
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateEqualFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_UNDER:
                {
                    // character_length( search_value ) = underCnt
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLengthFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLengthMBFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    break;
                }
                case MTC_FORMAT_PERCENT:
                {
                    // search_value is not null
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateIsNotNullFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_NORMAL_ONE_PERCENT:
                {
                    // search_value = [head]%[tail]
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateOnePercentFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_UNDER_PERCENT:
                {
                    // character_length( search_value ) >= underCnt
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLengthFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLengthMBFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    break;
                }
                case MTC_FORMAT_ALL:
                case MTC_FORMAT_NORMAL_MANY_PERCENT:
                case MTC_FORMAT_NORMAL_UNDER:
                {
                    if ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                    {                        
                        if ( sIndexColumn->language == &mtlAscii )
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                                mtfLikeCalculateNormalFast; 
                            aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                                sFormatInfo;
                        }
                        else
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                                mtfLikeCalculateMBNormalFast;
                            aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                                sFormatInfo;
                        }
                    }
                    else
                    {
                        // nothing to do
                    }                    
                    break;
                }
                case MTC_FORMAT_NULL:
                {
                    // Nothing to do.
                    break;
                }
                default:
                {
                    ideLog::log( IDE_ERR_0,
                                 "sFormatInfo->type : %u\n",
                                 sFormatInfo->type );

                    IDE_ASSERT( 0 );
//                    break;
                }
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;
}    

IDE_RC mtfLikeEstimateBitFast( mtcNode*     aNode,
                               mtcTemplate* aTemplate,
                               mtcStack*    aStack,
                               SInt      /* aRemain */,
                               mtcCallBack* aCallBack )
{
    mtcLikeFormatInfo * sFormatInfo;
    UShort              sFormatLen;    
    
    if ( mtfLikeFormatInfo( aNode, aTemplate, aStack, & sFormatInfo, &sFormatLen, aCallBack ) == IDE_SUCCESS )
    {
        if ( sFormatInfo != NULL )
        {
            switch ( sFormatInfo->type )
            {
                case MTC_FORMAT_NORMAL:
                {
                    // search_value = format pattern
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateEqualFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_UNDER:
                {
                    // character_length( search_value ) = underCnt
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateLengthFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_PERCENT:
                {
                    // search_value is not null
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateIsNotNullFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_NORMAL_ONE_PERCENT:
                {
                    // search_value = [head]%[tail]
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateOnePercentFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_UNDER_PERCENT:
                {
                    // character_length( search_value ) >= underCnt
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateLengthFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_NORMAL_MANY_PERCENT:
                case MTC_FORMAT_NORMAL_UNDER:
                case MTC_FORMAT_ALL:
                {
                    if ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                    {   
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateNormalFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    else
                    {
                        // nothing to do
                    }
                    break;
                }
                case MTC_FORMAT_NULL:
                {
                    // Nothing to do.
                    break;
                }
                default:
                {
                    ideLog::log( IDE_ERR_0,
                                 "sFormatInfo->type : %u\n",
                                 sFormatInfo->type );

                    IDE_ASSERT( 0 );
//                    break;
                }
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;
}

IDE_RC mtfLikeEstimateXlobLocatorFast( mtcNode*     aNode,
                                       mtcTemplate* aTemplate,
                                       mtcStack*    aStack,
                                       SInt      /* aRemain */,
                                       mtcCallBack* aCallBack )
{
    mtcColumn         * sIndexColumn;
    mtcLikeFormatInfo * sFormatInfo;
    UShort              sFormatLen;    

    // BUG-40992 FATAL when using _prowid
    // ������ ��� mtcStack �� column ���� �̿��ϸ� �ȴ�.
    sIndexColumn     = aStack[1].column;

    if ( mtfLikeFormatInfo( aNode, aTemplate, aStack, & sFormatInfo, &sFormatLen, aCallBack ) == IDE_SUCCESS )
    {
        if ( sFormatInfo != NULL )
        {
            switch ( sFormatInfo->type )
            {
                case MTC_FORMAT_NORMAL:
                {
                    // search_value = format pattern
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateEqual4XlobLocatorFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_UNDER:
                {
                    // character_length( search_value ) = underCnt
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4XlobLocatorFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4XlobLocatorMBFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    break;
                }
                case MTC_FORMAT_PERCENT:
                {
                    // search_value is not null
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateIsNotNull4XlobLocatorFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_NORMAL_ONE_PERCENT:
                {
                    // search_value = [head]%[tail]
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateOnePercent4XlobLocatorFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_UNDER_PERCENT:
                {
                    // character_length( search_value ) >= underCnt
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4XlobLocatorFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4XlobLocatorMBFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    break;
                }
                case MTC_FORMAT_NORMAL_MANY_PERCENT:
                case MTC_FORMAT_NORMAL_UNDER:
                case MTC_FORMAT_ALL:
                {
                    if ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                    {
                        if ( sIndexColumn->language == &mtlAscii )
                        {                        
                            aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                                mtfLikeCalculate4XlobLocatorNormalFast;
                            aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                                sFormatInfo;
                        }
                        else
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                                mtfLikeCalculate4XlobLocatorMBNormalFast;
                            aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                                sFormatInfo;
                        }
                    }
                    else
                    {
                        // nothing to do
                    }
                    
                    break;
                }
                case MTC_FORMAT_NULL:
                {
                    // Nothing to do.
                    break;
                }
                default:
                {
                    ideLog::log( IDE_ERR_0,
                                 "sFormatInfo->type : %u\n",
                                 sFormatInfo->type );

                    IDE_ASSERT( 0 );
//                  break;
                }
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;
}

IDE_RC mtfLikeEstimateEcharFast( mtcNode*     aNode,
                                 mtcTemplate* aTemplate,
                                 mtcStack*    aStack,
                                 SInt,
                                 mtcCallBack* aCallBack )
{
    mtcColumn         * sIndexColumn;
    mtcLikeFormatInfo * sFormatInfo;
    UShort              sFormatLen;    

    // BUG-40992 FATAL when using _prowid
    // ������ ��� mtcStack �� column ���� �̿��ϸ� �ȴ�.
    sIndexColumn     = aStack[1].column;

    if ( mtfLikeFormatInfo( aNode, aTemplate, aStack, & sFormatInfo, &sFormatLen, aCallBack ) == IDE_SUCCESS )
    {
        if ( sFormatInfo != NULL )
        {
            switch ( sFormatInfo->type )
            {
                case MTC_FORMAT_NORMAL:
                {
                    // search_value = format pattern
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateEqual4EcharFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_UNDER:
                {
                    // character_length( search_value ) = underCnt
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4EcharFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4EcharMBFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    break;
                }
                case MTC_FORMAT_PERCENT:
                {
                    // search_value is not null
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateIsNotNull4EcharFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_NORMAL_ONE_PERCENT:
                {
                    // search_value = [head]%[tail]
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateOnePercent4EcharFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_UNDER_PERCENT:
                {
                    // character_length( search_value ) >= underCnt
                    if( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4EcharFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4EcharMBFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    break;
                }
                case MTC_FORMAT_NORMAL_MANY_PERCENT:
                case MTC_FORMAT_NORMAL_UNDER:
                case MTC_FORMAT_ALL:
                {
                    if ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                    {
                        if ( sIndexColumn->language == &mtlAscii )
                        {                        
                            aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                                mtfLikeCalculate4EcharNormalFast;
                            aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                                sFormatInfo;
                        }
                        else
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                                mtfLikeCalculate4EcharMBNormalFast;
                            aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                                sFormatInfo;
                        }
                    }
                    else
                    {
                        // nothing to do
                    }
                    
                    break;
                }
                case MTC_FORMAT_NULL:
                {
                    // Nothing to do.
                    break;
                }
                default:
                {
                    ideLog::log( IDE_ERR_0,
                                 "sFormatInfo->type : %u\n",
                                 sFormatInfo->type );
                                 
                    IDE_ASSERT( 0 );                                                     
//                    break;
                }
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

static IDE_RC mtfLikeKey( UChar           * aKey,
                          UShort          * aKeyLen,
                          UShort            aKeyMax,
                          const UChar     * aSource,
                          UShort            aSourceLen,
                          const UChar     * aEscape,
                          UShort            aEscapeLen,
                          idBool          * aIsEqual,
                          const mtlModule * /* sLanguage */ )
{
/***********************************************************************
 *
 * Description : Like Key
 *
 * Implementation :
 *
 ***********************************************************************/

    UChar    sEscape = 0;
    idBool   sNullEscape;
    UChar  * sIndex;
    UChar  * sFence;
        
    // escape ���� ���� 
    if( aEscapeLen < 1 )
    {
        sNullEscape = ID_TRUE;
    }
    else if( aEscapeLen == 1 )
    {
        sNullEscape = ID_FALSE;
        sEscape = *aEscape;
    }
    else
    {
        IDE_RAISE( ERR_INVALID_ESCAPE );
    }

    sIndex = (UChar*) aSource;
    sFence = sIndex + aSourceLen;
    *aKeyLen = 0;
    while( sIndex < sFence )
    {        
        if( (sNullEscape == ID_FALSE) && (*sIndex == sEscape) )
        {
            // To Fix PR-13004
            // ABR ������ ���Ͽ� ������Ų �� �˻��Ͽ��� ��
            sIndex++;
            
            // escape ������ ���,
            // escape ���� ���ڰ� '%','_' �������� �˻�
            IDE_TEST_RAISE( sIndex >= sFence, ERR_INVALID_LITERAL );
            
            // To Fix BUG-12578
            IDE_TEST_RAISE( (*sIndex != (UShort)'%') &&
                            (*sIndex != (UShort)'_') &&
                            (*sIndex != sEscape), // sEsacpe�� null�� �ƴ�
                            ERR_INVALID_LITERAL );
        }
        else if( (*sIndex == (UShort)'%') ||
                 (*sIndex == (UShort)'_') )
        {
            // Ư�������� ��� 
            break;
        }
        else
        {
            // �Ϲ� ������ ���
        }

        aKey[(*aKeyLen)++] = *sIndex;
        sIndex++;

        if( (*aKeyLen)+1 > aKeyMax )
        {
            // fix BUG-19639
            
            break;
        }
        // To Fix PR-13368
        /*
        IDE_TEST_RAISE( (*aKeyLen) + 1 >= aKeyMax, ERR_THROUGH );
        */
    }

    if ( sIndex == sFence )
    {
        *aIsEqual = ID_TRUE;
    }
    else
    {
        // fix BUG-19639
        if( (*aKeyLen) > aKeyMax )
        {
            *aIsEqual = ID_TRUE;
        }
        else
        {
            *aIsEqual = ID_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_INVALID_ESCAPE ));

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE ));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//fix for BUG-15930
static IDE_RC mtfLikeKeyMB( UChar           * aKey,
                            UShort          * aKeyLen,
                            UShort            aKeyMax,
                            const UChar     * aSource,
                            UShort            aSourceLen,
                            const UChar     * aEscape,
                            UShort            aEscapeLen,
                            idBool          * aIsEqual,
                            const mtlModule * aLanguage )
{
/***********************************************************************
 *
 * Description : Like Key
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool   sNullEscape;
    UChar  * sIndex;
    UChar  * sIndexPrev;
    UChar  * sFence;
    idBool   sEqual;
    idBool   sEqual1;
    idBool   sEqual2;
    idBool   sEqual3;
    UChar    sSize;

    if( aLanguage->id == MTL_UTF16_ID )
    {
        // escape ���� ����
        if( aEscapeLen < MTL_UTF16_PRECISION )
        {
            sNullEscape = ID_TRUE;
        }
        else if( aEscapeLen == MTL_UTF16_PRECISION )
        {
            sNullEscape = ID_FALSE;
        }
        else
        {
            IDE_RAISE( ERR_INVALID_ESCAPE );
        }        
    }
    else
    {    
        // escape ���� ����
        if( aEscapeLen < 1 )
        {
            sNullEscape = ID_TRUE;
        }
        else if( aEscapeLen == 1 )
        {
            sNullEscape = ID_FALSE;
        }
        else
        {
            IDE_RAISE( ERR_INVALID_ESCAPE );
        }
    }

    sIndex     = (UChar*) aSource;
    sIndexPrev = sIndex;
    sFence     = sIndex + aSourceLen;
    *aKeyLen   = 0;

    while( sIndex < sFence )
    {
        sSize =  mtl::getOneCharSize( sIndex,
                                      sFence,
                                      aLanguage );

        if( sNullEscape == ID_FALSE )
        {
            sEqual = mtc::compareOneChar( sIndex,
                                          sSize,
                                          (UChar*)aEscape,
                                          aEscapeLen );
        }
        else
        {
            sEqual = ID_FALSE;
        }

        if( sEqual == ID_TRUE )
        {
            // To Fix PR-13004
            // ABR ������ ���Ͽ� ������Ų �� �˻��Ͽ��� ��
            (void)mtf::nextChar( sFence,
                                 &sIndex,
                                 &sIndexPrev,
                                 aLanguage );
            
            sSize =  mtl::getOneCharSize( sIndex,
                                          sFence,
                                          aLanguage );

            // escape ������ ���,
            // escape ���� ���ڰ� '%','_' �������� �˻�
            sEqual1 = mtc::compareOneChar( sIndex,
                                           sSize,
                                           aLanguage->specialCharSet[MTL_PC_IDX],
                                           aLanguage->specialCharSize );
            
            sEqual2 = mtc::compareOneChar( sIndex,
                                           sSize,
                                           aLanguage->specialCharSet[MTL_UB_IDX],
                                           aLanguage->specialCharSize );
            
            sEqual3 = mtc::compareOneChar( sIndex,
                                           sSize,
                                           (UChar*)aEscape,
                                           aEscapeLen );
            
            // To Fix BUG-12578
            IDE_TEST_RAISE( (sEqual1 != ID_TRUE) &&
                            (sEqual2 != ID_TRUE) &&
                            (sEqual3 != ID_TRUE),
                            ERR_INVALID_LITERAL );
        }
        else
        {
            sEqual1 = mtc::compareOneChar( sIndex,
                                           sSize,
                                           aLanguage->specialCharSet[MTL_PC_IDX],
                                           aLanguage->specialCharSize );
                
            sEqual2 = mtc::compareOneChar( sIndex,
                                           sSize,
                                           aLanguage->specialCharSet[MTL_UB_IDX],
                                           aLanguage->specialCharSize );
          
            if( (sEqual1 == ID_TRUE) ||
                (sEqual2 == ID_TRUE) )
            {
                // Ư�������� ���
                break;
            }
            else
            {
                // �Ϲ� ������ ���
            }
        }

        (void)mtf::nextChar( sFence,
                             &sIndex,
                             &sIndexPrev,
                             aLanguage );
        
        if( *aKeyLen + (sIndex - sIndexPrev) > aKeyMax )
        {
            sIndex = sIndexPrev;

            break;
        }
        else
        {
            idlOS::memcpy( &aKey[*aKeyLen],
                           sIndexPrev,
                           sIndex - sIndexPrev );
            *aKeyLen += sIndex - sIndexPrev;
        }
        // To Fix PR-13368
        /*
        IDE_TEST_RAISE( (*aKeyLen) + 1 >= aKeyMax, ERR_THROUGH );
        */
    }

    if ( sIndex == sFence )
    {
        *aIsEqual = ID_TRUE;
    }
    else
    {
        // fix BUG-19639
        if( (*aKeyLen) > aKeyMax )
        {
            *aIsEqual = ID_TRUE;
        }
        else
        {
            *aIsEqual = ID_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_INVALID_ESCAPE ));

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE ));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfExtractRange( mtcNode*      aNode,
                        mtcTemplate*  aTemplate,
                        mtkRangeInfo* aInfo,
                        smiRange*     aRange )
{
/***********************************************************************
 *
 * Description : Extract Range
 *
 * Implementation :
 *
 ***********************************************************************/
    
    static const mtdCharType sEscapeEmpty = { 0, { 0 } };
    
    mtcNode          * sIndexNode;
    mtcNode          * sValueNode1;
    mtcNode          * sValueNode2;
    mtdCharType      * sValue;
    const mtdCharType* sEscape;
    mtdCharType      * sValue1;
    mtdCharType      * sValue2;
    mtkRangeCallBack * sMinimumCallBack;
    mtkRangeCallBack * sMaximumCallBack;
    mtcColumn        * sValueColumn;
    mtcColumn        * sEscapeColumn;
    mtcColumn        * sValueColumn1;
    mtcColumn        * sValueColumn2;
    idBool             sIsEqual;

    sIndexNode  = aNode->arguments;
    sValueNode1 = sIndexNode->next;
    sValueNode2 = sValueNode1->next;
    
    sValueNode1 = mtf::convertedNode( sValueNode1, aTemplate );
    
    sValueColumn = aTemplate->rows[sValueNode1->table].columns
        + sValueNode1->column;
    sValue       = (mtdCharType*)
        ( (UChar*) aTemplate->rows[sValueNode1->table].row
          + sValueColumn->column.offset);
    if( sValueNode2 != NULL )
    {
        sValueNode2   = mtf::convertedNode( sValueNode2, aTemplate );
        sEscapeColumn = aTemplate->rows[sValueNode2->table].columns
            + sValueNode2->column;
        sEscape       = (mtdCharType*)
            ( (UChar*) aTemplate->rows[sValueNode2->table].row
              +sEscapeColumn->column.offset );
    }
    else
    {
        sEscape = &sEscapeEmpty;
    }
    
    sMinimumCallBack       = (mtkRangeCallBack*)( aRange + 1 );
    sMaximumCallBack       = sMinimumCallBack + 1;
    aRange->minimum.data   = sMinimumCallBack;
    aRange->maximum.data   = sMaximumCallBack;
    sMinimumCallBack->next = NULL;
    sMaximumCallBack->next = NULL;
    sMinimumCallBack->flag = 0;
    sMaximumCallBack->flag = 0;
    
    if( sValueColumn->module->isNull( sValueColumn,
                                      sValue ) == ID_TRUE )
    {
        if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
             aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
        {
            aRange->minimum.callback  = mtk::rangeCallBackGT4Mtd;
            aRange->maximum.callback  = mtk::rangeCallBackLT4Mtd;
        }
        else
        {
            if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                 ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
            {
                aRange->minimum.callback  = mtk::rangeCallBackGT4Stored;
                aRange->maximum.callback  = mtk::rangeCallBackLT4Stored;
            }
            else
            {
                /* PROJ-2433 */
                aRange->minimum.callback  = mtk::rangeCallBackGT4IndexKey;
                aRange->maximum.callback  = mtk::rangeCallBackLT4IndexKey;
            }
        }
        
        sMinimumCallBack->compare = mtk::compareMinimumLimit;
        sMaximumCallBack->compare = mtk::compareMinimumLimit;

        aRange->prev              = NULL;
        aRange->next              = NULL;

        /* BUG-9889 fix by kumdory. 2005-01-24 */
        sMinimumCallBack->columnIdx  =  aInfo->columnIdx;
        //sMinimumCallBack->columnDesc = NULL;
        //sMinimumCallBack->valueDesc  = NULL;
        sMinimumCallBack->value      = NULL;

        sMaximumCallBack->columnIdx  =  aInfo->columnIdx;
        //sMaximumCallBack->columnDesc = NULL;
        //sMaximumCallBack->valueDesc  = NULL;
        sMaximumCallBack->value      = NULL;
    }
    else
    {
        sValueColumn1 = (mtcColumn*)( sMaximumCallBack + 1 );
        sValueColumn2 = sValueColumn1 + 1;
        
        sValue1 = (mtdCharType*)( sValueColumn2 + 1 );
        sValue2 = (mtdCharType*)((UChar*)sValue1 + MTC_LIKE_KEY_SIZE);
       
        IDE_TEST( mtc::initializeColumn( sValueColumn1,
                                         & mtdVarchar,
                                         1,
                                         MTC_LIKE_KEY_PRECISION,
                                         0 )
                  != IDE_SUCCESS );
        
        sValueColumn1->column.offset = 0;  

        IDE_TEST( mtc::initializeColumn( sValueColumn2,
                                         & mtdVarchar,
                                         1,
                                         MTC_LIKE_KEY_PRECISION,
                                         0 )
                  != IDE_SUCCESS );
        
        sValueColumn2->column.offset = 0;                     

        //fix For BUG-15930  
        // like key ���� 
        if( (const mtlModule *) (sValueColumn->language) == &mtlAscii )
        {
            IDE_TEST( mtfLikeKey( sValue1->value,
                                  &sValue1->length,
                                  MTC_LIKE_KEY_PRECISION,
                                  sValue->value,
                                  sValue->length,
                                  sEscape->value,
                                  sEscape->length,
                                  & sIsEqual,
                                  (const mtlModule *) (sValueColumn->language) )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( mtfLikeKeyMB( sValue1->value,
                                    &sValue1->length,
                                    MTC_LIKE_KEY_PRECISION,
                                    sValue->value,
                                    sValue->length,
                                    sEscape->value,
                                    sEscape->length,
                                    & sIsEqual,
                                    (const mtlModule *) (sValueColumn->language) )
                        != IDE_SUCCESS );
        }

        //--------------------------------------------------
        // To Fix BUG-12306
        // - like key�� 'F%', 'F_' Ÿ���� ���,
        //   like key ���� ������ ���ڸ� �ϳ� ����
        //   ex) LIKE 'aa%'�� ��� : sValue1->value 'aa'
        //                           sValue2->value 'ab'
        // - like key�� 'F' Ÿ���� ���,
        //   ex) LIKE 'aa'�� ���  : sValue1->value 'aa'
        //                           sValue2->value 'aa'
        //--------------------------------------------------
        
        idlOS::memcpy( sValue2->value, sValue1->value, sValue1->length );
        sValue2->length = sValue1->length;
        
        if ( sIsEqual == ID_FALSE )
        {
            // like key�� 'F%', 'F_' Ÿ���� ���,
            for( sValue2->length = sValue1->length;
                 sValue2->length > 0;
                 sValue2->length-- )
            {
                sValue2->value[sValue2->length-1]++;
                if( sValue2->value[sValue2->length-1] != 0 )
                {
                    break;
                }
            }
        }
        else
        {
            // like key�� 'F' Ÿ���� ���
        }
        
        aRange->prev = NULL;
        aRange->next = NULL;
        
        if( aInfo->direction == MTD_COMPARE_ASCENDING )
        {
            // To Fix BUG-12306
            if ( sIsEqual == ID_TRUE )
            {
                //---------------------------
                // RangeCallBack ����
                //---------------------------

                if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
                     aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
                {
                    // mtd type�� column value�� ���� range callback 
                    aRange->minimum.callback     = mtk::rangeCallBackGE4Mtd;
                    aRange->maximum.callback     = mtk::rangeCallBackLE4Mtd;
                }
                else
                {
                    if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                         ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
                    {
                        /* MTD_COMPARE_STOREDVAL_MTDVAL
                           stored type�� column value�� ���� range callback */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4Stored;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4Stored;
                    }
                    else
                    {
                        /* PROJ-2433 */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4IndexKey;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4IndexKey;
                    }
                }
            }
            else
            {
                if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
                     aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
                {
                    // mtd type�� column value�� ���� range callback 
                    aRange->minimum.callback     = mtk::rangeCallBackGE4Mtd;
                    aRange->maximum.callback     = mtk::rangeCallBackLT4Mtd;
                }
                else
                {
                    if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                         ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
                    {
                        /* MTD_COMPARE_STOREDVAL_MTDVAL
                           stored type�� column value�� ���� range callback */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4Stored;
                        aRange->maximum.callback     = mtk::rangeCallBackLT4Stored;
                    }
                    else
                    {
                        /* PROJ-2433 */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4IndexKey;
                        aRange->maximum.callback     = mtk::rangeCallBackLT4IndexKey;
                    }
                }
            }

            //---------------------------
            // MinimumCallBack ����
            //---------------------------
            
            sMinimumCallBack->columnIdx  =  aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->columnDesc, aInfo->column ) )
            {
                sMinimumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->valueDesc, sValueColumn1 ) )
            {
                sMinimumCallBack->valueDesc  = *sValueColumn1;
            }
            sMinimumCallBack->value      = sValue1;
            
            if( sValue1->length > 0 )
            {
                // PROJ-1364
                if( aInfo->isSameGroupType == ID_FALSE )
                {
                    sMinimumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_SAMEGROUP_FALSE;

                    sMinimumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_DIRECTION_ASC;
                    
                    sMinimumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                                                         [aInfo->direction];
                }
                else
                {
                    sMinimumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_SAMEGROUP_TRUE;

                    sMinimumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_DIRECTION_ASC;
                    
                    sMinimumCallBack->compare =
                        mtd::findCompareFunc( aInfo->column,
                                              sValueColumn1,
                                              aInfo->compValueType,
                                              aInfo->direction );
                }
            }
            else
            {
                
                sMinimumCallBack->compare = mtk::compareMinimumLimit;
            }

            //---------------------------
            // MaximumCallBack ���� ����
            //---------------------------
        
            sMaximumCallBack->columnIdx  =  aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->columnDesc, aInfo->column ) )
            {
                sMaximumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->valueDesc, sValueColumn2 ) )
            {
                sMaximumCallBack->valueDesc  = *sValueColumn2;
            }
            
            if( sValue2->length > 0 )
            {
                // PROJ-1364
                if( aInfo->isSameGroupType == ID_FALSE )
                {
                    sMaximumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_SAMEGROUP_FALSE;

                    sMaximumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_DIRECTION_ASC;
                    
                    sMaximumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                                                         [aInfo->direction];
                }
                else
                {
                    sMaximumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_SAMEGROUP_TRUE;

                    sMaximumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_DIRECTION_ASC;
                    
                    sMaximumCallBack->compare =
                        mtd::findCompareFunc( aInfo->column,
                                              sValueColumn2,
                                              aInfo->compValueType,
                                              aInfo->direction );
                }
            }
            else
            {
                if ( ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_MTDVAL ) )
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Mtd;
                }
                else
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Stored;
                }
            }

            sMaximumCallBack->value = sValue2;
        }
        else
        {
            // To Fix BUG-12306
            if ( sIsEqual == ID_TRUE )
            {
                if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
                     aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
                {
                    // mtd type�� column value�� ���� range callback 
                    aRange->minimum.callback     = mtk::rangeCallBackGE4Mtd;
                    aRange->maximum.callback     = mtk::rangeCallBackLE4Mtd;
                }
                else
                {
                    if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                         ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
                    {
                        /* MTD_COMPARE_STOREDVAL_MTDVAL
                           stored type�� column value�� ���� range callback */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4Stored;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4Stored;
                    }
                    else
                    {
                        /* PROJ-2433 */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4IndexKey;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4IndexKey;
                    }
                }
            }
            else
            {
                //---------------------------
                // RangeCallBack ����
                //---------------------------

                if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
                     aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
                {
                    // mtd type�� column value�� ���� range callback 
                    aRange->minimum.callback     = mtk::rangeCallBackGT4Mtd;
                    aRange->maximum.callback     = mtk::rangeCallBackLE4Mtd;
                }
                else
                {
                    if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                         ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
                    {
                        /* MTD_COMPARE_STOREDVAL_MTDVAL
                           stored type�� column value�� ���� range callback */
                        aRange->minimum.callback     = mtk::rangeCallBackGT4Stored;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4Stored;
                    }
                    else
                    {
                        /* PROJ-2433*/
                        aRange->minimum.callback     = mtk::rangeCallBackGT4IndexKey;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4IndexKey;
                    }
                }
            }

            //---------------------------
            // MinimumCallBack ���� ����
            //---------------------------
            
            sMinimumCallBack->columnIdx  =  aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->columnDesc, aInfo->column ) )
            {
                sMinimumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->valueDesc, sValueColumn2 ) )
            {
                sMinimumCallBack->valueDesc  = *sValueColumn2;
            }
            sMinimumCallBack->value      = sValue2;
            
            if( sValue2->length > 0 )
            {
                // PROJ-1364
                if( aInfo->isSameGroupType == ID_FALSE )
                {
                    sMinimumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_SAMEGROUP_FALSE;

                    sMinimumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_DIRECTION_DESC;
                    
                    sMinimumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                                                         [aInfo->direction];
                }
                else
                {
                    sMinimumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_SAMEGROUP_TRUE;

                    sMinimumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_DIRECTION_DESC;
                    
                    sMinimumCallBack->compare =
                        mtd::findCompareFunc( aInfo->column,
                                              sValueColumn2,
                                              aInfo->compValueType,
                                              aInfo->direction );
                }
            }
            else
            {
                sMinimumCallBack->compare = mtk::compareMinimumLimit;
            }

            //---------------------------
            // MaximumCallBack ���� ����
            //---------------------------
            
            sMaximumCallBack->columnIdx  =  aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->columnDesc, aInfo->column ) )
            {
                sMaximumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->valueDesc, sValueColumn1 ) )
            {
                sMaximumCallBack->valueDesc  = *sValueColumn1;
            }
            sMaximumCallBack->value      = sValue1;
            
            if( sValue1->length > 0 )
            {
                // PROJ-1364
                if( aInfo->isSameGroupType == ID_FALSE )
                {
                    sMaximumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_SAMEGROUP_FALSE;

                    sMaximumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_DIRECTION_DESC;
                    
                    sMaximumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                                                         [aInfo->direction];
                }
                else
                {
                    sMaximumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_SAMEGROUP_TRUE;

                    sMaximumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_DIRECTION_DESC;
                    
                    sMaximumCallBack->compare =
                        mtd::findCompareFunc( aInfo->column,
                                              sValueColumn1,
                                              aInfo->compValueType,
                                              aInfo->direction );
                }
            }
            else
            {
                if ( ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_MTDVAL ) )
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Mtd;
                }
                else
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Stored;
                }
            }
        }
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if( ideGetErrorCode() == idERR_ABORT_idnReachEnd )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE));
    }
    else if( ideGetErrorCode() == idERR_ABORT_idnLikeEscape )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));
    }
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate( mtcNode*     aNode,
                         mtcStack*    aStack,
                         SInt         aRemain,
                         void*        aInfo,
                         mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *               ������ ���̿� ��� ��⿡ ���� �б⸦ �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    const mtdModule  * sModule;
    const mtcColumn  * sColumn; 
    mtdCharType      * sVarchar;
    UChar            * sString;
    UChar            * sStringFence;
    UChar            * sFormat;
    UChar              sEscape = 0;
    mtdBinaryType    * sTempBinary;
    UInt               sBlockCnt;
    mtcLikeBlockInfo * sBlock;
    UChar            * sRefineString;
    UShort             sFormatLen;
    mtdBooleanType     sResult;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sVarchar     = (mtdCharType*)aStack[1].value;
        sString      = sVarchar->value;
        sStringFence = sString + sVarchar->length;
        sVarchar     = (mtdCharType*)aStack[2].value;
        sFormat      = sVarchar->value;
        sFormatLen   = sVarchar->length;

        sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);

        sBlock = (mtcLikeBlockInfo*)(sTempBinary->mValue);

        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[2].column.offset);
        
        sRefineString = (UChar*)(sTempBinary->mValue);
        
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            sEscape = sVarchar->value[0];
        }
        else
        {
            // nothing to do
        }

        IDE_TEST( getMoreInfoFromPattern( sFormat,
                                          sFormatLen,
                                          &sEscape,
                                          1,
                                          sBlock,
                                          &sBlockCnt,
                                          sRefineString,
                                          NULL)
                  != IDE_SUCCESS);

        IDE_TEST( mtfLikeCalculateOnePass( sString,
                                           sStringFence,
                                           sBlock,
                                           sBlockCnt,
                                           &sResult )
                  != IDE_SUCCESS );

        *(mtdBooleanType*)aStack[0].value = sResult; 
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateEqualFast( mtcNode*     aNode,
                                  mtcStack*    aStack,
                                  SInt         aRemain,
                                  void*        aInfo,
                                  mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    mtdCharType       * sVarchar;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    SInt                sCompare;
    UInt                sStringLen;    
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sVarchar     = (mtdCharType*)aStack[1].value;
        sString      = sVarchar->value;
        sStringLen   = sVarchar->length;
        sStringFence = sVarchar->value + sStringLen;        
        sFormatInfo  = (mtcLikeFormatInfo*) aInfo;
        sVarchar     = (mtdCharType*)aStack[2].value;
        
        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_NORMAL,
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );

        if ( (sModule->id == MTD_CHAR_ID) &&
             (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE) )
        {
            if ( sStringLen >= sFormatInfo->patternSize )
            {
                sCompare = idlOS::memcmp( sString,
                                          sFormatInfo->pattern,
                                          sFormatInfo->patternSize );
                
                if ( ( sCompare == 0 ) &&
                     ( sStringLen > sFormatInfo->patternSize ) )
                {                    
                    for( ; sString < sStringFence; sString++ )
                    {
                        if ( *sString != ' ' )
                        {
                            sCompare = 1;
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                sCompare = -1;
            }
        }
        else
        {
            if ( sStringLen == sFormatInfo->patternSize )
            {
                sCompare = idlOS::memcmp( sString,
                                          sFormatInfo->pattern,
                                          sFormatInfo->patternSize );
            }
            else
            {
                sCompare = -1;
            }
        }

        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateIsNotNullFast( mtcNode*     aNode,
                                      mtcStack*    aStack,
                                      SInt         aRemain,
                                      void*        aInfo,
                                      mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    mtdCharType       * sVarchar;
    mtcLikeFormatInfo * sFormatInfo;
    UInt                sStringLen;    
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sVarchar    = (mtdCharType*)aStack[1].value;
        sStringLen  = sVarchar->length;        
        
        sFormatInfo = (mtcLikeFormatInfo*) aInfo;
        
        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_PERCENT,
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );

        if ( sStringLen > 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateLengthFast( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    mtdCharType       * sVarchar;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    SInt                sCompare;
    UInt                sStringLen;    
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sVarchar     = (mtdCharType*)aStack[1].value;
        sString      = sVarchar->value;
        sStringLen   = sVarchar->length;
        sStringFence = sString + sStringLen;        
        sFormatInfo  = (mtcLikeFormatInfo*) aInfo;
        sVarchar     = (mtdCharType*)aStack[2].value;
        
        IDE_ASSERT( sFormatInfo != NULL );

        if ( sFormatInfo->type == MTC_FORMAT_UNDER )
        {
            // character_length( search_value ) = underCnt
            
            if ( ( sModule->id == MTD_CHAR_ID ) &&
                 ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE ))
            {
                if ( sStringLen < sFormatInfo->patternSize )
                {
                    sCompare = -1;
                }
                else
                {
                    sCompare = 0;                    
                    
                    for( ; sString < sStringFence; sString++ )
                    {
                        if ( *sString != ' ' )
                        {
                            sCompare = 1;
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                }
            }
            else
            {
                if ( sStringLen == sFormatInfo->underCnt )
                {
                    sCompare = 0;
                }
                else
                {
                    sCompare = -1;
                }
            }
        }
        else
        {
            IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_UNDER_PERCENT,
                            "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                            sFormatInfo->type );

            // character_length( search_value ) >= underCnt
            
            if ( sStringLen >= sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }
        
        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateOnePercentFast( mtcNode*     aNode,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       void*        aInfo,
                                       mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    mtdCharType       * sVarchar;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    SInt                sCompare;
    UInt                sStringLen;    
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sVarchar     = (mtdCharType*)aStack[1].value;
        sString      = sVarchar->value;
        sStringFence = sString + sVarchar->length;
        sStringLen   = sVarchar->length;        
        sFormatInfo  = (mtcLikeFormatInfo*) aInfo;
        sVarchar     = (mtdCharType*)aStack[2].value;
        
        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_NORMAL_ONE_PERCENT,
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );
        IDE_ASSERT_MSG( sFormatInfo->percentCnt == 1,
                        "sFormatInfo->percentCnt : %"ID_UINT32_FMT"\n",
                        sFormatInfo->percentCnt );

        // search_value = [head]%[tail]
        
        if ( sStringLen >= sFormatInfo->headSize + sFormatInfo->tailSize )
        {
            if ( sFormatInfo->headSize > 0 )
            {
                sCompare = idlOS::memcmp( sString,
                                          sFormatInfo->head,
                                          sFormatInfo->headSize );
            }
            else
            {
                sCompare = 0;
            }

            if ( sCompare == 0 )
            {
                if ( sFormatInfo->tailSize > 0 )
                {
                    sCompare = idlOS::memcmp( sStringFence - sFormatInfo->tailSize,
                                              sFormatInfo->tail,
                                              sFormatInfo->tailSize );
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            sCompare = -1;
        }
        
        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

//fix For BUG-15930  
IDE_RC mtfLikeCalculateMB( mtcNode*     aNode,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           void*        aInfo,
                           mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *               ������ ���̿� ��� ��⿡ ���� �б⸦ �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    const mtdModule * sModule;
    const mtlModule * sLanguage;
    const mtcColumn * sColumn; 
    mtdCharType     * sVarchar;
    UChar           * sString;
    UChar           * sStringFence;
    UChar           * sFormat;
    UChar           * sEscape = NULL;
    UShort            sEscapeLen = 0;
    UInt              sFormatLen;
    mtdBinaryType    * sTempBinary;
    UInt               sBlockCnt;
    mtcLikeBlockInfo * sBlock;
    UChar            * sRefineString;
    mtdBooleanType     sResult; 

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule   = aStack[1].column->module;
    sLanguage = aStack[1].column->language;

    if( (sModule->isNull( aStack[1].column,
                          aStack[1].value ) == ID_TRUE) ||
        (sModule->isNull( aStack[2].column,
                          aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sVarchar     = (mtdCharType*)aStack[1].value;
        sString      = sVarchar->value;
        sStringFence = sString + sVarchar->length;
        sVarchar     = (mtdCharType*)aStack[2].value;
        sFormat      = sVarchar->value;
        sFormatLen   = sVarchar->length;

        sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);

        sBlock = (mtcLikeBlockInfo*)(sTempBinary->mValue);

        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[2].column.offset);
        
        sRefineString = (UChar*)(sTempBinary->mValue); 
            
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            if( sLanguage->id == MTL_UTF16_ID )
            {
                IDE_TEST_RAISE( sVarchar->length != 2, ERR_INVALID_ESCAPE );
            }
            else
            {
                IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            }
            
            sEscape = sVarchar->value;
            sEscapeLen = sVarchar->length;
        }
        else
        {
            //nothing to do 
        }

        IDE_TEST( getMoreInfoFromPatternMB( sFormat,
                                            sFormatLen,
                                            sEscape,
                                            sEscapeLen,
                                            sBlock,
                                            &sBlockCnt,
                                            sRefineString,
                                            sLanguage )
                  != IDE_SUCCESS);

        IDE_TEST( mtfLikeCalculateMBOnePass( sString,
                                             sStringFence,
                                             sBlock,
                                             sBlockCnt,
                                             &sResult,
                                             sLanguage )
                  != IDE_SUCCESS );

        *(mtdBooleanType*)aStack[0].value = sResult;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));
   
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateLengthMBFast( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtlModule   * sLanguage;
    const mtdModule   * sModule;
    mtdCharType       * sVarchar;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    mtdBigintType       sLength;
    SInt                sCompare;
    idBool              sEqual;
    UChar               sSize;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sLanguage   = aStack[1].column->language;
        sLength     = 0;
        
        sVarchar    = (mtdCharType*)aStack[1].value;
        sFormatInfo = (mtcLikeFormatInfo*) aInfo;

        sString      = sVarchar->value;
        sStringFence = sString + sVarchar->length;

        sVarchar    = (mtdCharType*)aStack[2].value;
        
        IDE_ASSERT( sFormatInfo != NULL );

        if ( sFormatInfo->type == MTC_FORMAT_UNDER )
        {
            // character_length( search_value ) = underCnt
            
            if( ( ( sModule->id == MTD_CHAR_ID ) ||
                  ( sModule->id == MTD_NCHAR_ID ) )
                &&
                ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE ) )
            {
                while ( sString < sStringFence )
                {
                    (void)sLanguage->nextCharPtr( & sString, sStringFence );
                    
                    sLength++;
                    
                    if ( sLength >= sFormatInfo->underCnt )
                    {
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                if ( sLength < sFormatInfo->underCnt )
                {
                    sCompare = -1;
                }
                else
                {
                    sCompare = 0;
                    
                    while ( sString < sStringFence )
                    {
                        sSize =  mtl::getOneCharSize( sString,
                                                      sStringFence,
                                                      sLanguage );
                        
                        sEqual = mtc::compareOneChar( sString,
                                                      sSize,
                                                      sLanguage->specialCharSet[MTL_SP_IDX],
                                                      sLanguage->specialCharSize );
                        
                        if ( sEqual != ID_TRUE )
                        {
                            sCompare = 1;
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                        
                        (void)sLanguage->nextCharPtr( & sString, sStringFence );
                    }
                }
            }
            else
            {
                while ( sString < sStringFence )
                {
                    (void)sLanguage->nextCharPtr( & sString, sStringFence );

                    sLength++;
                    
                    if ( sLength > sFormatInfo->underCnt )
                    {
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                if ( sLength == sFormatInfo->underCnt )
                {
                    sCompare = 0;
                }
                else
                {
                    sCompare = -1;
                }
            }
        }
        else
        {
            IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_UNDER_PERCENT,
                            "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                            sFormatInfo->type );

            // character_length( search_value ) >= underCnt
            
         
            
            while ( sString < sStringFence )
            {
                (void)sLanguage->nextCharPtr( & sString, sStringFence );
                
                sLength++;
                
                if ( sLength > sFormatInfo->underCnt )
                {
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
            
            if ( sLength >= sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }
        
        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4XlobLocator( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *               ������ ���̿� ��� ��⿡ ���� �б⸦ �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/

    const mtcColumn  * sColumn; 
    mtdCharType      * sVarchar;
    UChar            * sFormat;
    UChar              sEscape = 0;
    mtdClobLocatorType sLocator = MTD_LOCATOR_NULL;
    UInt               sLobLength;
    mtdBinaryType    * sTempBinary;
    UInt               sBlockCnt;
    mtcLikeBlockInfo * sBlock;
    UChar            * sRefineString;
    UShort             sFormatLen;
    mtdBooleanType     sResult;
    idBool             sIsNull;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    sVarchar     = (mtdCharType*)aStack[2].value;
    sFormat      = sVarchar->value;
    sFormatLen   = sVarchar->length;

    sLocator = *(mtdClobLocatorType*)aStack[1].value;

    // BUG-48005 Partition Table���� select /*+ use_one_pass_hash() */ ���� error�� ���ϴ�.
    if ( sLocator == MTD_LOCATOR_NULL )
    {
        sIsNull = ID_TRUE;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthLocator( sLocator,
                                            & sIsNull,
                                            & sLobLength,
                                            mtc::getStatistics(aTemplate) )
                  != IDE_SUCCESS );
    }

    if( (sIsNull == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sColumn     = aTemplate->rows[aNode->table].columns + aNode->column;
            
        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);

        sBlock = (mtcLikeBlockInfo*)(sTempBinary->mValue);

        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[2].column.offset);
        
        sRefineString = (UChar*)(sTempBinary->mValue);
        
        // escape ����
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            sEscape = sVarchar->value[0];
        }
        else
        {
            // nothing to do 
        }

        IDE_TEST( getMoreInfoFromPattern( sFormat,
                                          sFormatLen,
                                          &sEscape,
                                          1,
                                          sBlock,
                                          &sBlockCnt,
                                          sRefineString,
                                          NULL)
                  != IDE_SUCCESS);

        IDE_TEST( mtfLikeCalculate4XlobLocatorOnePass( sLocator,
                                                       sLobLength,
                                                       sBlock,
                                                       sBlockCnt,
                                                       &sResult )
                  != IDE_SUCCESS );

        *(mtdBooleanType*)aStack[0].value = sResult;
    }

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));

    IDE_EXCEPTION_END;

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateEqual4XlobLocatorFast( mtcNode*     aNode,
                                              mtcStack*    aStack,
                                              SInt         aRemain,
                                              void*        aInfo,
                                              mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    mtcLikeFormatInfo * sFormatInfo;
    UChar             * sPatternOffset;
    SInt                sCompare = 0;
    UChar               sBuffer[MTC_LOB_BUFFER_SIZE];
    UInt                sBufferOffset;
    UInt                sBufferMount;
    mtdClobLocatorType  sLocator = MTD_LOCATOR_NULL;
    UInt                sLobLength;
    idBool              sIsNull;
    UInt                sReadLength;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLocator = *(mtdClobLocatorType*)aStack[1].value;

    // BUG-48005 Partition Table���� select /*+ use_one_pass_hash() */ ���� error�� ���ϴ�.
    if ( sLocator == MTD_LOCATOR_NULL )
    {
        sIsNull = ID_TRUE;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthLocator( sLocator,
                                            & sIsNull,
                                            & sLobLength,
                                            mtc::getStatistics(aTemplate) )
                  != IDE_SUCCESS );
    }

    if( (sIsNull == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sFormatInfo = (mtcLikeFormatInfo*) aInfo;
        
        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_NORMAL,
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );

        if ( sLobLength == sFormatInfo->patternSize )
        {
            sBufferOffset = 0;
            sPatternOffset = sFormatInfo->pattern;
            
            while ( sBufferOffset < sLobLength )
            {
                // ���۸� �д´�.
                if ( sBufferOffset + MTC_LOB_BUFFER_SIZE > sLobLength )
                {
                    sBufferMount = sLobLength - sBufferOffset;
                }
                else
                {
                    sBufferMount = MTC_LOB_BUFFER_SIZE;
                }
                
                //ideLog::log( IDE_QP_0, "[like] offset=%d", sBufferOffset );
                
                IDE_TEST( mtc::readLob( mtc::getStatistics(aTemplate), /* idvSQL* */
                                        sLocator,
                                        sBufferOffset,
                                        sBufferMount,
                                        sBuffer,
                                        &sReadLength )
                          != IDE_SUCCESS );

                // ���Ѵ�.
                sCompare = idlOS::memcmp( sBuffer,
                                          sPatternOffset,
                                          sBufferMount );

                if ( sCompare != 0 )
                {
                    break;
                }
                else
                {
                    // Nothing to do.
                }

                sBufferOffset += sBufferMount;
                sPatternOffset += sBufferMount;
            }
        }
        else
        {
            sCompare = -1;
        }

        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateIsNotNull4XlobLocatorFast( mtcNode*     aNode,
                                                  mtcStack*    aStack,
                                                  SInt         aRemain,
                                                  void*        aInfo,
                                                  mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    mtcLikeFormatInfo * sFormatInfo;
    mtdClobLocatorType  sLocator = MTD_LOCATOR_NULL;
    UInt                sLobLength;
    idBool              sIsNull;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLocator = *(mtdClobLocatorType*)aStack[1].value;

    // BUG-48005 Partition Table���� select /*+ use_one_pass_hash() */ ���� error�� ���ϴ�.
    if ( sLocator == MTD_LOCATOR_NULL )
    {
        sIsNull = ID_TRUE;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthLocator( sLocator,
                                            & sIsNull,
                                            & sLobLength,
                                            mtc::getStatistics(aTemplate) )
                  != IDE_SUCCESS );
    }

    if( (sIsNull == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sFormatInfo = (mtcLikeFormatInfo*) aInfo;        

        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_PERCENT,
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );
        
        
        if ( sLobLength > 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateLength4XlobLocatorFast( mtcNode*     aNode,
                                               mtcStack*    aStack,
                                               SInt         aRemain,
                                               void*        aInfo,
                                               mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    mtcLikeFormatInfo * sFormatInfo;
    SInt                sCompare;
    mtdClobLocatorType  sLocator = MTD_LOCATOR_NULL;
    UInt                sLobLength;
    idBool              sIsNull;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLocator = *(mtdClobLocatorType*)aStack[1].value;

    // BUG-48005 Partition Table���� select /*+ use_one_pass_hash() */ ���� error�� ���ϴ�.
    if ( sLocator == MTD_LOCATOR_NULL )
    {
        sIsNull = ID_TRUE;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthLocator( sLocator,
                                            & sIsNull,
                                            & sLobLength,
                                            mtc::getStatistics(aTemplate) )
                  != IDE_SUCCESS );
    }

    if( (sIsNull == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sFormatInfo = (mtcLikeFormatInfo*) aInfo;
        
        IDE_ASSERT( sFormatInfo != NULL );
        
        if ( sFormatInfo->type == MTC_FORMAT_UNDER )
        {
            // character_length( search_value ) = underCnt
            
            if ( sLobLength == sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }
        else
        {
            IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_UNDER_PERCENT,
                            "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                            sFormatInfo->type );

            // character_length( search_value ) >= underCnt
            
            if ( sLobLength >= sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }
        
        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateOnePercent4XlobLocatorFast( mtcNode*     aNode,
                                                   mtcStack*    aStack,
                                                   SInt         aRemain,
                                                   void*        aInfo,
                                                   mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    mtcLikeFormatInfo * sFormatInfo;
    UChar             * sHeadOffset;
    UChar             * sTailOffset;
    SInt                sCompare = 0;
    UChar               sBuffer[MTC_LOB_BUFFER_SIZE];
    UInt                sBufferOffset;
    UInt                sBufferMount;
    mtdClobLocatorType  sLocator = MTD_LOCATOR_NULL;
    UInt                sLobLength;
    idBool              sIsNull;
    UInt                sReadLength;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLocator = *(mtdClobLocatorType*)aStack[1].value;

    // BUG-48005 Partition Table���� select /*+ use_one_pass_hash() */ ���� error�� ���ϴ�.
    if ( sLocator == MTD_LOCATOR_NULL )
    {
        sIsNull = ID_TRUE;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthLocator( sLocator,
                                            & sIsNull,
                                            & sLobLength,
                                            mtc::getStatistics(aTemplate) )
                  != IDE_SUCCESS );
    }

    if( (sIsNull == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sFormatInfo = (mtcLikeFormatInfo*) aInfo;
        
        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_NORMAL_ONE_PERCENT, 
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );
        IDE_ASSERT_MSG( sFormatInfo->percentCnt == 1,
                        "sFormatInfo->percentCnt : %"ID_UINT32_FMT"\n",
                        sFormatInfo->percentCnt );

        // search_value = [head]%[tail]
        
        if ( sLobLength >= (UInt)(sFormatInfo->headSize + sFormatInfo->tailSize) )
        {
            if ( sFormatInfo->headSize > 0 )
            {
                sBufferOffset = 0;
                sHeadOffset = sFormatInfo->head;

                while ( sBufferOffset < sFormatInfo->headSize )
                {
                    // ���۸� �д´�.
                    if ( sBufferOffset + MTC_LOB_BUFFER_SIZE > sFormatInfo->headSize )
                    {
                        sBufferMount = sFormatInfo->headSize - sBufferOffset;
                    }
                    else
                    {
                        sBufferMount = MTC_LOB_BUFFER_SIZE;
                    }
                    
                    //ideLog::log( IDE_QP_0, "[like] offset=%d", sBufferOffset );
                    
                    IDE_TEST( mtc::readLob( mtc::getStatistics(aTemplate), /* idvSQL* */
                                            sLocator,
                                            sBufferOffset,
                                            sBufferMount,
                                            sBuffer,
                                            &sReadLength )
                              != IDE_SUCCESS );
                    
                    // ���Ѵ�.
                    sCompare = idlOS::memcmp( sBuffer,
                                              sHeadOffset,
                                              sBufferMount );
                    
                    if ( sCompare != 0 )
                    {
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    sBufferOffset += sBufferMount;
                    sHeadOffset += sBufferMount;
                }
            }
            else
            {
                sCompare = 0;
            }

            if ( sCompare == 0 )
            {
                if ( sFormatInfo->tailSize > 0 )
                {
                    sBufferOffset = sLobLength - sFormatInfo->tailSize;
                    sTailOffset = sFormatInfo->tail;

                    while ( sBufferOffset < sLobLength )
                    {
                        // ���۸� �д´�.
                        if ( sBufferOffset + MTC_LOB_BUFFER_SIZE > sLobLength )
                        {
                            sBufferMount = sLobLength - sBufferOffset;
                        }
                        else
                        {
                            sBufferMount = MTC_LOB_BUFFER_SIZE;
                        }
                        
                        //ideLog::log( IDE_QP_0, "[like] offset=%d", sBufferOffset );
                        
                        IDE_TEST( mtc::readLob( mtc::getStatistics(aTemplate), /* idvSQL* */
                                                sLocator,
                                                sBufferOffset,
                                                sBufferMount,
                                                sBuffer,
                                                &sReadLength )
                                  != IDE_SUCCESS );
                        
                        // ���Ѵ�.
                        sCompare = idlOS::memcmp( sBuffer,
                                                  sTailOffset,
                                                  sBufferMount );
                        
                        if ( sCompare != 0 )
                        {
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }

                        sBufferOffset += sBufferMount;
                        sTailOffset += sBufferMount;
                    }
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            sCompare = -1;
        }
        
        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4XlobLocatorMB( mtcNode*     aNode,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       void*        aInfo,
                                       mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *               ������ ���̿� ��� ��⿡ ���� �б⸦ �����Ѵ�.
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/

    const mtcColumn   * sColumn;  
    const mtlModule   * sLanguage;
    const mtdModule   * sModule;
    mtdCharType       * sVarchar;
    UChar             * sFormat;
    UChar             * sEscape = NULL;
    mtdClobLocatorType  sLocator = MTD_LOCATOR_NULL;
    UInt                sLobLength;
    idBool              sIsNull;
    UShort              sEscapeLen = 0;
    mtdBinaryType     * sTempBinary;
    UInt                sBlockCnt;
    mtcLikeBlockInfo  * sBlock;
    UChar             * sRefineString;
    UShort              sFormatLen;
    mtdBooleanType      sResult;
    UInt                sMaxCharSize;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    sVarchar     = (mtdCharType*)aStack[2].value;
    sFormat      = sVarchar->value;
    sFormatLen   = sVarchar->length;

    sLocator = *(mtdClobLocatorType*)aStack[1].value;

    // BUG-48005 Partition Table���� select /*+ use_one_pass_hash() */ ���� error�� ���ϴ�.
    if ( sLocator == MTD_LOCATOR_NULL )
    {
        sIsNull = ID_TRUE;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthLocator( sLocator,
                                            & sIsNull,
                                            & sLobLength,
                                            mtc::getStatistics(aTemplate) )
                  != IDE_SUCCESS );
    }

    if( (sIsNull == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sModule      = aStack[1].column->module;
        sLanguage    = aStack[0].column->language;               

        sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);

        sBlock = (mtcLikeBlockInfo*)(sTempBinary->mValue);

        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[2].column.offset);
        
        sRefineString = (UChar*)(sTempBinary->mValue);

        // escape ����
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            sEscape = sVarchar->value;
            sEscapeLen = sVarchar->length;
        }
        else
        {
            // nothing to do
        }

        sMaxCharSize =  MTL_NCHAR_PRECISION(sModule);

        IDE_TEST( getMoreInfoFromPatternMB( sFormat,
                                            sFormatLen,
                                            sEscape,
                                            sEscapeLen,
                                            sBlock,
                                            &sBlockCnt,
                                            sRefineString,
                                            sLanguage )
                  != IDE_SUCCESS);

        IDE_TEST( mtfLikeCalculate4XlobLocatorMBOnePass( sLocator,
                                                         sLobLength,
                                                         sBlock,
                                                         sBlockCnt,
                                                         &sResult,
                                                         sMaxCharSize,
                                                         sLanguage,
                                                         aTemplate )
                  != IDE_SUCCESS );

        *(mtdBooleanType*)aStack[0].value = sResult;
    }

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));

    IDE_EXCEPTION_END;

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateLength4XlobLocatorMBFast( mtcNode*     aNode,
                                                 mtcStack*    aStack,
                                                 SInt         aRemain,
                                                 void*        aInfo,
                                                 mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtlModule   * sLanguage;
    mtdBigintType       sLength;
    UChar               sBuffer[MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION];
    UInt                sBufferOffset;
    UInt                sBufferMount;
    UInt                sBufferSize;
    UChar             * sIndex;
    UChar             * sFence;
    UChar             * sBufferFence;
    mtcLikeFormatInfo * sFormatInfo;
    SInt                sCompare;
    
    mtdClobLocatorType  sLocator = MTD_LOCATOR_NULL;
    UInt                sLobLength;
    idBool              sIsNull;
    UInt                sReadLength;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLocator = *(mtdClobLocatorType*)aStack[1].value;

    // BUG-48005 Partition Table���� select /*+ use_one_pass_hash() */ ���� error�� ���ϴ�.
    if ( sLocator == MTD_LOCATOR_NULL )
    {
        sIsNull = ID_TRUE;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthLocator( sLocator,
                                            & sIsNull,
                                            & sLobLength,
                                            mtc::getStatistics(aTemplate) )
                  != IDE_SUCCESS );
    }

    if ( sIsNull == ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        sLanguage = aStack[1].column->language;
        sLength = 0;
        
        sFormatInfo = (mtcLikeFormatInfo*) aInfo;
        
        IDE_ASSERT( sFormatInfo != NULL );

        sBufferOffset = 0;
        
        while ( sBufferOffset < sLobLength )
        {
            // ���۸� �д´�.
            if ( sBufferOffset + MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION > sLobLength )
            {
                sBufferMount = sLobLength - sBufferOffset;
                sBufferSize = sBufferMount;
            }
            else
            {
                sBufferMount = MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION;
                sBufferSize = MTC_LOB_BUFFER_SIZE;
            }
            
            //ideLog::log( IDE_QP_0, "[like] offset=%d", sBufferOffset );
            
            IDE_TEST( mtc::readLob( mtc::getStatistics(aTemplate), /* idvSQL* */
                                    sLocator,
                                    sBufferOffset,
                                    sBufferMount,
                                    sBuffer,
                                    &sReadLength )
                      != IDE_SUCCESS );
            
            // ���ۿ��� ���ڿ� ���̸� ���Ѵ�.
            sIndex = sBuffer;
            sFence = sIndex + sBufferSize;
            sBufferFence = sIndex + sBufferMount;
            
            while ( sIndex < sFence ) 
            {
                (void)sLanguage->nextCharPtr( & sIndex, sBufferFence );
                
                sLength++;
                
                if ( sLength > sFormatInfo->underCnt )
                {
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
            
            sBufferOffset += ( sIndex - sBuffer );
        }
        
        if ( sFormatInfo->type == MTC_FORMAT_UNDER )
        {
            // character_length( search_value ) = underCnt
            
            if ( sLength == sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }
        else
        {
            IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_UNDER_PERCENT,
                            "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                            sFormatInfo->type );

            // character_length( search_value ) >= underCnt
            
            if ( sLength >= sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }
        
        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }
    return IDE_FAILURE;
}

IDE_RC mtfExtractRange4Bit( mtcNode*      aNode,
                            mtcTemplate*  aTemplate,
                            mtkRangeInfo* aInfo,
                            smiRange*     aRange )
{
/***********************************************************************
 *
 * Description : Extract Range
 *
 * Implementation :
 *
 ***********************************************************************/
    
    static const mtdCharType sEscapeEmpty = { 0, { 0 } };
    
    mtcNode          * sIndexNode;
    mtcNode          * sValueNode1;
    mtcNode          * sValueNode2;
    mtdCharType      * sValue;
    const mtdCharType* sEscape;
    mtdCharType      * sValue1;
    mtdCharType      * sValue2;
    mtdBitType       * sValue3;
    mtdBitType       * sValue4;
    mtkRangeCallBack * sMinimumCallBack;
    mtkRangeCallBack * sMaximumCallBack;
    mtcColumn        * sValueColumn;
    mtcColumn        * sEscapeColumn;
    mtcColumn        * sValueColumn1;
    mtcColumn        * sValueColumn2;
    idBool             sIsEqual;
    SInt               sValueSize;

    // fix for PROJ-1571
    mtcColumn        * sIndexColumn;

    sIndexNode  = aNode->arguments;
    sValueNode1 = sIndexNode->next;
    sValueNode2 = sValueNode1->next;

    // fix for PROJ-1571
    sIndexColumn = aTemplate->rows[sIndexNode->table].columns
        + sIndexNode->column;

    sValueNode1 = mtf::convertedNode( sValueNode1, aTemplate );
    
    sValueColumn = aTemplate->rows[sValueNode1->table].columns
        + sValueNode1->column;
    sValue       = (mtdCharType*)
        ( (UChar*) aTemplate->rows[sValueNode1->table].row
          + sValueColumn->column.offset);
    if( sValueNode2 != NULL )
    {
        sValueNode2   = mtf::convertedNode( sValueNode2, aTemplate );
        sEscapeColumn = aTemplate->rows[sValueNode2->table].columns
            + sValueNode2->column;
        sEscape       = (mtdCharType*)
            ( (UChar*) aTemplate->rows[sValueNode2->table].row
              +sEscapeColumn->column.offset );
    }
    else
    {
        sEscape = &sEscapeEmpty;
    }
    
    sMinimumCallBack       = (mtkRangeCallBack*)( aRange + 1 );
    sMaximumCallBack       = sMinimumCallBack + 1;
    aRange->minimum.data   = sMinimumCallBack;
    aRange->maximum.data   = sMaximumCallBack;
    sMinimumCallBack->next = NULL;
    sMaximumCallBack->next = NULL;
    
    if( sValueColumn->module->isNull( sValueColumn,
                                      sValue ) == ID_TRUE )
    {
        aRange->minimum.callback     = mtk::rangeCallBackGT4Mtd;
        aRange->maximum.callback     = mtk::rangeCallBackLT4Mtd;       
        sMinimumCallBack->compare = mtk::compareMinimumLimit;
        sMaximumCallBack->compare = mtk::compareMinimumLimit;

        aRange->prev              = NULL;
        aRange->next              = NULL;

        /* BUG-9889 fix by kumdory. 2005-01-24 */
        sMinimumCallBack->columnIdx  =  aInfo->columnIdx;
        //sMinimumCallBack->columnDesc = NULL;
        //sMinimumCallBack->valueDesc  = NULL;
        sMinimumCallBack->value      = NULL;

        sMaximumCallBack->columnIdx  =  aInfo->columnIdx;
        //sMaximumCallBack->columnDesc = NULL;
        //sMaximumCallBack->valueDesc  = NULL;
        sMaximumCallBack->value      = NULL;
    }
    else
    {
        sValueColumn1 = (mtcColumn*)( sMaximumCallBack + 1 );
        sValueColumn2 = sValueColumn1 + 1;
        
        sValue1 = (mtdCharType*)( sValueColumn2 + 1 );
        sValue2 = (mtdCharType*)((UChar*)sValue1 + MTC_LIKE_KEY_SIZE);

// fix for PROJ-1571
        sValue3 = (mtdBitType*)((UChar*)sValue2);
        sValue4 = (mtdBitType*)((UChar*)sValue3 + MTC_LIKE_KEY_SIZE);

        IDE_TEST( mtc::initializeColumn( sValueColumn1,
                                         & mtdBit,
                                         1,
                                         MTC_LIKE_KEY_PRECISION,
                                         0 )
                  != IDE_SUCCESS );
        
        sValueColumn1->column.offset = 0;  

        IDE_TEST( mtc::initializeColumn( sValueColumn2,
                                         & mtdBit,
                                         1,
                                         MTC_LIKE_KEY_PRECISION,
                                         0 )
                  != IDE_SUCCESS );
        
        sValueColumn2->column.offset = 0;                     

        // like key ���� 
        IDE_TEST( mtfLikeKey( sValue1->value,
                              &sValue1->length,
                              MTC_LIKE_KEY_PRECISION,
                              sValue->value,
                              sValue->length,
                              sEscape->value,
                              sEscape->length,
                              & sIsEqual,
                              (const mtlModule *) (sValueColumn->language) )
                  != IDE_SUCCESS );

        IDE_TEST( convertChar2Bit( (void *)sValue3,
                                   (void *)sValue1->value,
                                   sValue1->length,
                                   ID_TRUE ) != IDE_SUCCESS );

        if( BIT_TO_BYTE( sIndexColumn->precision ) < ( SInt )MTC_LIKE_KEY_SIZE )
        {
            sValueSize = BIT_TO_BYTE( sIndexColumn->precision );
        }
        else
        {
            sValueSize = MTC_LIKE_KEY_SIZE;
        }

        idlOS::memset( sValue4->value,
                       0xFF,
                       sValueSize );

        IDE_TEST( convertChar2Bit( (void *)sValue4,
                                   (void *)sValue1->value,
                                   sValue1->length,
                                   ID_FALSE ) != IDE_SUCCESS );
        sValue4->length = sValueSize;

        aRange->prev                 = NULL;
        aRange->next                 = NULL;
        
        if( aInfo->direction == MTD_COMPARE_ASCENDING )
        {
            if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
                 aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
            {
                // mtd type�� column value�� ���� range callback 
                aRange->minimum.callback     = mtk::rangeCallBackGE4Mtd;
                aRange->maximum.callback     = mtk::rangeCallBackLE4Mtd;
            }
            else
            {
                if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
                {
                    /* MTD_COMPARE_STOREDVAL_MTDVAL
                       stored type�� column value�� ���� range callback */
                    aRange->minimum.callback     = mtk::rangeCallBackGE4Stored;
                    aRange->maximum.callback     = mtk::rangeCallBackLE4Stored;
                }
                else
                {
                    /* PROJ-2433 */
                    aRange->minimum.callback     = mtk::rangeCallBackGE4IndexKey;
                    aRange->maximum.callback     = mtk::rangeCallBackLE4IndexKey;
                }
            }
            
            sMinimumCallBack->columnIdx  =  aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->columnDesc, aInfo->column ) )
            {
                sMinimumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->valueDesc, sValueColumn1 ) )
            {
                sMinimumCallBack->valueDesc  = *sValueColumn1;
            }
            if( sValue3->length > 0 )
            {
                sMinimumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                                                         [aInfo->direction];
            }
            else
            {
                sMinimumCallBack->compare = mtk::compareMinimumLimit;
            }
            sMinimumCallBack->value      = sValue3;
        
            sMaximumCallBack->columnIdx  =  aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->columnDesc, aInfo->column ) )
            {
                sMaximumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->valueDesc, sValueColumn2 ) )
            {
                sMaximumCallBack->valueDesc  = *sValueColumn2;
            }
            if( sValue4->length > 0 )
            {
                sMaximumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                                                         [aInfo->direction];
            }
            else
            {
                if ( ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_MTDVAL ) )
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Mtd;
                }
                else
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Stored;
                }
            }

            sMaximumCallBack->value      = sValue4;
        }
        else
        {
            if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
                 aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
            {
                // mtd type�� column value�� ���� range callback 
                aRange->minimum.callback     = mtk::rangeCallBackGE4Mtd;
                aRange->maximum.callback     = mtk::rangeCallBackLE4Mtd;
            }
            else
            {
                if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
                {
                    /* MTD_COMPARE_STOREDVAL_MTDVAL
                       stored type�� column value�� ���� range callback */
                    aRange->minimum.callback     = mtk::rangeCallBackGE4Stored;
                    aRange->maximum.callback     = mtk::rangeCallBackLE4Stored;
                }
                else
                {
                    /* PROJ-2433 */
                    aRange->minimum.callback     = mtk::rangeCallBackGE4IndexKey;
                    aRange->maximum.callback     = mtk::rangeCallBackLE4IndexKey;
                }
            }
            
            sMinimumCallBack->columnIdx  =  aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->columnDesc, aInfo->column ) )
            {
                sMinimumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->valueDesc, sValueColumn2 ) )
            {
                sMinimumCallBack->valueDesc  = *sValueColumn2;
            }
            if( sValue4->length > 0 )
            {
                sMinimumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                                                         [aInfo->direction];
            }
            else
            {
                sMinimumCallBack->compare = mtk::compareMinimumLimit;
            }
            sMinimumCallBack->value      = sValue4;
            
            sMaximumCallBack->columnIdx  =  aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->columnDesc, aInfo->column ) )
            {
                sMaximumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->valueDesc, sValueColumn1 ) )
            {
                sMaximumCallBack->valueDesc  = *sValueColumn1;
            }
            if( sValue3->length > 0 )
            {
                sMaximumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                                                         [aInfo->direction];
            }
            else
            {
                if ( ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_MTDVAL ) )
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Mtd;
                }
                else
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Stored;
                }
            }
            
            sMaximumCallBack->value      = sValue3;
        }
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    if( ideGetErrorCode() == idERR_ABORT_idnReachEnd )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE));
    }
    else if( ideGetErrorCode() == idERR_ABORT_idnLikeEscape )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));
    }
    
    return IDE_FAILURE;
}

IDE_RC convertChar2Bit( void * aValue,
                        void * aToken,
                        UInt   aTokenLength,
                        idBool aFiller )
{
    mtdBitType   * sValue;
    const UChar  * sToken;
    const UChar  * sTokenFence;
    UChar*         sIterator;
    UInt           sSubIndex;

    static const UChar sBinary[256] = {
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
         0,  1, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
    };

    sValue = (mtdBitType*)aValue;

    sIterator = sValue->value;

    if( aTokenLength == 0 )
    {
        sValue->length = 0;
    }
    else
    {
        sValue->length = 0;

        for( sToken      = (const UChar*)aToken,
                 sTokenFence = sToken + aTokenLength;
             sToken      < sTokenFence;
             sIterator++, sToken += 8 )
        {
            IDE_TEST_RAISE( sBinary[sToken[0]] > 1, ERR_INVALID_LITERAL );

            if( aFiller == ID_TRUE )
            {
                idlOS::memset( sIterator,
                               0x00,
                               1 );
                *sIterator = sBinary[sToken[0]] << 7;
            }
            else
            {
                //*sIterator &= sBinary[sToken[0]] << 7;
                if( sBinary[sToken[0]] == 0 )
                {
                    *sIterator -= (sBinary[sToken[0]] + 1) << 7;
                }
            }

            sValue->length++;
            IDE_TEST_RAISE( sValue->length == 0,
                            ERR_INVALID_LITERAL );

            sSubIndex = 1;
            while( (sToken + sSubIndex < sTokenFence) && (sSubIndex < 8) )
            {
                IDE_TEST_RAISE( sBinary[sToken[sSubIndex]] > 1, ERR_INVALID_LITERAL );
                if( aFiller == ID_TRUE )
                {
                    *sIterator |= ( sBinary[sToken[sSubIndex]] << ( 7 - sSubIndex ) );
                }
                else
                {
                    //*sIterator &= ( sBinary[sToken[sSubIndex]] << ( 7 - sSubIndex ) );
                    if( sBinary[sToken[sSubIndex]] == 0 )
                    {
                        *sIterator -= ( (sBinary[sToken[sSubIndex]] + 1) << ( 7 - sSubIndex ) );
                    }
                }
                sValue->length++;
                sSubIndex++;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC nextCharWithBuffer( mtcLobBuffer    * aBuffer,
                           mtcLobCursor    * aCursor,
                           mtcLobCursor    * aCursorPrev,
                           mtcTemplate     * aTemplate )
{
    if( aCursorPrev != NULL )
    {
        aCursorPrev->offset = aCursor->offset;
        aCursorPrev->index  = aCursor->index;
    }
    else
    {
        // Nothing to do.
    }

    if ( aBuffer->offset != aCursor->offset )
    {
        //if ( aCursor->offset < aBuffer->offset )
        //{
        //    ideLog::log( IDE_QP_0, "[like] backward" );
        //}
        
        aBuffer->offset = aCursor->offset;
        
        // ���۸� �д´�.
        IDE_TEST( mtfLikeReadLob( aBuffer, aTemplate )
                      != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    aCursor->index++;
    
    if ( aCursor->index >= aBuffer->buf + aBuffer->size )
    {
        aBuffer->offset += ( aCursor->index - aBuffer->buf );
        
        // ���۸� �д´�.
        IDE_TEST( mtfLikeReadLob( aBuffer, aTemplate )
                  != IDE_SUCCESS );
        
        aCursor->offset = aBuffer->offset;
        aCursor->index = aBuffer->buf;
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC compareCharWithBuffer( mtcLobBuffer       * aBuffer,
                              const mtcLobCursor * aCursor,
                              const UChar        * aFormat,
                              idBool             * aIsSame,
                              mtcTemplate        * aTemplate )
{
    if ( aBuffer->offset != aCursor->offset )
    {
        //if ( aCursor->offset < aBuffer->offset )
        //{
        //    ideLog::log( IDE_QP_0, "[like] backward" );
        //}
        
        aBuffer->offset = aCursor->offset;
        
        // ���۸� �д´�.
        IDE_TEST( mtfLikeReadLob( aBuffer, aTemplate )
                      != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if ( *aCursor->index == *aFormat )
    {
        *aIsSame = ID_TRUE;
    }
    else
    {
        *aIsSame = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
    
IDE_RC nextCharWithBufferMB( mtcLobBuffer    * aBuffer,
                             mtcLobCursor    * aCursor,
                             mtcLobCursor    * aCursorPrev,
                             const mtlModule * aLanguage,
                             mtcTemplate     * aTemplate )
{
    if( aCursorPrev != NULL )
    {
        aCursorPrev->offset = aCursor->offset;
        aCursorPrev->index  = aCursor->index;
    }
    else
    {
        // Nothing to do.
    }

    if ( aBuffer->offset != aCursor->offset )
    {
        //if ( aCursor->offset < aBuffer->offset )
        //{
        //    ideLog::log( IDE_QP_0, "[like] backward" );
        //}
        
        aBuffer->offset = aCursor->offset;
        
        // ���۸� �д´�.
        IDE_TEST( mtfLikeReadLob( aBuffer, aTemplate )
                      != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    (void)aLanguage->nextCharPtr( & aCursor->index, aBuffer->fence );
    
    if ( aCursor->index >= aBuffer->buf + aBuffer->size )
    {
        aBuffer->offset += ( aCursor->index - aBuffer->buf );
        
        // ���۸� �д´�.
        IDE_TEST( mtfLikeReadLob( aBuffer, aTemplate )
                  != IDE_SUCCESS );
        
        aCursor->offset = aBuffer->offset;
        aCursor->index = aBuffer->buf;
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC compareCharWithBufferMB( mtcLobBuffer       * aBuffer,
                                const mtcLobCursor * aCursor,
                                const UChar        * aFormat,
                                UChar                aFormatCharSize,
                                idBool             * aIsSame,
                                const mtlModule    * aLanguage,
                                mtcTemplate        * aTemplate )
{
    UChar sSize;
    
    if ( aBuffer->offset != aCursor->offset )
    {
        //if ( aCursor->offset < aBuffer->offset )
        //{
        //    ideLog::log( IDE_QP_0, "[like] backward" );
        //}
        
        aBuffer->offset = aCursor->offset;
        
        // ���۸� �д´�.
        IDE_TEST( mtfLikeReadLob( aBuffer, aTemplate )
                      != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    sSize =  mtl::getOneCharSize( aCursor->index,
                                  aBuffer->fence,
                                  aLanguage ); 

    *aIsSame = mtc::compareOneChar( aCursor->index,
                                    sSize,
                                    (UChar*)aFormat,
                                    aFormatCharSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
    
IDE_RC mtfLikeReadLob( mtcLobBuffer * aBuffer,
                       mtcTemplate  * aTemplate )
{
    UInt   sLength;
    UInt   sOffset;
    UInt   sMount = 0;
    UInt   sSize = 0;
    UInt   sReadLength;

    sLength = aBuffer->length;
    sOffset = aBuffer->offset;

    if ( sOffset < sLength )
    {
        if ( sOffset + MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION > sLength )
        {
            sMount = sLength - sOffset;
            sSize = sMount;
        }
        else
        {
            sMount = MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION;
            sSize = MTC_LOB_BUFFER_SIZE;
        }

        //ideLog::log( IDE_QP_0, "[like] offset=%d", sOffset );

        IDE_TEST( mtc::readLob( mtc::getStatistics(aTemplate), /* idvSQL* */
                                aBuffer->locator,
                                sOffset,
                                sMount,
                                aBuffer->buf,
                                &sReadLength )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    aBuffer->fence = aBuffer->buf + sMount;
    aBuffer->size  = sSize;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfExtractRange4Echar( mtcNode*      aNode,
                              mtcTemplate*  aTemplate,
                              mtkRangeInfo* aInfo,
                              smiRange*     aRange )
{
/***********************************************************************
 *
 * Description : Extract Range
 *
 * Implementation :
 *
 ***********************************************************************/
    
    static const mtdCharType sEscapeEmpty = { 0, { 0 } };
    
    mtcNode          * sIndexNode;
    mtcNode          * sValueNode1;
    mtcNode          * sValueNode2;
    mtdEcharType     * sValue;
    const mtdCharType* sEscape;
    mtdEcharType     * sValue1;
    mtdEcharType     * sValue2;
    mtkRangeCallBack * sMinimumCallBack;
    mtkRangeCallBack * sMaximumCallBack;
    mtcColumn        * sValueColumn;
    mtcColumn        * sEscapeColumn;
    mtcColumn        * sValueColumn1;
    mtcColumn        * sValueColumn2;
    idBool             sIsEqual;
    
    mtcECCInfo         sInfo;
    mtcEncryptInfo     sDecryptInfo;
    UShort             sPlainLength;
    UChar            * sPlain;
    UChar              sDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    UInt               sECCSize;
    UInt               sEcharKeySize;

    sIndexNode  = aNode->arguments;
    sValueNode1 = sIndexNode->next;
    sValueNode2 = sValueNode1->next;
    
    sValueNode1 = mtf::convertedNode( sValueNode1, aTemplate );
    
    sValueColumn = aTemplate->rows[sValueNode1->table].columns
        + sValueNode1->column;
    sValue       = (mtdEcharType*)
        ( (UChar*) aTemplate->rows[sValueNode1->table].row
          + sValueColumn->column.offset);
    if( sValueNode2 != NULL )
    {
        sValueNode2   = mtf::convertedNode( sValueNode2, aTemplate );
        sEscapeColumn = aTemplate->rows[sValueNode2->table].columns
            + sValueNode2->column;
        sEscape       = (mtdCharType*)
            ( (UChar*) aTemplate->rows[sValueNode2->table].row
              +sEscapeColumn->column.offset );
    }
    else
    {
        sEscape = &sEscapeEmpty;
    }
    
    sMinimumCallBack       = (mtkRangeCallBack*)( aRange + 1 );
    sMaximumCallBack       = sMinimumCallBack + 1;
    aRange->minimum.data   = sMinimumCallBack;
    aRange->maximum.data   = sMaximumCallBack;
    sMinimumCallBack->next = NULL;
    sMaximumCallBack->next = NULL;
    sMinimumCallBack->flag = 0;
    sMaximumCallBack->flag = 0;
    
    if( sValueColumn->module->isNull( sValueColumn,
                                      sValue ) == ID_TRUE )
    {
        if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
             aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
        {
            aRange->minimum.callback  = mtk::rangeCallBackGT4Mtd;
            aRange->maximum.callback  = mtk::rangeCallBackLT4Mtd;
        }
        else
        {
            if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                 ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
            {
                aRange->minimum.callback  = mtk::rangeCallBackGT4Stored;
                aRange->maximum.callback  = mtk::rangeCallBackLT4Stored;
            }
            else
            {
                /* PROJ-2433 */
                aRange->minimum.callback  = mtk::rangeCallBackGT4IndexKey;
                aRange->maximum.callback  = mtk::rangeCallBackLT4IndexKey;
            }
        }
                    
        sMinimumCallBack->compare = mtk::compareMinimumLimit;
        sMaximumCallBack->compare = mtk::compareMinimumLimit;

        aRange->prev              = NULL;
        aRange->next              = NULL;

        /* BUG-9889 fix by kumdory. 2005-01-24 */
        sMinimumCallBack->columnIdx  =  aInfo->columnIdx;
        //sMinimumCallBack->columnDesc = NULL;
        //sMinimumCallBack->valueDesc  = NULL;
        sMinimumCallBack->value      = NULL;

        sMaximumCallBack->columnIdx  =  aInfo->columnIdx;
        //sMaximumCallBack->columnDesc = NULL;
        //sMaximumCallBack->valueDesc  = NULL;
        sMaximumCallBack->value      = NULL;
    }
    else
    {
        IDE_TEST( mtc::getLikeEcharKeySize( aTemplate,
                                            & sECCSize,
                                            & sEcharKeySize )
                  != IDE_SUCCESS );
        
        sValueColumn1 = (mtcColumn*)( sMaximumCallBack + 1 );
        sValueColumn2 = sValueColumn1 + 1;
        
        sValue1 = (mtdEcharType*)( sValueColumn2 + 1 );
        sValue2 = (mtdEcharType*)((UChar*)sValue1 + sEcharKeySize);
       
        IDE_TEST( mtc::initializeColumn( sValueColumn1,
                                         & mtdEvarchar,
                                         1,
                                         MTC_LIKE_KEY_PRECISION,
                                         0 )
                  != IDE_SUCCESS );
        
        IDE_TEST( mtc::initializeEncryptColumn( sValueColumn1,
                                                (const SChar*) "",
                                                MTC_LIKE_KEY_PRECISION,
                                                sECCSize )
                  != IDE_SUCCESS );
        
        sValueColumn1->column.offset = 0;  

        IDE_TEST( mtc::initializeColumn( sValueColumn2,
                                         & mtdEvarchar,
                                         1,
                                         MTC_LIKE_KEY_PRECISION,
                                         0 )
                  != IDE_SUCCESS );
        
        IDE_TEST( mtc::initializeEncryptColumn( sValueColumn2,
                                                (const SChar*) "",
                                                MTC_LIKE_KEY_PRECISION,
                                                sECCSize )
                  != IDE_SUCCESS );
        
        sValueColumn2->column.offset = 0;                     

        //--------------------------------------------------
        // pattern string���� �÷��� ���� ���
        // default policy�� �ƴϹǷ� decrypt�� �����Ͽ�
        // plain text�� ��´�.
        //--------------------------------------------------
        
        if ( sValueColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
        {
            sPlain = sDecryptedBuf;
            
            if( sValue->mCipherLength > 0 )
            {
                IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                     sValueNode1->baseTable,
                                                     sValueNode1->baseColumn,
                                                     & sDecryptInfo )
                          != IDE_SUCCESS );
                
                IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                              sValueColumn->mColumnAttr.mEncAttr.mPolicy,
                                              sValue->mValue,
                                              sValue->mCipherLength,
                                              sPlain,
                                              & sPlainLength )
                          != IDE_SUCCESS );
                
                IDE_ASSERT_MSG( sPlainLength <= sValueColumn->precision,
                                "sPlainLength : %"ID_UINT32_FMT"\n"
                                "sValueColumn->precision : %"ID_UINT32_FMT"\n",
                                sPlainLength, sValueColumn->precision );
            }
            else
            {
                sPlainLength = 0;
            }
        }
        else
        {
            sPlain = sValue->mValue;
            sPlainLength = sValue->mCipherLength;
        }

        // BUG-43810 echar type�� key range�� �����Ҷ���
        // plain text���� space pading�� ������ ���̸� ã�´�.
        if ( sValueColumn->module->id == MTD_ECHAR_ID )
        {
            for ( ; sPlainLength > 1; sPlainLength-- )
            {
                if ( sPlain[sPlainLength - 1] != ' ' )
                {
                    break;
                }
                else
                {
                    // Nothingo to do.
                }
            }
        }
        else
        {
            // Nothing to do.
        }
        
        //fix For BUG-15930  
        // like key ���� 
        if( (const mtlModule *) (sValueColumn->language) == &mtlAscii )
        {
            IDE_TEST( mtfLikeKey( sValue1->mValue,
                                  &sValue1->mCipherLength,
                                  MTC_LIKE_KEY_PRECISION,
                                  sPlain,
                                  sPlainLength,
                                  sEscape->value,
                                  sEscape->length,
                                  & sIsEqual,
                                  (const mtlModule *) (sValueColumn->language) )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( mtfLikeKeyMB( sValue1->mValue,
                                    &sValue1->mCipherLength,
                                    MTC_LIKE_KEY_PRECISION,
                                    sPlain,
                                    sPlainLength,
                                    sEscape->value,
                                    sEscape->length,
                                    & sIsEqual,
                                    (const mtlModule *) (sValueColumn->language) )
                      != IDE_SUCCESS );
        }

        //--------------------------------------------------
        // To Fix BUG-12306
        // - like key�� 'F%', 'F_' Ÿ���� ���,
        //   like key ���� ������ ���ڸ� �ϳ� ����
        //   ex) LIKE 'aa%'�� ��� : sValue1->value 'aa'
        //                           sValue2->value 'ab'
        // - like key�� 'F' Ÿ���� ���,
        //   ex) LIKE 'aa'�� ���  : sValue1->value 'aa'
        //                           sValue2->value 'aa'
        //--------------------------------------------------

        idlOS::memcpy( sValue2->mValue, sValue1->mValue,
                       sValue1->mCipherLength );
        
        sValue2->mCipherLength = sValue1->mCipherLength;
        
        if ( sIsEqual == ID_FALSE )
        {
            // like key�� 'F%', 'F_' Ÿ���� ���,
            for( sValue2->mCipherLength = sValue1->mCipherLength;
                 sValue2->mCipherLength > 0;
                 sValue2->mCipherLength-- )
            {
                sValue2->mValue[sValue2->mCipherLength-1]++;
                if( sValue2->mValue[sValue2->mCipherLength-1] != 0 )
                {
                    break;
                }
            }
        }
        else
        {
            // like key�� 'F' Ÿ���� ���
        }

        //--------------------------------------------------
        // ECC ����
        //--------------------------------------------------

        if ( sValue1->mCipherLength > 0 )
        {
            IDE_TEST( aTemplate->getECCInfo( aTemplate,
                                             & sInfo )
                      != IDE_SUCCESS );
            
            IDE_TEST( aTemplate->encodeECC(
                          & sInfo,
                          sValue1->mValue,
                          sValue1->mCipherLength,
                          sValue1->mValue + sValue1->mCipherLength,
                          & sValue1->mEccLength )
                      != IDE_SUCCESS );

            IDE_ASSERT_MSG( sValue1->mEccLength <= sECCSize,
                            "sValue1->mEccLength : %"ID_UINT32_FMT"\n"
                            "sECCSize : %"ID_UINT32_FMT"\n",
                            sValue1->mEccLength, sECCSize );
        }
        else
        {
            sValue1->mEccLength = 0;
        }
        
        if ( sValue2->mCipherLength > 0 )
        {
            IDE_TEST( aTemplate->getECCInfo( aTemplate,
                                             & sInfo )
                      != IDE_SUCCESS );
            
            IDE_TEST( aTemplate->encodeECC(
                          & sInfo,
                          sValue2->mValue,
                          sValue2->mCipherLength,
                          sValue2->mValue + sValue2->mCipherLength,
                          & sValue2->mEccLength )
                      != IDE_SUCCESS );
            
            IDE_ASSERT_MSG( sValue2->mEccLength <= sECCSize,
                            "sValue2->mEccLength : %"ID_UINT32_FMT"\n"
                            "sECCSize : %"ID_UINT32_FMT"\n",
                            sValue2->mEccLength, sECCSize );
        }
        else
        {
            sValue2->mEccLength = 0;
        }
        
        aRange->prev = NULL;
        aRange->next = NULL;
        
        if( aInfo->direction == MTD_COMPARE_ASCENDING )
        {
            // To Fix BUG-12306
            if ( sIsEqual == ID_TRUE )
            {
                //---------------------------
                // RangeCallBack ����
                //---------------------------

                if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
                     aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
                {
                    // mtd type�� column value�� ���� range callback 
                    aRange->minimum.callback     = mtk::rangeCallBackGE4Mtd;
                    aRange->maximum.callback     = mtk::rangeCallBackLE4Mtd;
                }
                else
                {
                    if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                         ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
                    {
                        /* MTD_COMPARE_STOREDVAL_MTDVAL
                           stored type�� column value�� ���� range callback */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4Stored;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4Stored;
                    }
                    else
                    {
                        /* PROJ-2433 */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4IndexKey;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4IndexKey;
                    }
                }
            }
            else
            {
                if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
                     aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
                {
                    // mtd type�� column value�� ���� range callback 
                    aRange->minimum.callback     = mtk::rangeCallBackGE4Mtd;
                    aRange->maximum.callback     = mtk::rangeCallBackLT4Mtd;
                }
                else
                {
                    if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                         ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
                    {
                        /* MTD_COMPARE_STOREDVAL_MTDVAL
                           stored type�� column value�� ���� range callback */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4Stored;
                        aRange->maximum.callback     = mtk::rangeCallBackLT4Stored;
                    }
                    else
                    {
                        /* PROJ-2433 */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4IndexKey;
                        aRange->maximum.callback     = mtk::rangeCallBackLT4IndexKey;
                    }
                }
            }

            //---------------------------
            // MinimumCallBack ����
            //---------------------------
            
            sMinimumCallBack->columnIdx  = aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->columnDesc, aInfo->column ) )
            {
                sMinimumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->valueDesc, sValueColumn1 ) )
            {
                sMinimumCallBack->valueDesc = *sValueColumn1;
            }
            sMinimumCallBack->value      = sValue1;
            
            if( sValue1->mEccLength > 0 )
            {
                // PROJ-1364
                if( aInfo->isSameGroupType == ID_FALSE )
                {
                    sMinimumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_SAMEGROUP_FALSE;
                    
                    sMinimumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_DIRECTION_ASC;
                    
                    sMinimumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                        [aInfo->direction];
                }
                else
                {
                    // echar, evarchar�� samegroup compare�� �Ұ����ϴ�.
                    IDE_ASSERT( 0 );
                }
            }
            else
            {
                sMinimumCallBack->compare = mtk::compareMinimumLimit;
            }

            //---------------------------
            // MaximumCallBack ���� ����
            //---------------------------
            
            sMaximumCallBack->columnIdx  = aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->columnDesc, aInfo->column ) )
            {
                sMaximumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->valueDesc, sValueColumn2 ) )
            {
                sMaximumCallBack->valueDesc  = *sValueColumn2;
            }
            
            if( sValue2->mEccLength > 0 )
            {
                // PROJ-1364
                if( aInfo->isSameGroupType == ID_FALSE )
                {
                    sMaximumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_SAMEGROUP_FALSE;
                    
                    sMaximumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_DIRECTION_ASC;
                    
                    sMaximumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                        [aInfo->direction];
                }
                else
                {
                    // echar, evarchar�� samegroup compare�� �Ұ����ϴ�.
                    IDE_ASSERT( 0 );
                }
            }
            else
            {
                if ( ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_MTDVAL ) )
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Mtd;
                }
                else
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Stored;
                }
            }
            
            sMaximumCallBack->value = sValue2;
        }
        else
        {
            // To Fix BUG-12306
            if ( sIsEqual == ID_TRUE )
            {
                if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
                     aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
                {
                    // mtd type�� column value�� ���� range callback 
                    aRange->minimum.callback     = mtk::rangeCallBackGE4Mtd;
                    aRange->maximum.callback     = mtk::rangeCallBackLE4Mtd;
                }
                else
                {
                    if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                         ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
                    {
                        /* MTD_COMPARE_STOREDVAL_MTDVAL
                           stored type�� column value�� ���� range callback */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4Stored;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4Stored;
                    }
                    else
                    {
                        /* PROJ-2433 */
                        aRange->minimum.callback     = mtk::rangeCallBackGE4IndexKey;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4IndexKey;
                    }
                }
            }
            else
            {
                //---------------------------
                // RangeCallBack ����
                //---------------------------

                if ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
                     aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL )
                {
                    // mtd type�� column value�� ���� range callback 
                    aRange->minimum.callback     = mtk::rangeCallBackGT4Mtd;
                    aRange->maximum.callback     = mtk::rangeCallBackLE4Mtd;
                }
                else
                {
                    if ( ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
                         ( aInfo->compValueType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
                    {
                        /* MTD_COMPARE_STOREDVAL_MTDVAL
                           stored type�� column value�� ���� range callback */
                        aRange->minimum.callback     = mtk::rangeCallBackGT4Stored;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4Stored;
                    }
                    else
                    {
                        /* PROJ-2433 */
                        aRange->minimum.callback     = mtk::rangeCallBackGT4IndexKey;
                        aRange->maximum.callback     = mtk::rangeCallBackLE4IndexKey;
                    }
                }
            }
 
            //---------------------------
            // MinimumCallBack ���� ����
            //---------------------------
            
            sMinimumCallBack->columnIdx  = aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->columnDesc, aInfo->column ) )
            {
                sMinimumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMinimumCallBack->valueDesc, sValueColumn2 ) )
            {
                sMinimumCallBack->valueDesc  = *sValueColumn2;
            }
            sMinimumCallBack->value      = sValue2;
            
            if( sValue2->mEccLength > 0 )
            {
                // PROJ-1364
                if( aInfo->isSameGroupType == ID_FALSE )
                {
                    sMinimumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_SAMEGROUP_FALSE;
                    
                    sMinimumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMinimumCallBack->flag |= MTK_COMPARE_DIRECTION_DESC;
                    
                    sMinimumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                        [aInfo->direction];
                }
                else
                {
                    // echar, evarchar�� samegroup compare�� �Ұ����ϴ�.
                    IDE_ASSERT( 0 );
                }
            }
            else
            {
                sMinimumCallBack->compare = mtk::compareMinimumLimit;
            }

            //---------------------------
            // MaximumCallBack ���� ����
            //---------------------------
            
            sMaximumCallBack->columnIdx  = aInfo->columnIdx;
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->columnDesc, aInfo->column ) )
            {
                sMaximumCallBack->columnDesc = *aInfo->column;
            }
            if ( MTC_COLUMN_IS_NOT_SAME( sMaximumCallBack->valueDesc, sValueColumn1 ) )
            {
                sMaximumCallBack->valueDesc  = *sValueColumn1;
            }
            sMaximumCallBack->value      = sValue1;
            
            if( sValue1->mEccLength > 0 )
            {
                // PROJ-1364
                if( aInfo->isSameGroupType == ID_FALSE )
                {
                    sMaximumCallBack->flag &= ~MTK_COMPARE_SAMEGROUP_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_SAMEGROUP_FALSE;
                    
                    sMaximumCallBack->flag &= ~MTK_COMPARE_DIRECTION_MASK;
                    sMaximumCallBack->flag |= MTK_COMPARE_DIRECTION_DESC;
                    
                    sMaximumCallBack->compare =
                        aInfo->column->module->keyCompare[aInfo->compValueType]
                        [aInfo->direction];
                }
                else
                {
                    // echar, evarchar�� samegroup compare�� �Ұ����ϴ�.
                    IDE_ASSERT( 0 );
                }
            }
            else
            {
                if ( ( aInfo->compValueType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_MTDVAL_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL ) ||
                     ( aInfo->compValueType == MTD_COMPARE_INDEX_KEY_MTDVAL ) )
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Mtd;
                }
                else
                {
                    sMaximumCallBack->compare = mtk::compareMaximumLimit4Stored;
                }
            }
        }
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if( ideGetErrorCode() == idERR_ABORT_idnReachEnd )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE));
    }
    else if( ideGetErrorCode() == idERR_ABORT_idnLikeEscape )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));
    }
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4Echar( mtcNode*     aNode,
                               mtcStack*    aStack,
                               SInt         aRemain,
                               void*        aInfo,
                               mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *               ������ ���̿� ��� ��⿡ ���� �б⸦ �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    const mtdModule  * sModule;
    const mtcColumn  * sColumn; 

    mtdEcharType     * sEchar;
    mtdCharType      * sVarchar;
    UChar            * sString;
    UChar            * sStringFence;
    UChar            * sFormat;
    UChar              sEscape = '\0';
    mtdBinaryType    * sTempBinary;

    mtcNode          * sEncNode;
    mtcColumn        * sEncColumn;
    mtcEncryptInfo     sDecryptInfo;
    UShort             sStringPlainLength;
    UChar            * sStringPlain;
    UChar              sStringDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    UShort             sFormatPlainLength;
    UChar            * sFormatPlain;
    UChar              sFormatDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    mtcLikeBlockInfo * sBlock;
    UChar            * sRefineString;

    mtdBooleanType     sResult;
    UInt               sBlockCnt;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        //--------------------------------------------------
        // search string�� plain text�� ��´�.
        //--------------------------------------------------

        sEchar   = (mtdEcharType*)aStack[1].value;
        
        sEncNode = aNode->arguments;
        sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
            + sEncNode->baseColumn;
        
        if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
        {
            sStringPlain = sStringDecryptedBuf;
            
            if( sEchar->mCipherLength > 0 )
            {
                IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                     sEncNode->baseTable,
                                                     sEncNode->baseColumn,
                                                     & sDecryptInfo )
                          != IDE_SUCCESS );
                
                IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                              sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                              sEchar->mValue,
                                              sEchar->mCipherLength,
                                              sStringPlain,
                                              & sStringPlainLength )
                          != IDE_SUCCESS );
                
                IDE_ASSERT_MSG( sStringPlainLength <= sEncColumn->precision,
                                "sStringPlainLength : %"ID_UINT32_FMT"\n"
                                "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                sStringPlainLength, sEncColumn->precision );
            }
            else
            {
                sStringPlainLength = 0;
            }
        }
        else
        {
            sStringPlain = sEchar->mValue;
            sStringPlainLength = sEchar->mCipherLength;
        }
        
        sString      = sStringPlain;
        sStringFence = sStringPlain + sStringPlainLength;
        
        //--------------------------------------------------
        // format string�� plain text�� ��´�.
        //--------------------------------------------------
        
        sEchar = (mtdEcharType*)aStack[2].value;

        sEncNode = sEncNode->next;

        // TASK-3876 codesonar
        IDE_TEST_RAISE( sEncNode == NULL,
                        ERR_INVALID_FUNCTION_ARGUMENT );

        if ( sEncNode->conversion == NULL )
        {
            // conversion�� null�� ��� default policy�� �ƴ�
            // �÷��� format string�� ���� ��찡 �ִ�.
            sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
                + sEncNode->baseColumn;
        
            if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
            {
                sFormatPlain = sFormatDecryptedBuf;
                
                if( sEchar->mCipherLength > 0 )
                {
                    IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                         sEncNode->baseTable,
                                                         sEncNode->baseColumn,
                                                         & sDecryptInfo )
                              != IDE_SUCCESS );
                    
                    IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                                  sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                                  sEchar->mValue,
                                                  sEchar->mCipherLength,
                                                  sFormatPlain,
                                                  & sFormatPlainLength )
                              != IDE_SUCCESS );
                    
                    IDE_ASSERT_MSG( sFormatPlainLength <= sEncColumn->precision,
                                    "sFormatPlainLength : %"ID_UINT32_FMT"\n"
                                    "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                    sFormatPlainLength, sEncColumn->precision );
                }
                else
                {
                    sFormatPlainLength = 0;
                }
            }
            else
            {
                sFormatPlain = sEchar->mValue;
                sFormatPlainLength = sEchar->mCipherLength;
            }
        }
        else
        {
            // conversion�� �ִ� ��� conversion�� �׻�
            // default policy�� ��ȯ�ȴ�.
            sFormatPlain = sEchar->mValue;
            sFormatPlainLength = sEchar->mCipherLength;
        }
        
        sFormat      = sFormatPlain;

        sColumn     = aTemplate->rows[aNode->table].columns + aNode->column;
        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);

        sBlock = (mtcLikeBlockInfo*)(sTempBinary->mValue);

        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[2].column.offset);
        
        sRefineString = (UChar*)(sTempBinary->mValue);
        
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            sEscape = sVarchar->value[0];
        }
        else
        {
            // nothing to do
        }

        IDE_TEST( getMoreInfoFromPattern( sFormat,
                                          sFormatPlainLength,
                                          &sEscape,
                                          1,
                                          sBlock,
                                          &sBlockCnt,
                                          sRefineString,
                                          NULL)
                  != IDE_SUCCESS);

        IDE_TEST( mtfLikeCalculateOnePass( sString,
                                           sStringFence,
                                           sBlock,
                                           sBlockCnt,
                                           &sResult )
                  != IDE_SUCCESS );

        *(mtdBooleanType*)aStack[0].value = sResult;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));    
    
    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateEqual4EcharFast( mtcNode*     aNode,
                                        mtcStack*    aStack,
                                        SInt         aRemain,
                                        void*        aInfo,
                                        mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    mtdEcharType      * sEchar;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    SInt                sCompare;
    
    mtcNode           * sEncNode;
    mtcColumn         * sEncColumn;
    mtcEncryptInfo      sDecryptInfo;
    UShort              sStringPlainLength;
    UChar             * sStringPlain;
    UChar               sStringDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        //--------------------------------------------------
        // search string�� plain text�� ��´�.
        //--------------------------------------------------
        
        sEchar   = (mtdEcharType*)aStack[1].value;
        
        sEncNode = aNode->arguments;
        sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
            + sEncNode->baseColumn;
        
        if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
        {
            sStringPlain = sStringDecryptedBuf;
            
            if( sEchar->mCipherLength > 0 )
            {
                IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                     sEncNode->baseTable,
                                                     sEncNode->baseColumn,
                                                     & sDecryptInfo )
                          != IDE_SUCCESS );
                
                IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                              sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                              sEchar->mValue,
                                              sEchar->mCipherLength,
                                              sStringPlain,
                                              & sStringPlainLength )
                          != IDE_SUCCESS );
                
                IDE_ASSERT_MSG( sStringPlainLength <= sEncColumn->precision,
                                "sStringPlainLength : %"ID_UINT32_FMT"\n"
                                "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                sStringPlainLength, sEncColumn->precision );
            }
            else
            {
                sStringPlainLength = 0;
            }
        }
        else
        {
            sStringPlain = sEchar->mValue;
            sStringPlainLength = sEchar->mCipherLength;
        }
        
        sString = sStringPlain;
        
        //--------------------------------------------------
        // like ����
        //--------------------------------------------------
        
        sFormatInfo = (mtcLikeFormatInfo*) aInfo;
        
        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_NORMAL,
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );

        if ( (sModule->id == MTD_ECHAR_ID) &&
             (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE))
        {
            if ( sStringPlainLength >= sFormatInfo->patternSize )
            {
                sCompare = idlOS::memcmp( sString,
                                          sFormatInfo->pattern,
                                          sFormatInfo->patternSize );
                
                if ( ( sCompare == 0 ) &&
                     ( sStringPlainLength > sFormatInfo->patternSize ) )
                {
                    sString = sStringPlain + sFormatInfo->patternSize;
                    sStringFence = sStringPlain + sStringPlainLength;
                    
                    for( ; sString < sStringFence; sString++ )
                    {
                        if ( *sString != ' ' )
                        {
                            sCompare = 1;
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                sCompare = -1;
            }
        }
        else
        {
            if ( sStringPlainLength == sFormatInfo->patternSize )
            {
                sCompare = idlOS::memcmp( sString,
                                          sFormatInfo->pattern,
                                          sFormatInfo->patternSize );
            }
            else
            {
                sCompare = -1;
            }
        }

        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateIsNotNull4EcharFast( mtcNode*     aNode,
                                            mtcStack*    aStack,
                                            SInt         aRemain,
                                            void*        aInfo,
                                            mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    mtdEcharType      * sEchar;
    mtcLikeFormatInfo * sFormatInfo;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sEchar      = (mtdEcharType*)aStack[1].value;
        sFormatInfo = (mtcLikeFormatInfo*) aInfo;
        
        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_PERCENT,
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );

        if ( sEchar->mEccLength > 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateLength4EcharFast( mtcNode*     aNode,
                                         mtcStack*    aStack,
                                         SInt         aRemain,
                                         void*        aInfo,
                                         mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    mtdEcharType      * sEchar;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    SInt                sCompare;
    
    mtcNode           * sEncNode;
    mtcColumn         * sEncColumn;
    mtcEncryptInfo      sDecryptInfo;
    UShort              sStringPlainLength;
    UChar             * sStringPlain;
    UChar               sStringDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        //--------------------------------------------------
        // search string�� plain text�� ��´�.
        //--------------------------------------------------
        
        sEchar   = (mtdEcharType*)aStack[1].value;
        
        sEncNode = aNode->arguments;
        sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
            + sEncNode->baseColumn;
        
        if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
        {
            sStringPlain = sStringDecryptedBuf;
            
            if( sEchar->mCipherLength > 0 )
            {
                IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                     sEncNode->baseTable,
                                                     sEncNode->baseColumn,
                                                     & sDecryptInfo )
                          != IDE_SUCCESS );
                
                IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                              sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                              sEchar->mValue,
                                              sEchar->mCipherLength,
                                              sStringPlain,
                                              & sStringPlainLength )
                          != IDE_SUCCESS );
                
                IDE_ASSERT_MSG( sStringPlainLength <= sEncColumn->precision,
                                "sStringPlainLength : %"ID_UINT32_FMT"\n"
                                "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                sStringPlainLength, sEncColumn->precision );
            }
            else
            {
                sStringPlainLength = 0;
            }
        }
        else
        {
            sStringPlain = sEchar->mValue;
            sStringPlainLength = sEchar->mCipherLength;
        }
        
        //sString = sStringPlain;
        
        //--------------------------------------------------
        // like ����
        //--------------------------------------------------

        sFormatInfo = (mtcLikeFormatInfo*) aInfo;
        
        IDE_ASSERT( sFormatInfo != NULL );

        if ( sFormatInfo->type == MTC_FORMAT_UNDER )
        {
            // character_length( search_value ) = underCnt
            
            if ( (sModule->id == MTD_ECHAR_ID) &&
                 (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE ))
            {
                if ( sStringPlainLength < sFormatInfo->patternSize )
                {
                    sCompare = -1;
                }
                else
                {
                    sCompare = 0;
                    
                    sString = sStringPlain + sFormatInfo->underCnt;
                    sStringFence = sStringPlain + sStringPlainLength;
                    
                    for( ; sString < sStringFence; sString++ )
                    {
                        if ( *sString != ' ' )
                        {
                            sCompare = 1;
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                }
            }
            else
            {
                if ( sStringPlainLength == sFormatInfo->underCnt )
                {
                    sCompare = 0;
                }
                else
                {
                    sCompare = -1;
                }
            }
        }
        else
        {
            IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_UNDER_PERCENT,
                            "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                            sFormatInfo->type );

            // character_length( search_value ) >= underCnt
            
            if ( sStringPlainLength >= sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }
        
        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateOnePercent4EcharFast( mtcNode*     aNode,
                                             mtcStack*    aStack,
                                             SInt         aRemain,
                                             void*        aInfo,
                                             mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    mtdEcharType      * sEchar;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    SInt                sCompare;
    
    mtcNode           * sEncNode;
    mtcColumn         * sEncColumn;
    mtcEncryptInfo      sDecryptInfo;
    UShort              sStringPlainLength;
    UChar             * sStringPlain;
    UChar               sStringDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        //--------------------------------------------------
        // search string�� plain text�� ��´�.
        //--------------------------------------------------
        
        sEchar   = (mtdEcharType*)aStack[1].value;
        
        sEncNode = aNode->arguments;
        sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
            + sEncNode->baseColumn;
        
        if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
        {
            sStringPlain = sStringDecryptedBuf;
            
            if( sEchar->mCipherLength > 0 )
            {
                IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                     sEncNode->baseTable,
                                                     sEncNode->baseColumn,
                                                     & sDecryptInfo )
                          != IDE_SUCCESS );
                
                IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                              sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                              sEchar->mValue,
                                              sEchar->mCipherLength,
                                              sStringPlain,
                                              & sStringPlainLength )
                          != IDE_SUCCESS );
                
                IDE_ASSERT_MSG( sStringPlainLength <= sEncColumn->precision,
                                "sStringPlainLength : %"ID_UINT32_FMT"\n"
                                "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                sStringPlainLength, sEncColumn->precision );
            }
            else
            {
                sStringPlainLength = 0;
            }
        }
        else
        {
            sStringPlain = sEchar->mValue;
            sStringPlainLength = sEchar->mCipherLength;
        }
        
        sString = sStringPlain;
        sStringFence = sStringPlain + sStringPlainLength;
        
        //--------------------------------------------------
        // like ����
        //--------------------------------------------------
        
        sFormatInfo  = (mtcLikeFormatInfo*) aInfo;
        
        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_NORMAL_ONE_PERCENT ,
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );
        IDE_ASSERT_MSG( sFormatInfo->percentCnt == 1,
                        "sFormatInfo->percentCnt : %"ID_UINT32_FMT"\n",
                        sFormatInfo->percentCnt );

        // search_value = [head]%[tail]
        
        if ( ( sStringPlainLength > 0 ) &&
             ( sStringPlainLength >= sFormatInfo->headSize + sFormatInfo->tailSize ) )
        {
            if ( sFormatInfo->headSize > 0 )
            {
                sCompare = idlOS::memcmp( sString,
                                          sFormatInfo->head,
                                          sFormatInfo->headSize );
            }
            else
            {
                sCompare = 0;
            }

            if ( sCompare == 0 )
            {
                if ( sFormatInfo->tailSize > 0 )
                {
                    sCompare = idlOS::memcmp( sStringFence - sFormatInfo->tailSize,
                                              sFormatInfo->tail,
                                              sFormatInfo->tailSize );
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            sCompare = -1;
        }
        
        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4EcharMB( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *               ������ ���̿� ��� ��⿡ ���� �б⸦ �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/

    const mtdModule  * sModule;
    const mtlModule  * sLanguage;
    const mtcColumn  * sColumn; 

    mtdEcharType     * sEchar;
    mtdCharType      * sVarchar;
    UChar            * sString;
    UChar            * sStringFence;
    UChar            * sFormat;
    UChar            * sEscape = NULL;
    UShort             sEscapeLen = 0;
    mtdBinaryType    * sTempBinary;
    
    mtcNode          * sEncNode;
    mtcColumn        * sEncColumn;
    mtcEncryptInfo     sDecryptInfo;
    UShort             sStringPlainLength;
    UChar            * sStringPlain;
    UChar              sStringDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    UShort             sFormatPlainLength;
    UChar            * sFormatPlain;
    UChar              sFormatDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    mtcLikeBlockInfo * sBlock;
    UChar            * sRefineString;
    
    mtdBooleanType     sResult;
    UInt               sBlockCnt;    

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule   = aStack[1].column->module;
    sLanguage = aStack[1].column->language;

    if( (sModule->isNull( aStack[1].column,
                          aStack[1].value ) == ID_TRUE) ||
        (sModule->isNull( aStack[2].column,
                          aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        //--------------------------------------------------
        // search string�� plain text�� ��´�.
        //--------------------------------------------------

        sEchar   = (mtdEcharType*)aStack[1].value;
        
        sEncNode = aNode->arguments;
        sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
            + sEncNode->baseColumn;
        
        if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
        {
            sStringPlain = sStringDecryptedBuf;
            
            if( sEchar->mCipherLength > 0 )
            {
                IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                     sEncNode->baseTable,
                                                     sEncNode->baseColumn,
                                                     & sDecryptInfo )
                          != IDE_SUCCESS );
                
                IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                              sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                              sEchar->mValue,
                                              sEchar->mCipherLength,
                                              sStringPlain,
                                              & sStringPlainLength )
                          != IDE_SUCCESS );
                
                IDE_ASSERT_MSG( sStringPlainLength <= sEncColumn->precision,
                                "sStringPlainLength : %"ID_UINT32_FMT"\n"
                                "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                sStringPlainLength, sEncColumn->precision );
            }
            else
            {
                sStringPlainLength = 0;
            }
        }
        else
        {
            sStringPlain = sEchar->mValue;
            sStringPlainLength = sEchar->mCipherLength;
        }
        
        sString      = sStringPlain;
        sStringFence = sStringPlain + sStringPlainLength;
        
        //--------------------------------------------------
        // format string�� plain text�� ��´�.
        //--------------------------------------------------
        
        sEchar = (mtdEcharType*)aStack[2].value;

        sEncNode = sEncNode->next;
        
        // TASK-3876 codesonar
        IDE_TEST_RAISE( sEncNode == NULL,
                        ERR_INVALID_FUNCTION_ARGUMENT );
        
        if ( sEncNode->conversion == NULL )
        {
            // conversion�� null�� ��� default policy�� �ƴ�
            // �÷��� format string�� ���� ��찡 �ִ�.
            sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
                + sEncNode->baseColumn;
        
            if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
            {
                sFormatPlain = sFormatDecryptedBuf;
            
                if( sEchar->mCipherLength > 0 )
                {
                    IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                         sEncNode->baseTable,
                                                         sEncNode->baseColumn,
                                                         & sDecryptInfo )
                              != IDE_SUCCESS );
                
                    IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                                  sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                                  sEchar->mValue,
                                                  sEchar->mCipherLength,
                                                  sFormatPlain,
                                                  & sFormatPlainLength )
                              != IDE_SUCCESS );
                
                    IDE_ASSERT_MSG( sFormatPlainLength <= sEncColumn->precision,
                                    "sFormatPlainLength : %"ID_UINT32_FMT"\n"
                                    "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                    sFormatPlainLength, sEncColumn->precision );
                }
                else
                {
                    sFormatPlainLength = 0;
                }
            }
            else
            {
                sFormatPlain = sEchar->mValue;
                sFormatPlainLength = sEchar->mCipherLength;
            }
        }
        else
        {
            // conversion�� �ִ� ��� conversion�� �׻�
            // default policy�� ��ȯ�ȴ�.
            sFormatPlain = sEchar->mValue;
            sFormatPlainLength = sEchar->mCipherLength;
        }
        
        sFormat      = sFormatPlain;
        
        //--------------------------------------------------
        // escape ����
        //--------------------------------------------------
        
        sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);

        sBlock = (mtcLikeBlockInfo*)(sTempBinary->mValue);

        sTempBinary = (mtdBinaryType*)
            ((UChar*)aTemplate->rows[aNode->table].row + sColumn[2].column.offset);
        
        sRefineString = (UChar*)(sTempBinary->mValue); 
            
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            if( sLanguage->id == MTL_UTF16_ID )
            {
                IDE_TEST_RAISE( sVarchar->length != 2, ERR_INVALID_ESCAPE );
            }
            else
            {
                IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            }
            
            sEscape = sVarchar->value;
            sEscapeLen = sVarchar->length;
        }
        else
        {
            //nothing to do 
        }

        IDE_TEST( getMoreInfoFromPatternMB( sFormat,
                                            sFormatPlainLength,
                                            sEscape,
                                            sEscapeLen,
                                            sBlock,
                                            &sBlockCnt,
                                            sRefineString,
                                            sLanguage )
                  != IDE_SUCCESS);

        IDE_TEST( mtfLikeCalculateMBOnePass( sString,
                                             sStringFence,
                                             sBlock,
                                             sBlockCnt,
                                             &sResult,
                                             sLanguage )
                  != IDE_SUCCESS );

        *(mtdBooleanType*)aStack[0].value = sResult;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));    
    
    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateLength4EcharMBFast( mtcNode*     aNode,
                                           mtcStack*    aStack,
                                           SInt         aRemain,
                                           void*        aInfo,
                                           mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *
 * Implementation :
 *
 ***********************************************************************/
    
    const mtlModule   * sLanguage;
    const mtdModule   * sModule;
    mtdEcharType      * sEchar;
    UChar             * sIndex;
    UChar             * sFence;
    mtcLikeFormatInfo * sFormatInfo;
    mtdBigintType       sLength;
    SInt                sCompare;
    idBool              sEqual;
    UChar               sSize;
    
    mtcNode           * sEncNode;
    mtcColumn         * sEncColumn;
    mtcEncryptInfo      sDecryptInfo;
    UShort              sStringPlainLength;
    UChar             * sStringPlain;
    UChar               sStringDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sLanguage   = aStack[1].column->language;
        sLength     = 0;

        //--------------------------------------------------
        // search string�� plain text�� ��´�.
        //--------------------------------------------------
        
        sEchar   = (mtdEcharType*)aStack[1].value;
        
        sEncNode = aNode->arguments;
        sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
            + sEncNode->baseColumn;
        
        if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
        {
            sStringPlain = sStringDecryptedBuf;
            
            if( sEchar->mCipherLength > 0 )
            {
                IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                     sEncNode->baseTable,
                                                     sEncNode->baseColumn,
                                                     & sDecryptInfo )
                          != IDE_SUCCESS );
                
                IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                              sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                              sEchar->mValue,
                                              sEchar->mCipherLength,
                                              sStringPlain,
                                              & sStringPlainLength )
                          != IDE_SUCCESS );
                
                IDE_ASSERT_MSG( sStringPlainLength <= sEncColumn->precision,
                                "sStringPlainLength : %"ID_UINT32_FMT"\n"
                                "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                sStringPlainLength, sEncColumn->precision );
            }
            else
            {
                sStringPlainLength = 0;
            }
        }
        else
        {
            sStringPlain = sEchar->mValue;
            sStringPlainLength = sEchar->mCipherLength;
        }
        
        //--------------------------------------------------
        // like ����
        //--------------------------------------------------

        sFormatInfo = (mtcLikeFormatInfo*) aInfo;
        
        IDE_ASSERT( sFormatInfo != NULL );

        if ( sFormatInfo->type == MTC_FORMAT_UNDER )
        {
            // character_length( search_value ) = underCnt

            if( (sModule->id == MTD_ECHAR_ID) &&
                (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE) )
            {
                sIndex = sStringPlain;
                sFence = sStringPlain + sStringPlainLength;

                while ( sIndex < sFence )
                {
                    (void)sLanguage->nextCharPtr( & sIndex, sFence );
                    
                    sLength++;
                    
                    if ( sLength >= sFormatInfo->underCnt )
                    {
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                if ( sLength < sFormatInfo->underCnt )
                {
                    sCompare = -1;
                }
                else
                {
                    sCompare = 0;
                    
                    while ( sIndex < sFence )
                    {
                        sSize =  mtl::getOneCharSize( sIndex,
                                                      sFence,
                                                      sLanguage );
                        
                        sEqual = mtc::compareOneChar( sIndex,
                                                      sSize,
                                                      sLanguage->specialCharSet[MTL_SP_IDX],
                                                      sLanguage->specialCharSize );
                        
                        if ( sEqual != ID_TRUE )
                        {
                            sCompare = 1;
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                        
                        (void)sLanguage->nextCharPtr( & sIndex, sFence );
                    }
                }
            }
            else
            {
                sIndex = sStringPlain;
                sFence = sStringPlain + sStringPlainLength;

                while ( sIndex < sFence )
                {
                    (void)sLanguage->nextCharPtr( & sIndex, sFence );

                    sLength++;
                    
                    if ( sLength > sFormatInfo->underCnt )
                    {
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                if ( sLength == sFormatInfo->underCnt )
                {
                    sCompare = 0;
                }
                else
                {
                    sCompare = -1;
                }
            }
        }
        else
        {
            IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_UNDER_PERCENT,
                            "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                            sFormatInfo->type );

            // character_length( search_value ) >= underCnt

            sIndex = sStringPlain;
            sFence = sStringPlain + sStringPlainLength;
            
            while ( sIndex < sFence )
            {
                (void)sLanguage->nextCharPtr( & sIndex, sFence );
                
                sLength++;
                
                if ( sLength > sFormatInfo->underCnt )
                {
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
            
            if ( sLength >= sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }
        
        if ( sCompare == 0 )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
   
SInt matchSubString( UChar * aSource,
                     SInt    aSourceSize,
                     UChar * aPattern,
                     SInt    aPatternSize )
{
/***********************************************************************
 *
 * Description :  ��ġ�ϴ� �κ� ���ڿ��� ��ġ�� ���Ѵ�.
 *
 * Implementation :
 *   ���� ����Ʈ�� ��Ƽ ����Ʈ�� ���� ���� ���ϴ� ����� ���� ����ϴ� ������
 * ���� ��ġ �Ǻ��� ���� ������ �Ѵ�.
 *
 ***********************************************************************/

    SInt    i;
    SInt    sHashPattern = 0;
    SInt    sHashSource  = 0;
    SInt    sResult      = 1;
    SInt    sRet;
    UChar * sOldChar;
    UChar * sNewChar;

    if ( aSourceSize >= aPatternSize )
    {
        for ( i = 0; i < aPatternSize - 1; i++ )
        {
            sResult = (sResult << MTC_LIKE_SHIFT ) & MTC_LIKE_HASH_KEY;
        }
        for ( i = 0; i < aPatternSize; i++ )
        {
            sHashPattern = ((sHashPattern << MTC_LIKE_SHIFT) + aPattern[i]) & MTC_LIKE_HASH_KEY;
            sHashSource  = ((sHashSource  << MTC_LIKE_SHIFT) + aSource[i] ) & MTC_LIKE_HASH_KEY;
        }

        sOldChar = aSource;
        sNewChar = aSource + aPatternSize;

        for ( i = 0; i <= aSourceSize - aPatternSize; i++, sOldChar++, sNewChar++ )
        {
            if ( sHashPattern == sHashSource )
            {
                if ( idlOS::memcmp( sOldChar, aPattern, aPatternSize ) == 0 )
                {
                    // match
                    break;
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                // nothing to do
            }

            // BUG-37131 ������ source hash���� ������� �ʴ´�.
            if ( i < aSourceSize - aPatternSize )
            {
                sHashSource = (((sHashSource - (*sOldChar) * sResult) << MTC_LIKE_SHIFT) + (*sNewChar) ) & MTC_LIKE_HASH_KEY;
            }
            else
            {
                // nothing to do
            }
        }

        if ( i <= aSourceSize - aPatternSize )
        {
            sRet = i;
        }
        else
        {
            sRet = -1;
        }
    }
    else
    {
        sRet = -1;
    }

    return sRet;
}

IDE_RC matchSubStringForLOB( mtcLobBuffer     * aBuffer,
                             UInt               aOffset,                         
                             UInt               aLobLength,
                             UChar            * aPattern,
                             UInt               aPatternSize,
                             SInt             * aCmpResult )
{
/***********************************************************************
 *
 * Description :  LOB���� ��ġ�ϴ� �κ� ���ڿ��� ��ġ�� ���Ѵ�.
 *
 * Implementation :
 *   ���� ����Ʈ�� ��Ƽ ����Ʈ�� ���� ���� ���ϴ� ����� ���� ����ϴ� ������
 * ���� ��ġ �Ǻ��� ���� ������ �Ѵ�.   
 *
 ***********************************************************************/
    
    UInt   i;
    SInt   sHashPattern = 0;
    SInt   sHashSource  = 0;
    SInt   sResult      = 1;
    UChar *sOldChar;
    UChar *sNewChar;
    UInt   sMount;
    UInt   sSize;
    UInt   sOffset;    
    UInt   sReadLength;
    UInt   sRemain;    

    *aCmpResult = -1;    

    sOffset = aOffset;    

    if ( sOffset + MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION > aLobLength )
    {
        sMount = aLobLength - sOffset;
        sSize = sMount;
    }
    else
    {
        sMount = MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION;
        sSize  = MTC_LOB_BUFFER_SIZE;
    }

    IDE_TEST_CONT( sSize < aPatternSize, normal_exit );

    sRemain = sSize - aPatternSize;    
        
    IDE_TEST( mtc::readLob( NULL, /* idvSQL* */
                            aBuffer->locator,
                            sOffset,
                            sMount,
                            aBuffer->buf,
                            &sReadLength )
              != IDE_SUCCESS );

    aBuffer->fence = aBuffer->buf + sMount;
    aBuffer->size  = sSize;  
   
    for ( i = 0; i < aPatternSize - 1; i++ )
    {
        sResult = (sResult << MTC_LIKE_SHIFT ) & MTC_LIKE_HASH_KEY;        
    }
    for ( i = 0; i < aPatternSize; i++ )
    {
        sHashPattern = ( (sHashPattern << MTC_LIKE_SHIFT) + aPattern[i] ) & MTC_LIKE_HASH_KEY;
        sHashSource  = ( (sHashSource  << MTC_LIKE_SHIFT) + aBuffer->buf[i] ) & MTC_LIKE_HASH_KEY;        
    }

    while( sOffset + aPatternSize <= aLobLength )
    {
        sOldChar  = aBuffer->buf;    
        sNewChar  = sOldChar + aPatternSize;
        
        for ( i = 0; i <= sRemain; i++, sOldChar++, sNewChar++, sOffset++ )
        {
            if ( sHashPattern == sHashSource )
            {
                if ( idlOS::memcmp( sOldChar, aPattern, aPatternSize ) == 0 )
                {
                    // match
                    *aCmpResult = sOffset - aOffset;
                    
                    IDE_CONT( normal_exit );
                }
                else
                {
                    // nothing to do 
                }
            }
            else
            {
                // nothing to do 
            }

            // BUG-37131 ������ source hash���� ������� �ʴ´�.
            if ( sOffset + aPatternSize < aLobLength )
            {
                sHashSource = ((( sHashSource - (*sOldChar) * sResult ) << MTC_LIKE_SHIFT ) + (*sNewChar) ) & MTC_LIKE_HASH_KEY;
            }
            else
            {
                // nothing to do 
            }
        }

        if ( sOffset + MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION > aLobLength )
        {
            if ( sOffset >= aLobLength )
            {
                break;                
            }
            else
            {
                sMount = aLobLength - sOffset;
                sSize  = sMount;
            }            
        }
        else
        {
            sMount = MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION;
            sSize  = MTC_LOB_BUFFER_SIZE;
        }

        IDE_TEST( mtc::readLob( NULL, /* idvSQL* */
                                aBuffer->locator,
                                sOffset,
                                sMount,
                                aBuffer->buf,
                                &sReadLength )
                  != IDE_SUCCESS );
        
        sRemain = sSize - aPatternSize;                    
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC getMoreInfoFromPattern( const UChar      * aFormat,
                                      UShort             aFormatLen,
                                      const UChar      * aEscape,
                                      UShort             aEscapeLen,
                                      mtcLikeBlockInfo * aBlockInfo,
                                      UInt             * aBlockCnt,
                                      UChar            * aNewString,
                                      const mtlModule  * /*aLanguage*/ )
{
/***********************************************************************
 *
 * Description :  ���� ���ϳ��� �з� �ϰ� Escapeó���� �Ѵ�.
 *
 * Implementation :
 * 3������ ���� %, _, ���ڿ� �� ������ ������ �Ǵ� ������ �������� ������.  
 * ���� ���⼭ escape���ڸ� �����Ѵ�.
 *
 *  �� Ÿ�Կ� ���� cnt�� ��� ���� �ǹ̰� �ٸ���.
 *
 *  ���ڿ�:  ���� ���ڿ��� ����Ʈ
 *  _     :  _�� ��
 *  %     :  %�� ��
 ***********************************************************************/
    
    UChar          * sPattern;
    UChar          * sPatternFence;
    UChar          * sStart    = NULL;
    UChar            sEscape   = '\0';        
    idBool           sNullEscape;
    UChar            sType     = MTC_FORMAT_BLOCK_STRING;
    UInt             sBlockCnt = 0;
    UInt             sTokenCnt = 0;
    UChar          * sPtr;

    // BUG-35504
    // new like������ format length ������ �ִ�.
    IDE_TEST_RAISE( aFormatLen > MTC_LIKE_PATTERN_MAX_SIZE,
                    ERR_LONG_PATTERN );
    
    sPtr = aNewString;    

    // escape ���� ����
    if( aEscapeLen < 1 )
    {
        sNullEscape = ID_TRUE;
    }
    else
    {
        if( aEscapeLen == 1 )
        {
            sNullEscape = ID_FALSE;
            sEscape = *aEscape;
        }
        else
        {            
            IDE_RAISE( ERR_INVALID_ESCAPE );
        }        
    }

    sPattern      = (UChar*) aFormat;
    sPatternFence = sPattern + aFormatLen;

    // ù��° ��¥�� Ÿ���� ��� �´�.

    if ( sPattern < sPatternFence )
    {
        sTokenCnt = 1;
        
        if( (sNullEscape == ID_FALSE) && (*sPattern == sEscape) ) // escape �ɷ�����
        {
            sPattern++;

            IDE_TEST_RAISE( sPattern >= sPatternFence, ERR_INVALID_LITERAL );

            IDE_TEST_RAISE( (*sPattern != (UShort)'%') &&
                            (*sPattern != (UShort)'_') &&
                            (*sPattern != sEscape), // sEsacpe�� null�� �ƴ�
                            ERR_INVALID_LITERAL );

            sType = MTC_FORMAT_BLOCK_STRING;            
        }
        else
        {
            if ( *sPattern == (UShort)'%')
            {
                sType = MTC_FORMAT_BLOCK_PERCENT;                
            }
            else
            {
                if ( *sPattern == (UShort)'_')
                {
                    sType = MTC_FORMAT_BLOCK_UNDER;                    
                }
                else
                {
                    sType = MTC_FORMAT_BLOCK_STRING;
                }
            }
        }
        
        *sPtr  = *sPattern;        
        sStart = sPtr;        
        sPattern++;
        sPtr++;        
    }    

    while( sPattern < sPatternFence )
    {
        if( (sNullEscape == ID_FALSE) && (*sPattern == sEscape) ) // escape �ɷ�����
        {
            sPattern++;

            IDE_TEST_RAISE( sPattern >= sPatternFence, ERR_INVALID_LITERAL );

            IDE_TEST_RAISE( (*sPattern != (UShort)'%') &&
                            (*sPattern != (UShort)'_') &&
                            (*sPattern != sEscape), // sEsacpe�� null�� �ƴ�
                            ERR_INVALID_LITERAL );

            switch( sType )
            {
                case MTC_FORMAT_BLOCK_STRING:
                    sTokenCnt++;                            
                    break;
                    
                case MTC_FORMAT_BLOCK_PERCENT:                           
                case MTC_FORMAT_BLOCK_UNDER:
                    aBlockInfo[sBlockCnt].type      = sType;
                    aBlockInfo[sBlockCnt].start     = sStart;
                    aBlockInfo[sBlockCnt].sizeOrCnt = sTokenCnt;
                    
                    sBlockCnt++;
                    sTokenCnt = 1;
                    sStart    = sPtr;
                    sType     = MTC_FORMAT_BLOCK_STRING;
                    break;
            }
        }
        else
        {
            if ( *sPattern == (UShort)'%')
            {
                switch( sType )
                {
                    case MTC_FORMAT_BLOCK_STRING:
                    case MTC_FORMAT_BLOCK_UNDER:
                        aBlockInfo[sBlockCnt].type      = sType;
                        aBlockInfo[sBlockCnt].start     = sStart;
                        aBlockInfo[sBlockCnt].sizeOrCnt = sTokenCnt;
                        
                        sBlockCnt++;
                        sStart    = sPtr;
                        sTokenCnt = 1;
                        sType     = MTC_FORMAT_BLOCK_PERCENT;
                        break;
                        
                    case MTC_FORMAT_BLOCK_PERCENT:
                        sTokenCnt++;                           
                        break;
                }
            }
            else
            {
                if ( *sPattern == (UShort)'_')
                {
                    switch( sType )
                    {
                        case MTC_FORMAT_BLOCK_STRING:
                        case MTC_FORMAT_BLOCK_PERCENT:  
                            aBlockInfo[sBlockCnt].type      = sType;
                            aBlockInfo[sBlockCnt].start     = sStart;
                            aBlockInfo[sBlockCnt].sizeOrCnt = sTokenCnt;
                            
                            sBlockCnt++;
                            sTokenCnt = 1;                            
                            sStart    = sPtr;
                            sType     = MTC_FORMAT_BLOCK_UNDER;
                            break;
                            
                        case MTC_FORMAT_BLOCK_UNDER:
                            sTokenCnt++;                            
                            break;
                    }
                }
                else
                {
                    switch( sType )
                    {
                        case MTC_FORMAT_BLOCK_STRING:
                            sTokenCnt++;
                            break;
                            
                        case MTC_FORMAT_BLOCK_PERCENT:                           
                        case MTC_FORMAT_BLOCK_UNDER:
                            aBlockInfo[sBlockCnt].type      = sType;
                            aBlockInfo[sBlockCnt].start     = sStart;                
                            aBlockInfo[sBlockCnt].sizeOrCnt = sTokenCnt;
                            
                            sBlockCnt++;
                            sTokenCnt = 1;
                            sStart    = sPtr;
                            sType     = MTC_FORMAT_BLOCK_STRING;
                            break;
                    }
                }
            }
        }
        
        *sPtr = *sPattern;
        sPtr++;
        sPattern++;
    }

    // ������ �ݾ� �ش�.
    if ( sStart != NULL )
    {        
        aBlockInfo[sBlockCnt].type      = sType;
        aBlockInfo[sBlockCnt].start     = sStart;                
        aBlockInfo[sBlockCnt].sizeOrCnt = sTokenCnt;        
        sBlockCnt++;
    }
    else
    {
        // nothing to do 
    }
    
    *aBlockCnt = sBlockCnt;
        
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_LONG_PATTERN );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_LONG_PATTERN ));
    
    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));
    
    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

static IDE_RC getMoreInfoFromPatternMB( const UChar      * aFormat,
                                        UShort             aFormatLen,
                                        const UChar      * aEscape,
                                        UShort             aEscapeLen,
                                        mtcLikeBlockInfo * aBlockInfo,
                                        UInt             * aBlockCnt,
                                        UChar            * aNewString,
                                        const mtlModule  * aLanguage )
{
/***********************************************************************
 *
 * Description :  ���� ���ϳ��� �з� �ϰ� Escapeó���� �Ѵ�.
 *
 * Implementation :
 * 3������ ���� %, _, ���ڿ� �� ������ ������ �Ǵ� ������ �������� ������.  
 * ���� ���⼭ escape���ڸ� �����Ѵ�.
 *
 *  �� Ÿ�Կ� ���� cnt�� ��� ���� �ǹ̰� �ٸ���.
 *
 *  ���ڿ�:  ���� ���ڿ��� ����Ʈ
 *  _     :  _�� �� ----> ���� ���� ����Ʈ �����δ� �󸶰� ���� �� �� ����.
 *  %     :  %�� ��
 ***********************************************************************/
    
    UChar          * sPattern;
    UChar          * sPatternFence;
    UChar          * sStart    = NULL;
    idBool           sNullEscape;
    UChar            sType     = MTC_FORMAT_BLOCK_STRING;
    UInt             sBlockCnt = 0;
    UInt             sTokenCnt = 0;
    UChar          * sPtr;
    UChar          * sPrev;
    UChar            sSize;
    idBool           sEqual;
    idBool           sEqual1;
    idBool           sEqual2;
    idBool           sEqual3;    

    // BUG-35504
    // new like������ format length ������ �ִ�.
    IDE_TEST_RAISE( aFormatLen > MTC_LIKE_PATTERN_MAX_SIZE,
                    ERR_LONG_PATTERN );
    
    sPtr = aNewString;    

    // escape ���� ����
    if ( aLanguage->id == MTL_UTF16_ID )
    {
        // escape ���� ����
        if( aEscapeLen < MTL_UTF16_PRECISION )
        {
            sNullEscape = ID_TRUE;
        }
        else
        {
            if( aEscapeLen == MTL_UTF16_PRECISION )
            {
                sNullEscape = ID_FALSE;
            }
            else
            {
                IDE_RAISE( ERR_INVALID_ESCAPE );
            }
        }
    }
    else
    {        
        if( aEscapeLen < 1 )
        {
            sNullEscape = ID_TRUE;
        }
        else
        {
            if( aEscapeLen == 1 )
            {
                sNullEscape = ID_FALSE;
            }
            else
            {            
                IDE_RAISE( ERR_INVALID_ESCAPE );
            }        
        }
    }    

    sPattern      = (UChar*) aFormat;
    sPatternFence = sPattern + aFormatLen;
    sPrev         = sPattern;    

    // ù��° ������ Ÿ���� ��� �´�.
    if ( sPattern < sPatternFence )
    {
        sSize = mtl::getOneCharSize( sPattern,
                                     sPatternFence,
                                     aLanguage);

        if ( sNullEscape == ID_FALSE )
        {
            sEqual = mtc::compareOneChar( sPattern,
                                          sSize,
                                          (UChar*)aEscape,
                                          aEscapeLen );            
        }
        else
        {
            sEqual = ID_FALSE;            
        }
        
        if ( sEqual == ID_TRUE )
        {
            (void)mtf::nextChar( sPatternFence,
                                 &sPattern,
                                 &sPrev,
                                 aLanguage );
            
            sSize =  mtl::getOneCharSize( sPattern,
                                          sPatternFence,
                                          aLanguage );

            sEqual1 = mtc::compareOneChar( sPattern,
                                           sSize,
                                           aLanguage->specialCharSet[MTL_PC_IDX],
                                           aLanguage->specialCharSize );
            
            sEqual2 = mtc::compareOneChar( sPattern,
                                           sSize,
                                           aLanguage->specialCharSet[MTL_UB_IDX],
                                           aLanguage->specialCharSize );
            
            sEqual3 = mtc::compareOneChar( sPattern,
                                           sSize,
                                           (UChar*)aEscape,
                                           aEscapeLen );

            IDE_TEST_RAISE( (sEqual1 != ID_TRUE) &&
                            (sEqual2 != ID_TRUE) &&
                            (sEqual3 != ID_TRUE),
                            ERR_INVALID_LITERAL );

            // �Ϲ� ������ ���
            sTokenCnt = sSize;
            sType     = MTC_FORMAT_BLOCK_STRING;            
        }
        else
        {
            sEqual = mtc::compareOneChar( sPattern,
                                          sSize,
                                          aLanguage->specialCharSet[MTL_UB_IDX],
                                          aLanguage->specialCharSize );
            
            if ( sEqual == ID_TRUE )
            {
                // Ư������ '_'�� ���
                sType     = MTC_FORMAT_BLOCK_UNDER;
                sTokenCnt = 1;
            }
            else
            {
                sEqual = mtc::compareOneChar( sPattern,
                                              sSize,
                                              aLanguage->specialCharSet[MTL_PC_IDX],
                                              aLanguage->specialCharSize );
                            
                if ( sEqual == ID_TRUE )
                {
                    // Ư������ '%'�� ���
                    sType     = MTC_FORMAT_BLOCK_PERCENT;
                    sTokenCnt = 1;
                }
                else
                {
                    // �Ϲ� ������ ���
                    sType     = MTC_FORMAT_BLOCK_STRING;
                    sTokenCnt = sSize;
                }
            }
            
        }
        
        (void)mtf::nextChar( sPatternFence,
                             &sPattern,
                             &sPrev,
                             aLanguage );

        sStart = sPtr;
        
        idlOS::memcpy( sPtr,
                       sPrev,
                       sPattern - sPrev );
        
        sPtr += sPattern - sPrev;
    }

    while ( sPattern < sPatternFence )
    {
        sSize = mtl::getOneCharSize( sPattern,
                                     sPatternFence,
                                     aLanguage);

        if ( sNullEscape == ID_FALSE )
        {
            sEqual = mtc::compareOneChar( sPattern,
                                          sSize,
                                          (UChar*)aEscape,
                                          aEscapeLen );            
        }
        else
        {
            sEqual = ID_FALSE;            
        }
        
        if ( sEqual == ID_TRUE )
        {
            (void)mtf::nextChar( sPatternFence,
                                 &sPattern,
                                 &sPrev,
                                 aLanguage );
            
            sSize =  mtl::getOneCharSize( sPattern,
                                          sPatternFence,
                                          aLanguage );

            sEqual1 = mtc::compareOneChar( sPattern,
                                           sSize,
                                           aLanguage->specialCharSet[MTL_PC_IDX],
                                           aLanguage->specialCharSize );
            
            sEqual2 = mtc::compareOneChar( sPattern,
                                           sSize,
                                           aLanguage->specialCharSet[MTL_UB_IDX],
                                           aLanguage->specialCharSize );
            
            sEqual3 = mtc::compareOneChar( sPattern,
                                           sSize,
                                           (UChar*)aEscape,
                                           aEscapeLen );

            IDE_TEST_RAISE( (sEqual1 != ID_TRUE) &&
                            (sEqual2 != ID_TRUE) &&
                            (sEqual3 != ID_TRUE),
                            ERR_INVALID_LITERAL );

            // �Ϲ� ������ ���
            switch( sType )
            {
                case MTC_FORMAT_BLOCK_STRING:
                    sTokenCnt+= sSize;
                    break;
                    
                case MTC_FORMAT_BLOCK_PERCENT:                           
                case MTC_FORMAT_BLOCK_UNDER:
                    aBlockInfo[sBlockCnt].type      = sType;
                    aBlockInfo[sBlockCnt].start     = sStart;
                    aBlockInfo[sBlockCnt].sizeOrCnt = sTokenCnt;
                    
                    sBlockCnt++;
                    sTokenCnt = sSize;
                    sStart    = sPtr;
                    sType     = MTC_FORMAT_BLOCK_STRING;
                    break;
            }
        }
        else
        {
            sEqual = mtc::compareOneChar( sPattern,
                                          sSize,
                                          aLanguage->specialCharSet[MTL_UB_IDX],
                                          aLanguage->specialCharSize );
            
            if ( sEqual == ID_TRUE )
            {
                // Ư������ '_'�� ���
                switch( sType )
                {
                    case MTC_FORMAT_BLOCK_STRING:
                    case MTC_FORMAT_BLOCK_PERCENT:  
                        aBlockInfo[sBlockCnt].type      = sType;
                        aBlockInfo[sBlockCnt].start     = sStart;
                        aBlockInfo[sBlockCnt].sizeOrCnt = sTokenCnt;
                        
                        sBlockCnt++;
                        sTokenCnt = 1;
                        sStart    = sPtr;
                        sType     = MTC_FORMAT_BLOCK_UNDER;
                        break;
                        
                    case MTC_FORMAT_BLOCK_UNDER:
                        sTokenCnt++;                            
                        break;
                }
            }
            else
            {
                sEqual = mtc::compareOneChar( sPattern,
                                              sSize,
                                              aLanguage->specialCharSet[MTL_PC_IDX],
                                              aLanguage->specialCharSize );
                            
                if ( sEqual == ID_TRUE )
                {
                    // Ư������ '%'�� ���
                    switch( sType )
                    {
                        case MTC_FORMAT_BLOCK_STRING:
                        case MTC_FORMAT_BLOCK_UNDER:
                            aBlockInfo[sBlockCnt].type      = sType;
                            aBlockInfo[sBlockCnt].start     = sStart;
                            aBlockInfo[sBlockCnt].sizeOrCnt = sTokenCnt;
                            
                            sBlockCnt++;
                            sTokenCnt = 1;
                            sStart    = sPtr;
                            sType     = MTC_FORMAT_BLOCK_PERCENT; 
                            break;
                            
                        case MTC_FORMAT_BLOCK_PERCENT:
                            sTokenCnt++;                           
                            break;
                    }
                }
                else
                {
                    // �Ϲ� ������ ���
                    switch( sType )
                    {
                        case MTC_FORMAT_BLOCK_STRING:
                            sTokenCnt+= sSize;
                            break;
                            
                        case MTC_FORMAT_BLOCK_PERCENT:
                        case MTC_FORMAT_BLOCK_UNDER:
                            aBlockInfo[sBlockCnt].type      = sType;
                            aBlockInfo[sBlockCnt].start     = sStart;
                            aBlockInfo[sBlockCnt].sizeOrCnt = sTokenCnt;
                            
                            sBlockCnt++;
                            sTokenCnt = sSize;
                            sStart    = sPtr;
                            sType     = MTC_FORMAT_BLOCK_STRING;
                            break;
                    }
                }
            }
            
        }
        
        (void)mtf::nextChar( sPatternFence,
                             &sPattern,
                             &sPrev,
                             aLanguage );
        
        idlOS::memcpy( sPtr,
                       sPrev,
                       sPattern - sPrev );

        sPtr += sPattern - sPrev;
    }    

    // ������ �ݾ� �ش�.
    if ( sStart != NULL )
    {        
        aBlockInfo[sBlockCnt].type      = sType;
        aBlockInfo[sBlockCnt].start     = sStart;
        aBlockInfo[sBlockCnt].sizeOrCnt = sTokenCnt;
        sBlockCnt++;
    }
    else
    {
        // nothing to do 
    }
    
    *aBlockCnt = sBlockCnt;
        
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_LONG_PATTERN );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_LONG_PATTERN ));
    
    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));
    
    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

IDE_RC mtfLikeCalculateNormalFast( mtcNode*     aNode,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   void*        aInfo,
                                   mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : �ܼ��� ������ �ƴ� ���
 *                Like�� Calculate ����
 *                ���� ���̿� ��� ��⿡ ���� ó�� ����� �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    mtdCharType       * sVarchar;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    mtdBooleanType      sResult;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sVarchar     = (mtdCharType*)aStack[1].value;
        sString      = sVarchar->value;
        sStringFence = sString + sVarchar->length;
        sVarchar     = (mtdCharType*)aStack[2].value;

        sFormatInfo = (mtcLikeFormatInfo*) aInfo;

        IDE_TEST( mtfLikeCalculateOnePass( sString,
                                           sStringFence,
                                           (sFormatInfo->blockInfo),
                                           sFormatInfo->blockCnt,
                                           &sResult )
                  != IDE_SUCCESS );

        *(mtdBooleanType*)aStack[0].value = sResult;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateMBNormalFast( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate )
{
 /***********************************************************************
 *
 * Description : �ܼ��� ������ �ƴ� ���
 *                Like�� Calculate ����
 *                ���� ���̿� ��� ��⿡ ���� ó�� ����� �����Ѵ�.   
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    const mtlModule   * sLanguage;
    mtdCharType       * sVarchar;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    mtdBooleanType      sResult;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule   = aStack[1].column->module;
    sLanguage = aStack[1].column->language;

    if( (sModule->isNull( aStack[1].column,
                          aStack[1].value ) == ID_TRUE) ||
        (sModule->isNull( aStack[2].column,
                          aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
     
        sVarchar     = (mtdCharType*)aStack[1].value;
        sString      = sVarchar->value;
        sStringFence = sString + sVarchar->length;
        sVarchar     = (mtdCharType*)aStack[2].value;

        sFormatInfo = (mtcLikeFormatInfo*) aInfo;

        IDE_TEST( mtfLikeCalculateMBOnePass( sString,
                                             sStringFence,
                                             (sFormatInfo->blockInfo),
                                             sFormatInfo->blockCnt,
                                             &sResult,
                                             sLanguage )
                  != IDE_SUCCESS );
        
        *(mtdBooleanType*)aStack[0].value = sResult;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4XlobLocatorNormalFast( mtcNode*     aNode,
                                               mtcStack*    aStack,
                                               SInt         aRemain,
                                               void*        aInfo,
                                               mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : �ܼ��� ������ �ƴ� ���
 *                Like�� Calculate ����
 *                ���� ���̿� ��� ��⿡ ���� ó�� ����� �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    mtdClobLocatorType  sLocator = MTD_LOCATOR_NULL;
    SLong               sLobLength;
    mtcLikeFormatInfo * sFormatInfo;
    mtdBooleanType      sResult;
    idBool              sIsNullLob;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLocator = *(mtdClobLocatorType*)aStack[1].value;

    // BUG-48005 Partition Table���� select /*+ use_one_pass_hash() */ ���� error�� ���ϴ�.
    if ( sLocator == MTD_LOCATOR_NULL )
    {
        sIsNullLob = ID_TRUE;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthWithLocator( mtc::getStatistics(aTemplate),
                                                sLocator,
                                                & sLobLength,
                                                & sIsNullLob )
                  != IDE_SUCCESS );
    }

    if( (sIsNullLob == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sFormatInfo = (mtcLikeFormatInfo*) aInfo;

        IDE_TEST( mtfLikeCalculate4XlobLocatorOnePass( sLocator,
                                                       sLobLength,
                                                       (sFormatInfo->blockInfo),
                                                       sFormatInfo->blockCnt,
                                                       &sResult )
                  != IDE_SUCCESS );

        *(mtdBooleanType*)aStack[0].value = sResult;
    }

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4XlobLocatorMBNormalFast( mtcNode*     aNode,
                                                 mtcStack*    aStack,
                                                 SInt         aRemain,
                                                 void*        aInfo,
                                                 mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : �ܼ��� ������ �ƴ� ���
 *                Like�� Calculate ����
 *                ���� ���̿� ��� ��⿡ ���� ó�� ����� �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/

    const mtdModule   * sModule;
    const mtlModule   * sLanguage;    
    mtdClobLocatorType  sLocator = MTD_LOCATOR_NULL;
    SLong               sLobLength;
    mtcLikeFormatInfo * sFormatInfo;
    UInt                sMaxCharSize;
    mtdBooleanType      sResult;
    idBool              sIsNullLob;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sModule   = aStack[1].column->module;
    sLanguage = aStack[1].column->language;

    sLocator = *(mtdClobLocatorType*)aStack[1].value;

    // BUG-48005 Partition Table���� select /*+ use_one_pass_hash() */ ���� error�� ���ϴ�.
    if ( sLocator == MTD_LOCATOR_NULL )
    {
        sIsNullLob = ID_TRUE;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthWithLocator( mtc::getStatistics(aTemplate),
                                                sLocator,
                                                & sLobLength,
                                                & sIsNullLob )
                  != IDE_SUCCESS );
    }

    if( (sIsNullLob == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sMaxCharSize =  MTL_NCHAR_PRECISION(sModule);

        sFormatInfo = (mtcLikeFormatInfo*) aInfo;

        IDE_TEST( mtfLikeCalculate4XlobLocatorMBOnePass( sLocator,
                                                         sLobLength,
                                                         (sFormatInfo->blockInfo),
                                                         sFormatInfo->blockCnt,
                                                         &sResult,
                                                         sMaxCharSize,
                                                         sLanguage,
                                                         aTemplate )
                  != IDE_SUCCESS );

        *(mtdBooleanType*)aStack[0].value = sResult;
    }

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4EcharNormalFast( mtcNode*     aNode,
                                         mtcStack*    aStack,
                                         SInt         aRemain,
                                         void*        aInfo,
                                         mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : �ܼ��� ������ �ƴ� ���
 *                Like�� Calculate ����
 *                ���� ���̿� ��� ��⿡ ���� ó�� ����� �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    mtdEcharType      * sEchar;
    UChar             * sString;
    UChar             * sStringFence;
    mtcNode           * sEncNode;
    mtcColumn         * sEncColumn;
    mtcEncryptInfo      sDecryptInfo;
    UShort              sStringPlainLength;
    UChar             * sStringPlain;
    UChar               sStringDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    UShort              sFormatPlainLength;
    UChar             * sFormatPlain;
    UChar               sFormatDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];   
    
    mtcLikeFormatInfo * sFormatInfo;
    mtdBooleanType      sResult;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        //--------------------------------------------------
        // search string�� plain text�� ��´�.
        //--------------------------------------------------

        sEchar   = (mtdEcharType*)aStack[1].value;
        
        sEncNode = aNode->arguments;
        sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
            + sEncNode->baseColumn;
        
        if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
        {
            sStringPlain = sStringDecryptedBuf;
            
            if( sEchar->mCipherLength > 0 )
            {
                IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                     sEncNode->baseTable,
                                                     sEncNode->baseColumn,
                                                     & sDecryptInfo )
                          != IDE_SUCCESS );
                
                IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                              sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                              sEchar->mValue,
                                              sEchar->mCipherLength,
                                              sStringPlain,
                                              & sStringPlainLength )
                          != IDE_SUCCESS );
                
                IDE_ASSERT_MSG( sStringPlainLength <= sEncColumn->precision,
                                "sStringPlainLength : %"ID_UINT32_FMT"\n"
                                "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                sStringPlainLength, sEncColumn->precision );
            }
            else
            {
                sStringPlainLength = 0;
            }
        }
        else
        {
            sStringPlain = sEchar->mValue;
            sStringPlainLength = sEchar->mCipherLength;
        }
        
        sString      = sStringPlain;
        sStringFence = sStringPlain + sStringPlainLength;

        //--------------------------------------------------
        // format string�� plain text�� ��´�.
        //--------------------------------------------------
        
        sEchar = (mtdEcharType*)aStack[2].value;

        sEncNode = sEncNode->next;
        
        // TASK-3876 codesonar
        IDE_TEST_RAISE( sEncNode == NULL,
                        ERR_INVALID_FUNCTION_ARGUMENT );
        
        if ( sEncNode->conversion == NULL )
        {
            // conversion�� null�� ��� default policy�� �ƴ�
            // �÷��� format string�� ���� ��찡 �ִ�.
            sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
                + sEncNode->baseColumn;
        
            if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
            {
                sFormatPlain = sFormatDecryptedBuf;
            
                if( sEchar->mCipherLength > 0 )
                {
                    IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                         sEncNode->baseTable,
                                                         sEncNode->baseColumn,
                                                         & sDecryptInfo )
                              != IDE_SUCCESS );
                
                    IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                                  sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                                  sEchar->mValue,
                                                  sEchar->mCipherLength,
                                                  sFormatPlain,
                                                  & sFormatPlainLength )
                              != IDE_SUCCESS );
                
                    IDE_ASSERT_MSG( sFormatPlainLength <= sEncColumn->precision,
                                    "sFormatPlainLength : %"ID_UINT32_FMT"\n"
                                    "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                    sFormatPlainLength, sEncColumn->precision );
                }
                else
                {
                    sFormatPlainLength = 0;
                }
            }
            else
            {
                sFormatPlain = sEchar->mValue;
                sFormatPlainLength = sEchar->mCipherLength;
            }
        }
        else
        {
            // conversion�� �ִ� ��� conversion�� �׻�
            // default policy�� ��ȯ�ȴ�.
            sFormatPlain = sEchar->mValue;
            sFormatPlainLength = sEchar->mCipherLength;
        }        

        sFormatInfo = (mtcLikeFormatInfo*) aInfo;            

        IDE_TEST( mtfLikeCalculateOnePass( sString,
                                           sStringFence,
                                           (sFormatInfo->blockInfo),
                                           sFormatInfo->blockCnt,
                                           &sResult )
                  != IDE_SUCCESS );

        *(mtdBooleanType*)aStack[0].value = sResult;
    }    
    
    return IDE_SUCCESS;
   
    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4EcharMBNormalFast( mtcNode*     aNode,
                                           mtcStack*    aStack,
                                           SInt         aRemain,
                                           void*        aInfo,
                                           mtcTemplate* aTemplate )
{

/***********************************************************************
 *
 * Description : �ܼ��� ������ �ƴ� ���
 *                Like�� Calculate ����
 *                ���� ���̿� ��� ��⿡ ���� ó�� ����� �����Ѵ�.   
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    const mtdModule   * sModule;
    const mtlModule   * sLanguage;
    mtdEcharType      * sEchar;
    UChar             * sString;
    UChar             * sStringFence;    
    mtcNode           * sEncNode;
    mtcColumn         * sEncColumn;
    mtcEncryptInfo      sDecryptInfo;
    UShort              sStringPlainLength;
    UChar             * sStringPlain;
    UChar               sStringDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    UShort              sFormatPlainLength;
    UChar             * sFormatPlain;
    UChar               sFormatDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    
    mtcLikeFormatInfo * sFormatInfo;
    mtdBooleanType      sResult;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule   = aStack[1].column->module;
    sLanguage = aStack[1].column->language;

    if( (sModule->isNull( aStack[1].column,
                          aStack[1].value ) == ID_TRUE) ||
        (sModule->isNull( aStack[2].column,
                          aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        //--------------------------------------------------
        // search string�� plain text�� ��´�.
        //--------------------------------------------------

        sEchar   = (mtdEcharType*)aStack[1].value;
        
        sEncNode = aNode->arguments;
        sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
            + sEncNode->baseColumn;
        
        if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
        {
            sStringPlain = sStringDecryptedBuf;
            
            if( sEchar->mCipherLength > 0 )
            {
                IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                     sEncNode->baseTable,
                                                     sEncNode->baseColumn,
                                                     & sDecryptInfo )
                          != IDE_SUCCESS );
                
                IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                              sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                              sEchar->mValue,
                                              sEchar->mCipherLength,
                                              sStringPlain,
                                              & sStringPlainLength )
                          != IDE_SUCCESS );
                
                IDE_ASSERT_MSG( sStringPlainLength <= sEncColumn->precision,
                                "sStringPlainLength : %"ID_UINT32_FMT"\n"
                                "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                sStringPlainLength, sEncColumn->precision );
            }
            else
            {
                sStringPlainLength = 0;
            }
        }
        else
        {
            sStringPlain = sEchar->mValue;
            sStringPlainLength = sEchar->mCipherLength;
        }
            
        sString      = sStringPlain;
        sStringFence = sStringPlain + sStringPlainLength;

        //--------------------------------------------------
        // format string�� plain text�� ��´�.
        //--------------------------------------------------
        
        sEchar = (mtdEcharType*)aStack[2].value;

        sEncNode = sEncNode->next;

        // TASK-3876 codesonar
        IDE_TEST_RAISE( sEncNode == NULL,
                        ERR_INVALID_FUNCTION_ARGUMENT );

        if ( sEncNode->conversion == NULL )
        {
            // conversion�� null�� ��� default policy�� �ƴ�
            // �÷��� format string�� ���� ��찡 �ִ�.
            sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
                + sEncNode->baseColumn;
        
            if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
            {
                sFormatPlain = sFormatDecryptedBuf;
                
                if( sEchar->mCipherLength > 0 )
                {
                    IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                         sEncNode->baseTable,
                                                         sEncNode->baseColumn,
                                                         & sDecryptInfo )
                              != IDE_SUCCESS );
                    
                    IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                                  sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                                  sEchar->mValue,
                                                  sEchar->mCipherLength,
                                                  sFormatPlain,
                                                  & sFormatPlainLength )
                              != IDE_SUCCESS );
                    
                    IDE_ASSERT_MSG( sFormatPlainLength <= sEncColumn->precision,
                                    "sFormatPlainLength : %"ID_UINT32_FMT"\n"
                                    "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                    sFormatPlainLength, sEncColumn->precision );
                }
                else
                {
                    sFormatPlainLength = 0;
                }
            }
            else
            {
                sFormatPlain = sEchar->mValue;
                sFormatPlainLength = sEchar->mCipherLength;
            }
        }
        else
        {
            // conversion�� �ִ� ��� conversion�� �׻�
            // default policy�� ��ȯ�ȴ�.
            sFormatPlain = sEchar->mValue;
            sFormatPlainLength = sEchar->mCipherLength;
        }
        
        sFormatInfo = (mtcLikeFormatInfo*) aInfo;

        IDE_TEST( mtfLikeCalculateMBOnePass( sString,
                                             sStringFence,
                                             (sFormatInfo->blockInfo),
                                             sFormatInfo->blockCnt,
                                             &sResult,
                                             sLanguage )
                  != IDE_SUCCESS );
        
        *(mtdBooleanType*)aStack[0].value = sResult;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));    
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

idBool mtfCheckMatchBlock( mtcLikeBlockInfo ** aBlock,
                           mtcLikeBlockInfo *  aBlockFence,
                           UChar            ** aString,
                           UChar            *  aStringFence)
    
{
    mtcLikeBlockInfo * sBlock;  
    UChar            * sString;
    idBool             sIsTrue;

    sBlock  = *aBlock;
    sString = *aString;
    sIsTrue = ID_TRUE;    

    while( ( sBlock < aBlockFence ) &&
           ( sIsTrue == ID_TRUE) )
    {
        switch( sBlock->type )
        {
            case MTC_FORMAT_BLOCK_STRING:

                if ( aStringFence - sString >= sBlock->sizeOrCnt)
                {
                    if ( idlOS::memcmp( sBlock->start,
                                        sString,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;
                    }
                    else
                    {
                        sString += sBlock->sizeOrCnt;
                        sBlock++;                    
                    }
                }
                else
                {
                    sIsTrue = ID_FALSE;  
                }
                
                break;
                
            case MTC_FORMAT_BLOCK_PERCENT:

                IDE_CONT( normal_exit );                
//                break;
                
            case MTC_FORMAT_BLOCK_UNDER:

                sString += sBlock->sizeOrCnt;
                                        
                if ( sString > aStringFence )
                {
                    sIsTrue = ID_FALSE;                    
                    break;                            
                }
                else
                {
                    sBlock++;                    
                }
                break;
        }        
    }

    IDE_EXCEPTION_CONT( normal_exit );

    if ( sIsTrue == ID_TRUE )
    {
        *aBlock = sBlock;
        *aString = sString;                
    }
    else
    {
        // nothing to do 
    }

    return sIsTrue;
}

idBool mtfCheckMatchBlockMB( mtcLikeBlockInfo ** aBlock,
                             mtcLikeBlockInfo *  aBlockFence,
                             UChar            ** aString,
                             UChar            *  aStringFence,
                             const mtlModule  *  aLanguage)
{
    mtcLikeBlockInfo * sBlock;  
    UChar            * sString;
    idBool             sIsTrue;
    UInt               i;
    UChar            * sPrev;    

    sBlock  = *aBlock;
    sString = *aString;
    sIsTrue = ID_TRUE;    

    while( ( sBlock < aBlockFence ) &&
           ( sIsTrue == ID_TRUE) )
    {
        switch( sBlock->type )
        {
            case MTC_FORMAT_BLOCK_STRING:

                if ( aStringFence - sString >= sBlock->sizeOrCnt )
                {
                    if ( idlOS::memcmp( sBlock->start,
                                        sString,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;
                    }
                    else
                    {
                        sString += sBlock->sizeOrCnt;
                        sBlock++;                    
                    }
                }
                else
                {
                    sIsTrue = ID_FALSE;                    
                }
                
                break;
                
            case MTC_FORMAT_BLOCK_PERCENT:

                IDE_CONT( normal_exit );                
//                break;
                
            case MTC_FORMAT_BLOCK_UNDER:

                for( i = 0; i < sBlock->sizeOrCnt; i++ )
                {
                    if ( sString >= aStringFence )
                    {
                        sIsTrue = ID_FALSE;
                        break;
                    }
                    else
                    {
                        // nothing to do
                    }
                    
                    if ( mtf::nextChar( aStringFence,
                                        &sString,
                                        &sPrev,
                                        aLanguage ) != NC_VALID )
                    {
                        sIsTrue = ID_FALSE;
                        break;                        
                    }
                    else
                    {
                        // nothing to do
                    }
                }                
                                        
                if ( sIsTrue == ID_TRUE )
                {
                    sBlock++;                    
                }
                else
                {
                    // nothing to do
                }
                
                break;
        }        
    }

    IDE_EXCEPTION_CONT( normal_exit );

    if ( sIsTrue == ID_TRUE )
    {
        *aBlock  = sBlock;
        *aString = sString;                
    }
    else
    {
        // nothing to do 
    }

    return sIsTrue;
}

IDE_RC mtfCheckMatchBlockForLOB( mtcLobBuffer     *  aBuffer,
                                 UInt             *  aOffset,
                                 UInt                aLobLength,
                                 mtcLikeBlockInfo ** aBlock,
                                 mtcLikeBlockInfo *  aBlockFence,
                                 idBool           *  aIsTrue)
{
    mtcLikeBlockInfo * sBlock;  
    idBool             sIsTrue;
    UInt               sOffset;
    UInt               sReadLength;

    sBlock  = *aBlock;
    sOffset = *aOffset;    
    sIsTrue =  ID_TRUE;    

    while( ( sBlock < aBlockFence ) &&
           ( sIsTrue == ID_TRUE) )
    {
        switch( sBlock->type )
        {
            case MTC_FORMAT_BLOCK_STRING:

                if ( aLobLength - sOffset >= sBlock->sizeOrCnt )
                {
                    IDE_TEST( mtc::readLob( NULL, /* idvSQL* */
                                            aBuffer->locator,
                                            sOffset,
                                            sBlock->sizeOrCnt,
                                            aBuffer->buf,
                                            &sReadLength )
                              != IDE_SUCCESS );

                    if ( idlOS::memcmp( sBlock->start,
                                        aBuffer->buf,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;                                                
                    }
                    else
                    {
                        sOffset += sBlock->sizeOrCnt;
                        sBlock++;                    
                    }
                }
                else
                {
                    sIsTrue = ID_FALSE;                    
                }
                
                break;
                
            case MTC_FORMAT_BLOCK_PERCENT:

                IDE_CONT( normal_exit );                
//                break;
                
            case MTC_FORMAT_BLOCK_UNDER:

                sOffset += sBlock->sizeOrCnt;
                                        
                if ( sOffset > aLobLength )
                {
                    sIsTrue = ID_FALSE;                    
                    break;                            
                }
                else
                {
                    sBlock++;                    
                }
                break;
        }        
    }

    IDE_EXCEPTION_CONT( normal_exit );

    if ( sIsTrue == ID_TRUE )
    {
        *aBlock  = sBlock;
        *aOffset = sOffset;
        *aIsTrue = ID_TRUE;        
    }
    else
    {
        *aIsTrue = ID_FALSE;        
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfCheckMatchBlockMBForLOB( mtcLobBuffer     *  aBuffer,
                                   UInt             *  aOffset,
                                   UInt                aLobLength,
                                   mtcLikeBlockInfo ** aBlock,
                                   mtcLikeBlockInfo *  aBlockFence,
                                   const mtlModule  *  aLanguage,
                                   idBool           *  aIsTrue)
{
    mtcLikeBlockInfo * sBlock;  
    idBool             sIsTrue;
    UInt               sOffset;
    UInt               sReadLength;
    UInt               i;
    UChar            * sNow;
    UChar            * sPrev;
    UInt               sMount;    
    
    sBlock  = *aBlock;
    sOffset = *aOffset;    
    sIsTrue = ID_TRUE;    

    while( ( sBlock < aBlockFence ) &&
           ( sIsTrue == ID_TRUE) )
    {
        switch( sBlock->type )
        {
            case MTC_FORMAT_BLOCK_STRING:

                if ( aLobLength - sOffset >= sBlock->sizeOrCnt )
                {
                    IDE_TEST( mtc::readLob( NULL, /* idvSQL* */
                                            aBuffer->locator,
                                            sOffset,
                                            sBlock->sizeOrCnt,
                                            aBuffer->buf,
                                            &sReadLength )
                              != IDE_SUCCESS );             

                    if ( idlOS::memcmp( sBlock->start,
                                        aBuffer->buf,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;                                                
                    }
                    else
                    {
                        sOffset += sBlock->sizeOrCnt;
                        sBlock++;                    
                    }
                }
                else
                {
                    sIsTrue = ID_FALSE;                    
                }
                
                break;
                
            case MTC_FORMAT_BLOCK_PERCENT:

                IDE_CONT( normal_exit );                
//                break;
                
            case MTC_FORMAT_BLOCK_UNDER:

                if ( aLobLength > sBlock->sizeOrCnt * MTL_MAX_PRECISION + sOffset )
                {
                    sMount = sBlock->sizeOrCnt * MTL_MAX_PRECISION;                    
                }
                else
                {
                    sMount = aLobLength - sOffset;                    
                }                

                IDE_TEST( mtc::readLob( NULL, /* idvSQL* */
                                        aBuffer->locator,
                                        sOffset,
                                        sMount,
                                        aBuffer->buf,
                                        &sReadLength )
                          != IDE_SUCCESS );

                sNow = aBuffer->buf;

                for ( i = 0; i < sBlock->sizeOrCnt; i++)
                {
                    if ( sNow >= aBuffer->buf + sReadLength )
                    {
                        sIsTrue = ID_FALSE;
                        break;                        
                    }
                    else
                    {
                        // nothing to do
                    }
                    
                    if ( mtf::nextChar( (UChar*)(&(aBuffer->buf[sMount])),
                                        &sNow,
                                        &sPrev,
                                        aLanguage )
                         != NC_VALID )
                    {
                        sIsTrue = ID_FALSE;
                        break;                        
                    }
                    else
                    {
                        // nothing to do
                    }

                    sOffset += sNow - sPrev;
                }
                
                if ( sIsTrue == ID_TRUE )
                {
                    sBlock++;                    
                }
                break;
        }        
    }

    IDE_EXCEPTION_CONT( normal_exit );

    if ( sIsTrue == ID_TRUE )
    {
        *aBlock  = sBlock;
        *aOffset = sOffset;
        *aIsTrue = ID_TRUE;        
    }
    else
    {
        *aIsTrue = ID_FALSE;        
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateOnePass( UChar            * aString,
                                UChar            * aStringFence,
                                mtcLikeBlockInfo * aBlock,
                                UInt               aBlockCnt,
                                mtdBooleanType   * aResult )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    UChar             * sString;
    UChar             * sStringFence;
    UChar             * sTempString;
    idBool              sIsTrue = ID_TRUE;
    mtcLikeBlockInfo  * sBlock;
    mtcLikeBlockInfo  * sTempBlock;
    mtcLikeBlockInfo  * sBlockStart;    
    mtcLikeBlockInfo  * sBlockFence;
    SInt                sCmpResult;
    
    sString      = aString;    
    sStringFence = aStringFence;    

    sBlockStart = aBlock;        

    IDE_ASSERT( aBlockCnt != 0 );

    sBlockFence = aBlock + aBlockCnt;

    sBlock = sBlockFence - 1;

    /*    %aaa%bbbb%ccc �� ���� ��� ccc�� ���� �ϴ� �κ��̴� */
        
    while( (sBlock >= sBlockStart ) &&
           (sIsTrue == ID_TRUE) )
    {
        if ( sBlock->type == MTC_FORMAT_BLOCK_PERCENT )
        {
            sBlock++;                
            break;                
        }
        else
        {
            /* �ں��� �˻��Ѵ� */
            if ( sBlock->type == MTC_FORMAT_BLOCK_STRING )
            {
                if ( sStringFence - sString >= sBlock->sizeOrCnt )
                {                    
                    if ( idlOS::memcmp( sBlock->start,
                                        sStringFence - sBlock->sizeOrCnt,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;                        
                    }
                    else
                    {
                        sStringFence -= sBlock->sizeOrCnt;
                        sBlock--;
                    }
                }
                else
                {
                    sIsTrue = ID_FALSE;                    
                }
                
            }
            else // MTC_FORMAT_UNDER
            {
                if ( sString + sBlock->sizeOrCnt <= sStringFence )
                {
                    sStringFence -= sBlock->sizeOrCnt;
                    sBlock--;
                }
                else
                {
                    sIsTrue = ID_FALSE;                        
                }
            }
        }
    }        

    sBlockFence = sBlock;

    sBlock = sBlockStart;
            
    while ( (sBlock < sBlockFence) && (sIsTrue == ID_TRUE) )
    {            
        switch ( sBlock->type )
        {
            case MTC_FORMAT_BLOCK_STRING:
                
                /*  ó�� ���ڿ��̰ų� �Ǵ� �ܼ� ���ϰ�쿡�� �� �� �ִ�.
                     __string���� __�� ó�� �� �ڿ� ���ų�
                     string ���¿��� �ҷ����� */
                if ( sStringFence - sString >= sBlock->sizeOrCnt )
                {
                    if ( idlOS::memcmp( sBlock->start,
                                        sString,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;                        
                    }
                    else
                    {
                        sString = sString + sBlock->sizeOrCnt;
                    }
                }
                else
                {
                    sIsTrue = ID_FALSE;                    
                }
                
                sBlock++;                    
                    
                break;

            case MTC_FORMAT_BLOCK_UNDER:
                /* ���� ����ŭ �ڷ� ������.*/

                if ( sString + sBlock->sizeOrCnt <= sStringFence )
                {
                    sString += sBlock->sizeOrCnt;
                }
                else
                {
                    sIsTrue = ID_FALSE;
                }

                sBlock++;
                    
                break;    
                    
            case MTC_FORMAT_BLOCK_PERCENT:

                if ( sBlock + 1 < sBlockFence )
                {
                    sBlock++;                      
                        
                    if ( sBlock->type == MTC_FORMAT_BLOCK_UNDER )
                    {
                        /*  __ �� �������� ������ ��� �׸�ŭ ��¥�� �δ� */

                        sString += sBlock->sizeOrCnt;
                        sBlock++;                    
                    }
                    else
                    {
                        // nothing to do                        
                    }

                    if ( sString > sStringFence )
                    {
                        sIsTrue = ID_FALSE;
                        break;                        
                    }
                    else
                    {
                        // nothing to do
                    }
                        
                    if ( sBlock < sBlockFence )
                    {
                        if ( sBlock->type == MTC_FORMAT_BLOCK_STRING )
                        {                       
                            while ( (sString < sStringFence) &&
                                    (sIsTrue == ID_TRUE) )
                            {                       
                                sCmpResult = matchSubString( sString,
                                                             sStringFence - sString,
                                                             sBlock->start,
                                                             sBlock->sizeOrCnt );

                                if ( sCmpResult == -1 )
                                {
                                    /* �κ� ���ڿ� �˻��� ���� ���� ���� ���̴� */
                                    sIsTrue = ID_FALSE;
                                }
                                else
                                {
                                    /* �� ������ ��Ī�� �����Ѵ� */
                                    sTempBlock  = sBlock  + 1;
                                    sTempString = sString + sCmpResult + sBlock->sizeOrCnt;
                                    sIsTrue     = ID_TRUE;

                                    if ( mtfCheckMatchBlock( &sTempBlock,
                                                             sBlockFence,
                                                             &sTempString,
                                                             sStringFence )
                                         != ID_TRUE )
                                    {
                                        /* �� ��Ī�� Ʋ������ ���� ���������� �̵���Ų��*/
                                        sString += (sCmpResult + 1);                                            
                                    }
                                    else
                                    {
                                        sBlock  = sTempBlock;
                                        sString = sTempString;                                            
                                        break;                                            
                                    }
                                }
                            }
                        }
                        else // __�� ���� �о����� %�̴�.
                        {
                            // nothing to do
                        }
                    }
                    else // __�� ���� �� �о����� �� �̻� ���� �ʾҴ�.
                    {
                        // nothing to do
                    }
                }                     
                else // ������ %%               
                {                    
                    sBlock++;
                    sString = sStringFence;                        
                }                    
                break;                              
        }
    }

    if( sIsTrue == ID_TRUE )
    {               
        if ( sString == sStringFence )
        {
            *aResult = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *aResult = MTD_BOOLEAN_FALSE;
        }            
    }
    else
    {
        *aResult = MTD_BOOLEAN_FALSE;
    }
    
    return IDE_SUCCESS;    
    
//    IDE_EXCEPTION_END;
    
//    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateMBOnePass( UChar            * aString,
                                  UChar            * aStringFence,
                                  mtcLikeBlockInfo * aBlock,
                                  UInt               aBlockCnt,                                         
                                  mtdBooleanType   * aResult,
                                  const mtlModule  * aLanguage )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    UChar             * sString;
    UChar             * sStringFence;
    UChar             * sTempString;    
    UInt                i;
    mtcLikeBlockInfo  * sBlock;
    mtcLikeBlockInfo  * sTempBlock;
    mtcLikeBlockInfo  * sBlockFence;
    mtcLikeBlockInfo  * sBlockStart;
    idBool              sIsTrue = ID_TRUE;
    UChar             * sPrev;
    SInt                sCmpResult;
    UInt                sCount;
    mtlNCRet            sNcRet = NC_INVALID;
    UChar             * sValidChar;        
    
    sString      = aString;
    sStringFence = aStringFence;    
    sValidChar   = sString;            
    
    sBlockStart = aBlock;

    IDE_ASSERT( aBlockCnt != 0 );
    
    sBlockFence = aBlock + aBlockCnt;    
    
    sBlock = sBlockFence - 1;

    /*    %aaa%bbbb%ccc �� ���� ��� ccc�� ���� �ϴ� �κ��̴� */
    
    while( (sBlock >= sBlockStart ) &&
           (sIsTrue == ID_TRUE) )
    {       
        if ( sBlock->type == MTC_FORMAT_BLOCK_PERCENT )
        {
            sBlock++;                
            break;                
        }
        else
        {
            /* �ں��� �˻��Ѵ� */
            if ( sBlock->type == MTC_FORMAT_BLOCK_STRING )
            {
                if ( sStringFence - sString >= sBlock->sizeOrCnt )
                {   
                    if ( idlOS::memcmp( sBlock->start,
                                        sStringFence - sBlock->sizeOrCnt,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;
                    }
                    else
                    {
                        sStringFence -= sBlock->sizeOrCnt;
                        sBlock--;
                    }
                }
                else
                {
                    sIsTrue = ID_FALSE;                        
                }
            }                 
            else // MTC_FORMAT_UNDER
            {
                sCount = 0;
                
                while( sCount < sBlock->sizeOrCnt )
                {
                    sTempString = sStringFence - 1;

                    if ( sTempString < sString )
                    {
                        sIsTrue = ID_FALSE;                            
                        break;
                    }

                    while ( sTempString >= sString )
                    {
                        sNcRet = mtf::nextChar( sStringFence,
                                                &sTempString,
                                                &sPrev,
                                                aLanguage);

                        if ( sNcRet == NC_VALID )
                        {
                            sStringFence = sPrev;
                            break;                            
                        }
                        else
                        {
                            sTempString = sPrev - 1;                                
                        }   
                    }

                    if ( sNcRet == NC_VALID )
                    {
                        sCount++;                        
                    }
                    else
                    {
                        sIsTrue = ID_FALSE;                        
                        break;                        
                    }
                }
                sBlock--;                
            }
        }
    }        

    sBlockFence = sBlock;

    sBlock = sBlockStart;
            
    while ( sBlock < sBlockFence && sIsTrue == ID_TRUE )
    {
        switch ( sBlock->type )
        {
            case MTC_FORMAT_BLOCK_STRING:
                /*  ó�� ���ڿ��̰ų� �Ǵ� �ܼ� ���ϰ�쿡�� �� �� �ִ�.
                     __string���� __�� ó�� �� �ڿ� ���ų�
                     string ���¿��� �ҷ����� */

                if ( sStringFence - sString >= sBlock->sizeOrCnt )
                {   
                    if ( idlOS::memcmp( sBlock->start,
                                        sString,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;                        
                    }
                    else
                    {
                        sString = sString + sBlock->sizeOrCnt;
                    }
                }
                else
                {
                    sIsTrue = ID_FALSE;                    
                }

                sBlock++;                    
                    
                break;

            case MTC_FORMAT_BLOCK_UNDER:
                /* ���� ����ŭ �ڷ� ������.*/

                for ( i = 0 ; i < sBlock->sizeOrCnt; i++)
                {
                    if ( sString >= sStringFence )
                    {
                        sIsTrue = ID_FALSE;
                        break;
                    }
                    else
                    {
                        // nothing to do
                    }
                    
                    if ( mtf::nextChar( sStringFence,
                                        &sString,
                                        &sPrev,
                                        aLanguage ) != NC_VALID )
                    {
                        sIsTrue = ID_FALSE;
                        break;                        
                    }
                    else
                    {
                        // nothing to do
                    }
                }
                
                sBlock++;
                    
                break;    
                    
            case MTC_FORMAT_BLOCK_PERCENT:

                if ( sBlock + 1 < sBlockFence )
                {
                    sBlock++;                      
                        
                    if ( sBlock->type == MTC_FORMAT_BLOCK_UNDER )
                    {
                        /*  __ �� �������� ������ ��� �׸�ŭ ��¥�� �δ� */
                        for ( i = 0 ; i < sBlock->sizeOrCnt; i++)
                        {
                            if ( sString >= sStringFence )
                            {
                                sIsTrue = ID_FALSE;
                                break;                            
                            }
                            else
                            {
                                // nothing to do
                            }
                            
                            if ( mtf::nextChar( sStringFence,
                                                &sString,
                                                &sPrev,
                                                aLanguage ) != NC_VALID )
                            {
                                sIsTrue = ID_FALSE;
                                break;                        
                            }
                            else
                            {
                                // nothing to do
                            }
                        }
                        
                        sBlock++;                            
                    }
                    else
                    {
                        // nothing to do                            
                    }

                    if ( sString > sStringFence )
                    {
                        sIsTrue = ID_FALSE;
                        break;                            
                    }
                    else
                    {
                        // nothing to do
                    }   
                    
                    if ( sBlock < sBlockFence )
                    {
                        if ( sBlock->type == MTC_FORMAT_BLOCK_STRING )
                        {                       
                            while ( (sString < sStringFence) &&
                                    (sIsTrue == ID_TRUE) )
                            {                       
                                sCmpResult = matchSubString( sString,
                                                             sStringFence - sString,
                                                             sBlock->start,
                                                             sBlock->sizeOrCnt );
                                
                                if ( sCmpResult == -1 )                                    
                                {
                                    /* �κ� ���ڿ� �˻��� ���� ���� ���� ���̴� */
                                    sIsTrue = ID_FALSE;                                    
                                }
                                else
                                {
                                    /* matchSubString�� ��� Multibyte�� ��� ���� �ʾҴ� ����
                                       �߸��� ������� ���� �� �� �ִ�.
                                       �˻��� ����� ���� ���ڿ��� ����� �� �������̶��
                                       ����� �˻��� �Ѱ��̴�. */
                                    
                                    while( sValidChar < sString + sCmpResult )
                                    {
                                        (void)mtf::nextChar( sStringFence,
                                                             &sValidChar,
                                                             &sPrev,
                                                             aLanguage);                                            
                                    }
                                    
                                    if ( sValidChar == sString + sCmpResult )
                                    {
                                        /* �� ������ ��Ī�� �����Ѵ� */
                                        sTempBlock  = sBlock + 1;
                                        sTempString = sString + sCmpResult + sBlock->sizeOrCnt;
                                        sIsTrue     = ID_TRUE;

                                        if ( mtfCheckMatchBlockMB( &sTempBlock,
                                                                   sBlockFence,
                                                                   &sTempString,
                                                                   sStringFence,
                                                                   aLanguage ) != ID_TRUE )
                                        {
                                            /* �� ��Ī�� Ʋ������ ���� ���������� �̵���Ų��*/
                                            sString += sCmpResult;
                                            (void)mtf::nextChar( sStringFence,
                                                                 &sString,
                                                                 &sPrev,
                                                                 aLanguage);                                            
                                        }
                                        else
                                        {
                                            sBlock  = sTempBlock;
                                            sString = sTempString;
                                            
                                            break;                                            
                                        }
                                    }
                                    else
                                    {
                                        sString = sValidChar;
                                    }
                                }
                            }
                        }
                        else // __�� ���� �о����� %�̴�.
                        {
                            // nothing to do
                        }
                    }
                    else // __�� ���� �� �о����� �� �̻� ���� �ʾҴ�.
                    {   
                        // nothing to do 
                    }
                }
                else // ������ %%               
                {                    
                    sBlock++;
                    sString = sStringFence;                        
                }                    
                break;                              
        }
    }        

    if( sIsTrue == ID_TRUE )
    {   
        if ( sString == sStringFence )
        {
           *aResult = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *aResult = MTD_BOOLEAN_FALSE;
        }            
    }
    else
    {
        *aResult = MTD_BOOLEAN_FALSE;
    }
    
    return IDE_SUCCESS;    
    
//    IDE_EXCEPTION_END;
    
//    return IDE_FAILURE;
}


IDE_RC mtfLikeCalculate4XlobLocatorOnePass( mtdClobLocatorType  aLocator,
                                            UInt                aLobLength,
                                            mtcLikeBlockInfo  * aBlock,
                                            UInt                aBlockCnt,
                                            mtdBooleanType    * aResult )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    UChar               sBuf[MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION] = {0,};
    mtcLobBuffer        sBuffer;    
    UInt                sLobLength;
    UInt                sOffset = 0;
    UInt                sTempOffset;    
    mtcLikeBlockInfo  * sBlock;
    mtcLikeBlockInfo  * sTempBlock;
    mtcLikeBlockInfo  * sBlockFence;
    mtcLikeBlockInfo  * sBlockStart;
    idBool              sIsTrue     = ID_TRUE;
    idBool              sIsTempTrue = ID_TRUE;    
    UInt                sReadLength;
    SInt                sCmpResult;
    
    sBuffer.locator = aLocator;
    sBuffer.buf     = sBuf;
    sLobLength      = aLobLength;
    sBlockStart     = aBlock;        

    IDE_ASSERT( aBlockCnt != 0 );

    sBlockFence = aBlock + aBlockCnt;

    sBlock = sBlockFence - 1;

    /*    %aaa%bbbb%ccc �� ���� ��� ccc�� ���� �ϴ� �κ��̴� */
        
    while( (sBlock >= sBlockStart ) &&
           (sIsTrue == ID_TRUE) )
    {
        if ( sBlock->type == MTC_FORMAT_BLOCK_PERCENT )
        {
            sBlock++;                
            break;                
        }
        else
        {
            if ( sBlock->type == MTC_FORMAT_BLOCK_STRING )
            {
                if ( sLobLength > sBlock->sizeOrCnt )
                {
                    IDE_TEST( mtc::readLob( NULL, /* idvSQL* */
                                            aLocator,
                                            sLobLength - sBlock->sizeOrCnt,
                                            sBlock->sizeOrCnt,
                                            sBuf,
                                            &sReadLength )
                              != IDE_SUCCESS );
                    
                    if ( idlOS::memcmp( sBlock->start,
                                        sBuf,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;                        
                    }
                    else
                    {
                        sLobLength -= sBlock->sizeOrCnt;
                        sBlock--;
                    }
                }
                else
                {
                    sIsTrue = ID_FALSE;                        
                }
                    
            }
            else // MTC_FORMAT_UNDER
            {
                if ( sBlock->sizeOrCnt <= sLobLength )
                {
                    sLobLength -= sBlock->sizeOrCnt;
                    sBlock--;
                }
                else
                {
                    sIsTrue = ID_FALSE;                        
                }
            }
        }
    }        

    sBlockFence = sBlock;
        
    sBlock = sBlockStart;
            
    while ( sBlock < sBlockFence && sIsTrue == ID_TRUE )
    {            
        switch ( sBlock->type )
        {
            case MTC_FORMAT_BLOCK_STRING:

                 /*  ó�� ���ڿ��̰ų� �Ǵ� �ܼ� ���ϰ�쿡�� �� �� �ִ�.
                     __string���� __�� ó�� �� �ڿ� ���ų�
                     string ���¿��� �ҷ����� */
                if ( sOffset + sBlock->sizeOrCnt <= sLobLength )
                {                        
                    IDE_TEST( mtc::readLob( NULL, /* idvSQL* */
                                            aLocator,
                                            sOffset,
                                            sBlock->sizeOrCnt,
                                            sBuf,
                                            &sReadLength )
                              != IDE_SUCCESS );
                    
                    if ( idlOS::memcmp( sBlock->start,
                                        sBuf,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;                        
                    }
                    else
                    {
                        sOffset += sBlock->sizeOrCnt;
                    }
                }
                else
                {
                    sIsTrue = ID_FALSE;                        
                }

                sBlock++;                    
                    
                break;

            case MTC_FORMAT_BLOCK_UNDER:
                /* ���� ����ŭ �ڷ� ������.*/

                if ( sOffset + sBlock->sizeOrCnt <= sLobLength )
                {
                    sOffset += sBlock->sizeOrCnt;
                }
                else
                {
                    sIsTrue = ID_FALSE;
                }

                sBlock++;
                    
                break;    
                    
            case MTC_FORMAT_BLOCK_PERCENT:

                if ( sBlock + 1 < sBlockFence )
                {
                    sBlock++;                      
                        
                    if ( sBlock->type == MTC_FORMAT_BLOCK_UNDER )
                    {
                        /*  __ �� �������� ������ ��� �׸�ŭ ��¥�� �δ� */
                        sOffset += sBlock->sizeOrCnt;
                        sBlock++;                        
                    }
                    else
                    {
                        // nothing to do
                    }

                    if ( sOffset > sLobLength )
                    {
                        sIsTrue = ID_FALSE;
                        break;
                    }
                    else
                    {
                        // nothing to do
                    }
                        
                    if ( sBlock < sBlockFence )
                    {
                        if ( sBlock->type == MTC_FORMAT_BLOCK_STRING )
                        {                       
                            while ( (sOffset < sLobLength) &&
                                    (sIsTrue == ID_TRUE) )
                            {
                                IDE_TEST( matchSubStringForLOB( &sBuffer,
                                                                sOffset,
                                                                sLobLength,                                                      
                                                                sBlock->start,
                                                                sBlock->sizeOrCnt,
                                                                &sCmpResult )
                                          != IDE_SUCCESS);                                    

                                if ( sCmpResult == -1 )
                                {
                                    /* �κ� ���ڿ� �˻��� ���� ���� ���� ���̴� */
                                    sIsTrue = ID_FALSE;                                    
                                }
                                else
                                {
                                    /* �� ������ ��Ī�� �����Ѵ� */
                                    sTempBlock  = sBlock + 1;
                                    sTempOffset = sOffset + sCmpResult + sBlock->sizeOrCnt;
                                    sIsTrue     = ID_TRUE;                                        

                                    IDE_TEST( mtfCheckMatchBlockForLOB( &sBuffer,
                                                                        &sTempOffset,
                                                                        sLobLength,
                                                                        &sTempBlock,
                                                                        sBlockFence,
                                                                        &sIsTempTrue)
                                              != IDE_SUCCESS);                                        
                                        
                                    if ( sIsTempTrue != ID_TRUE )
                                    {
                                        /* �� ��Ī�� Ʋ������ ���� ���������� �̵���Ų��*/
                                        sOffset += (sCmpResult + 1);                                            
                                    }
                                    else
                                    {
                                        sBlock  = sTempBlock;
                                        sOffset = sTempOffset;                                            
                                        break;                                            
                                    }
                                }
                            }
                        }
                        else // __�� ���� �о����� %�̴�.
                        {
                            // nothing to do
                        }
                    }
                    else // __�� ���� �� �о����� �� �̻� ���� �ʾҴ�.
                    {
                        // nothing to do
                    }
                }
                else // ������ %%               
                {                    
                    sBlock++;
                    sOffset = sLobLength;
                }                    
                break;                              
        }
    }

    if( sIsTrue == ID_TRUE )
    {   
        if ( sOffset == sLobLength )
        {
            *aResult = MTD_BOOLEAN_TRUE;            
        }
        else
        {
            *aResult = MTD_BOOLEAN_FALSE;
        }
    }
    else
    {
        *aResult = MTD_BOOLEAN_FALSE;
    }
    
    return IDE_SUCCESS;   
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4XlobLocatorMBOnePass( mtdClobLocatorType  aLocator,
                                              UInt                aLobLength,
                                              mtcLikeBlockInfo  * aBlock,
                                              UInt                aBlockCnt,
                                              mtdBooleanType    * aResult,
                                              UInt                aMaxCharSize,
                                              const mtlModule   * aLanguage,
                                              mtcTemplate       * aTemplate )
{    
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/

    UChar               sBuf[MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION] = {0,};
    mtcLobBuffer        sBuffer;    
    UInt                sLobLength;
    UInt                sOffset = 0;
    UInt                sTempOffset;    
    mtcLikeBlockInfo  * sBlock;
    mtcLikeBlockInfo  * sTempBlock;
    mtcLikeBlockInfo  * sBlockFence;
    mtcLikeBlockInfo  * sBlockStart;
    idBool              sIsTrue = ID_TRUE;
    idBool              sIsTempTrue;    
    UInt                sReadLength;
    SInt                sCmpResult;
    UInt                sCount;
    UInt                sMaxStringLen;
    mtlNCRet            sNcRet = NC_INVALID;
    UChar             * sPrev;
    UChar             * sNow;
    UInt                sValidChar = 0;
    UInt                i;
    UInt                sMount;
    UInt                sSize;    

    sBuffer.locator = aLocator;
    sBuffer.buf     = sBuf;
    sLobLength      = aLobLength;
    sBlockStart     = aBlock;    

    IDE_ASSERT( aBlockCnt != 0 );

    sBlockFence = aBlock + aBlockCnt;    
        
    sBlock = sBlockFence - 1;

    /*    %aaa%bbbb%ccc �� ���� ��� ccc�� ���� �ϴ� �κ��̴� */
        
    while( (sBlock >= sBlockStart ) &&
           (sIsTrue == ID_TRUE) )
    {
        if ( sBlock->type == MTC_FORMAT_BLOCK_PERCENT )
        {
            sBlock++;                
            break;                
        }
        else
        {
            /* �ں��� �˻��Ѵ� */
            if ( sBlock->type == MTC_FORMAT_BLOCK_STRING )
            {
                if ( sLobLength >= sBlock->sizeOrCnt )
                {
                    IDE_TEST( mtc::readLob( mtc::getStatistics(aTemplate), /* idvSQL* */
                                            aLocator,
                                            sLobLength - sBlock->sizeOrCnt,
                                            sBlock->sizeOrCnt,
                                            sBuf,
                                            &sReadLength )
                              != IDE_SUCCESS );
                    
                    if ( idlOS::memcmp( sBlock->start,
                                        sBuf,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;                        
                    }
                    else
                    {
                        sLobLength -= sBlock->sizeOrCnt;
                        sBlock--;
                    }
                }
                else
                {
                    sIsTrue = ID_FALSE;                        
                }                    
            }
            else // MTC_FORMAT_UNDER
            {
                sCount = 0;

                sMaxStringLen = sBlock->sizeOrCnt * aMaxCharSize;

                if ( sOffset + sMaxStringLen < sLobLength )
                {
                    sOffset = sLobLength - sMaxStringLen;                        
                }
                else
                {
                    sOffset = 0;
                    sMaxStringLen = sLobLength;
                }                    
                                        
                IDE_TEST( mtc::readLob( NULL, /* idvSQL* */
                                        aLocator,
                                        sOffset,
                                        sMaxStringLen,
                                        sBuf,
                                        &sReadLength )
                          != IDE_SUCCESS );
                    
                while ( sCount < sBlock->sizeOrCnt )
                {
                    sTempOffset = sMaxStringLen - 1;

                    if ( (SInt)sTempOffset < 0 )
                    {
                        sIsTrue = ID_FALSE;                            
                        break;
                    }

                    while ( (SInt)sTempOffset >= 0 )
                    {
                        sNow = &sBuf[sTempOffset];
                            
                        sNcRet = mtf::nextChar( (UChar *)(&sBuf[sMaxStringLen]),
                                                &sNow,
                                                &sPrev,
                                                aLanguage);

                        if ( sNcRet == NC_VALID )
                        {
                            sMaxStringLen = sTempOffset;                                
                            break;                            
                        }
                        else
                        {
                            sTempOffset--;                                
                        }   
                    }

                    if ( sNcRet == NC_VALID )
                    {
                        sCount++;                        
                    }
                    else
                    {
                        sIsTrue = ID_FALSE;                        
                        break;                        
                    }
                }

                sLobLength = sOffset + sMaxStringLen;
                sBlock--;
            }
        }
    }        

    sBlockFence = sBlock;
    sBlock      = sBlockStart;
    sOffset     = 0;        
            
    while ( (sBlock < sBlockFence) &&
            (sIsTrue == ID_TRUE) )
    {            
        switch ( sBlock->type )
        {
            case MTC_FORMAT_BLOCK_STRING:
                /*  ó�� ���ڿ��̰ų� �Ǵ� �ܼ� ���ϰ�쿡�� �� �� �ִ�.
                    __string���� __�� ó�� �� �ڿ� ���ų�
                    string ���¿��� �ҷ����� */
               
                if ( sOffset + sBlock->sizeOrCnt <= sLobLength )
                {
                    IDE_TEST( mtc::readLob( NULL, /* idvSQL* */
                                            aLocator,
                                            sOffset,
                                            sBlock->sizeOrCnt,
                                            sBuf,
                                            &sReadLength )
                              != IDE_SUCCESS );
                    
                    if ( idlOS::memcmp( sBlock->start,
                                        sBuf,
                                        sBlock->sizeOrCnt ) != 0 )
                    {
                        sIsTrue = ID_FALSE;                        
                    }
                    else
                    {
                        sOffset += sBlock->sizeOrCnt;
                    }

                    sBlock++;
                }
                else
                {
                    sIsTrue = ID_FALSE;                        
                }
                    
                break;

            case MTC_FORMAT_BLOCK_UNDER:
                /* ���� ����ŭ �ڷ� ������.*/

                sMaxStringLen = sBlock->sizeOrCnt * aMaxCharSize;

                if ( sOffset + sMaxStringLen > sLobLength )
                {
                    sMaxStringLen = sLobLength - sOffset;                        
                }
                else
                {
                    // nothing to do
                }

                IDE_TEST( mtc::readLob( NULL, /* idvSQL* */
                                        aLocator,
                                        sOffset,
                                        sMaxStringLen,
                                        sBuf,
                                        &sReadLength )
                          != IDE_SUCCESS );

                sNow = &sBuf[0];
                    
                for ( i = 0 ; i < sBlock->sizeOrCnt; i++)
                {   
                    if ( sNow >= sBuf + sReadLength )
                    {
                        sIsTrue = ID_FALSE;
                        break;                        
                    }
                    else
                    {
                        // nothing to do
                    }
                    
                    if ( mtf::nextChar( (UChar*) (&sBuf[sMaxStringLen]),
                                        &sNow,
                                        &sPrev,
                                        aLanguage )
                         != NC_VALID )
                    {
                        sIsTrue = ID_FALSE;
                        break;                        
                    }
                    else
                    {
                        // nothing to do
                    }

                    sOffset += (sNow - sPrev);                        
                }

                sBlock++;
                    
                break;    
                    
            case MTC_FORMAT_BLOCK_PERCENT:

                if ( sBlock + 1 < sBlockFence )
                {
                    sBlock++;                      
                        
                    if ( sBlock->type == MTC_FORMAT_BLOCK_UNDER )
                    {
                        /*  __ �� �������� ������ ��� �׸�ŭ ��¥�� �δ� */
                        sOffset += sBlock->sizeOrCnt;
                        sBlock++;                            
                    }
                    else
                    {
                        // nothing to do                            
                    }

                    if ( sOffset > sLobLength )
                    {
                        sIsTrue = ID_FALSE;
                        break;                            
                    }
                    else
                    {
                        // nothing to do
                    }
                        
                    if ( sBlock < sBlockFence )
                    {
                        if ( sBlock->type == MTC_FORMAT_BLOCK_STRING )
                        {                       
                            while ( (sOffset < sLobLength) &&
                                    (sIsTrue == ID_TRUE) )
                            {
                                IDE_TEST( matchSubStringForLOB( &sBuffer,
                                                                sOffset,
                                                                sLobLength,                                      
                                                                sBlock->start,
                                                                sBlock->sizeOrCnt,
                                                                &sCmpResult )
                                          != IDE_SUCCESS);                                    

                                if ( sCmpResult == -1 )
                                {
                                    /* �κ� ���ڿ� �˻��� ���� ���� ���� ���̴� */
                                    sIsTrue = ID_FALSE;                                    
                                }
                                else
                                {
                                    /* matchSubString�� ��� Multibyte�� ��� ���� �ʾҴ� ����
                                       �߸��� ������� ���� �� �� �ִ�.
                                       �˻��� ����� ���� ���ڿ��� ����� �� �������̶��
                                       ����� �˻��� �Ѱ��̴�. */
                                    
                                    while ( sValidChar < sOffset + sCmpResult )
                                    {
                                        sTempOffset = 0;
                                            
                                        if ( sValidChar + MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION > sOffset + sCmpResult )
                                        {
                                            sMount = sOffset + sCmpResult - sValidChar;
                                            sSize = sMount;                                                
                                        }
                                        else
                                        {
                                            sMount = MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION;
                                            sSize  = MTC_LOB_BUFFER_SIZE;                                                
                                        }

                                        IDE_TEST( mtc::readLob( NULL, /* idvSQL* */
                                                                aLocator,
                                                                sValidChar,
                                                                sMount,
                                                                sBuf,
                                                                &sReadLength )
                                                  != IDE_SUCCESS );
                                            
                                        sNow = &sBuf[0];                                            

                                        while( sTempOffset < sSize )
                                        {                                            
                                            mtf::nextChar( (UChar*)(&sBuf[sMount]),
                                                           &sNow,
                                                           &sPrev,
                                                           aLanguage);

                                            sValidChar  += sNow - sPrev;
                                            sTempOffset += sNow - sPrev;                                                
                                        }                                                                                 
                                    }

                                    if ( sValidChar == sOffset + sCmpResult )
                                    {
                                        /* �� ������ ��Ī�� �����Ѵ� */
                                        sTempBlock  = sBlock + 1;
                                        sTempOffset = sOffset + sCmpResult + sBlock->sizeOrCnt;
                                        sIsTrue     = ID_TRUE;                                        

                                        IDE_TEST( mtfCheckMatchBlockMBForLOB( &sBuffer,
                                                                              &sTempOffset,
                                                                              sLobLength,
                                                                              &sTempBlock,
                                                                              sBlockFence,
                                                                              aLanguage,
                                                                              &sIsTempTrue)
                                                  != IDE_SUCCESS);                                            
                                        
                                        if ( sIsTempTrue != ID_TRUE )
                                        {
                                            /* �� ��Ī�� Ʋ������ ���� ���������� �̵���Ų��*/
                                            sOffset += sCmpResult;
                                            /* ������ LOB�� �о� nextchar�� �ؾ� �ϴµ� �δ㽺���� +1�� �ߴ�
                                               ���Ŀ� ����Ǵ� MatchSubstring���� readlob�� ���� �Ұ��̰�
                                               ������ +1�� �κ��� �κ� ���ڿ� �˻��� �� Ȯ���� �ſ� ���� */
                                            sOffset++;                                            
                                        }
                                        else
                                        {
                                            sBlock  = sTempBlock;
                                            sOffset = sTempOffset;
                                            break;                                            
                                        }
                                    }
                                    else
                                    {
                                        sOffset = sValidChar;
                                    }                                        
                                }
                            }
                        }
                        else // __�� ���� �о����� %�̴�.
                        {
                            // nothing to do
                        }
                    }
                    else // __�� ���� �� �о����� �� �̻� ���� �ʾҴ�.
                    {
                        // nothing to do
                    }
                }
                else // ������ %%               
                {                    
                    sBlock++;
                    sOffset = sLobLength;
                }                    
                break;
        }
    }

    if( sIsTrue == ID_TRUE )
    {   
        if ( sOffset == sLobLength )
        {
            *aResult = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *aResult = MTD_BOOLEAN_FALSE;
        }
    }
    else
    {
        *aResult = MTD_BOOLEAN_FALSE;
    }
    
    return IDE_SUCCESS;   
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateNormal( mtcNode*     aNode,
                               mtcStack*    aStack,
                               SInt         aRemain,
                               void*        aInfo,
                               mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    const mtdModule * sModule;

    mtdCharType     * sVarchar;
    UChar           * sString;
    UChar           * sStringIntermediate;
    UChar           * sStringFence;
    UChar           * sFormat;
    UChar           * sFormatIntermediate;
    UChar           * sFormatFence;
    UChar             sEscape = 0;
    idBool            sNullEscape;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sVarchar     = (mtdCharType*)aStack[1].value;
        sString      = sVarchar->value;
        sStringFence = sString + sVarchar->length;
        sVarchar     = (mtdCharType*)aStack[2].value;
        sFormat      = sVarchar->value;
        sFormatFence = sFormat + sVarchar->length;
        
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            sEscape = sVarchar->value[0];
            sNullEscape = ID_FALSE;
        }
        else
        {
            sNullEscape = ID_TRUE;
        }
        
        for( ; (sString < sStringFence) && (sFormat < sFormatFence);
             sString++, sFormat++ )
        {
            if( (sNullEscape == ID_FALSE) && (*sFormat == sEscape) )
            {
                sFormat++;
                IDE_TEST_RAISE( sFormat >= sFormatFence,
                                ERR_INVALID_LITERAL );
                IDE_TEST_RAISE( (*sFormat != '%') &&
                                (*sFormat != '_') &&
                                (*sFormat != sEscape), // sEsacpe�� null�� �ƴ�
                                ERR_INVALID_LITERAL );

                if( *sFormat != *sString )
                {
                    // Bug-11942 fix
                    // 'aaab' like 'aaa__' escape '_' ó����
                    // sFormat�� ���� aaa__�� ������ _�ε�
                    // b != '_' �̹Ƿ� break�� ������
                    // sFormat�� �ٽ� ������ ��ĭ ��ܾ� �Ѵ�.
                    sFormat--;
                    break;
                }
            }
            else if( *sFormat != '%' )
            {
                /*
                To Fix BUG-12306
                if( *sFormat != '_' && *sFormat != *sString )
                {
                    break;
                }
                */
                if ( *sFormat != '_' )
                {
                    // �Ϲ� ������ ���
                    if ( *sFormat != *sString )
                    {
                        break;
                    }
                    else
                    {
                        // keep going
                    }
                }
                else
                {
                    // '_'������ ���
                    // keep going
                }
            }
            else // %
            {
                sFormat += 1;
                for( sStringIntermediate = sString,
                         sFormatIntermediate = sFormat;
                     sStringIntermediate < sStringFence; )
                {
                    if( sFormatIntermediate < sFormatFence )
                    {
                        if( (sNullEscape == ID_FALSE) &&
                            (*sFormatIntermediate == sEscape) )
                        {
                            sFormatIntermediate++;
                            IDE_TEST_RAISE( sFormatIntermediate >= sFormatFence,
                                            ERR_INVALID_LITERAL );
                            
                            IDE_TEST_RAISE( (*sFormatIntermediate != '%') && 
                                            (*sFormatIntermediate != '_') &&
                                            (*sFormatIntermediate != sEscape), // sEsacpe�� null�� �ƴ�
                                            ERR_INVALID_LITERAL );
                            
                            if( *sStringIntermediate !=
                                *sFormatIntermediate )
                            {
                                sString++;
                                sStringIntermediate = sString;
                                sFormatIntermediate = sFormat;
                            }
                            else
                            {    
                                sStringIntermediate++;
                                sFormatIntermediate++;
                            }
                        }
                        else if( *sFormatIntermediate != '%' )
                        {
                            if( (*sFormatIntermediate != '_') &&
                                (*sFormatIntermediate != *sStringIntermediate) )
                            {
                                sString++;
                                sStringIntermediate = sString;
                                sFormatIntermediate = sFormat;
                            }
                            else
                            {    
                                sStringIntermediate++;
                                sFormatIntermediate++;
                            }
                        }
                        else // %
                        {
                            sString             = sStringIntermediate;
                            sFormat             = sFormatIntermediate + 1;
                            sStringIntermediate = sString;
                            sFormatIntermediate = sFormat;
                        }
                    }
                    else
                    {
                        sString++;
                        sStringIntermediate = sString;
                        sFormatIntermediate = sFormat;
                    }
                }
                sString = sStringIntermediate;
                sFormat = sFormatIntermediate;
                break;
            }
        }

        if ( (sModule->id == MTD_CHAR_ID) &&
             (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE))
        {
            for( ; sFormat < sFormatFence; sFormat++ )
            {
                if( (sNullEscape == ID_FALSE) && (*sFormat == sEscape) )
                {
                    sFormat++;
                    IDE_TEST_RAISE( sFormat >= sFormatFence, 
                                    ERR_INVALID_LITERAL );

                    IDE_TEST_RAISE( (*sFormat != '%') &&
                                    (*sFormat != '_') &&
                                    (*sFormat != sEscape), // sEsacpe�� null�� �ƴ�
                                    ERR_INVALID_LITERAL );
                    if( *sFormat != ' ' )
                    {
                        // Bug-11942 fix
                        sFormat--;
                        break;
                    }
                }
                else
                {
                    if( (*sFormat != '%') && (*sFormat != ' ') )
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            for( ; sFormat < sFormatFence; sFormat++ )
            {
                if( (sNullEscape == ID_FALSE) && (*sFormat == sEscape) )
                {
                    sFormat++;
                    IDE_TEST_RAISE( sFormat >= sFormatFence,
                                    ERR_INVALID_LITERAL );

                    IDE_TEST_RAISE( (*sFormat != '%') &&
                                    (*sFormat != '_') &&
                                    (*sFormat != sEscape), // sEsacpe�� null�� �ƴ�
                                    ERR_INVALID_LITERAL );

                    // Bug-11942 fix
                    sFormat--;
                    break;
                }
                else if( *sFormat != '%' )
                {
                    break;
                }
            }
        }

        if ( (sModule->id == MTD_CHAR_ID) &&
             (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE))
        {
            for( ; sString < sStringFence; sString++ )
            {
                if( *sString != ' ' )
                {
                    break;
                }
            }
        }
        else
        {
            // nothing to do
        }        
        
        if( (sString >= sStringFence) && (sFormat >= sFormatFence) )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));
    
    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

//fix For BUG-15930  
IDE_RC mtfLikeCalculateMBNormal( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    const mtdModule * sModule;
    const mtlModule * sLanguage;

    mtdCharType     * sVarchar;
    UChar           * sString;
    UChar           * sStringIntermediate;
    UChar           * sStringFence;
    UChar           * sFormat;
    UChar           * sFormatPrev;
    UChar           * sFormatIntermediate;
    UChar           * sFormatFence;
    UChar           * sEscape = NULL;
    idBool            sNullEscape;
    idBool            sEqual;
    idBool            sEqual1;
    idBool            sEqual2;
    idBool            sEqual3;
    UChar             sSize;
    UChar             sSize2;
    UShort            sEscapeLen = 0;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule   = aStack[1].column->module;
    sLanguage = aStack[1].column->language;

    if( (sModule->isNull( aStack[1].column,
                          aStack[1].value ) == ID_TRUE) ||
        (sModule->isNull( aStack[2].column,
                          aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sVarchar     = (mtdCharType*)aStack[1].value;
        sString      = sVarchar->value;
        sStringFence = sString + sVarchar->length;
        sVarchar     = (mtdCharType*)aStack[2].value;
        sFormat      = sVarchar->value;
        sFormatPrev  = sFormat;
        sFormatFence = sFormat + sVarchar->length;
        
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            if( sLanguage->id == MTL_UTF16_ID )
            {
                IDE_TEST_RAISE( sVarchar->length != 2, ERR_INVALID_ESCAPE );
            }
            else
            {
                IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            }
            
            sEscape = sVarchar->value;
            sEscapeLen = sVarchar->length;
            
            sNullEscape = ID_FALSE;
        }
        else
        {
            sNullEscape = ID_TRUE;
        }
        
        for( ; (sString < sStringFence) && (sFormat < sFormatFence); )
        {
            sSize =  mtl::getOneCharSize( sFormat,
                                          sFormatFence,
                                          sLanguage );

            sSize2 =  mtl::getOneCharSize( sString,
                                           sStringFence,
                                           sLanguage );

            if( sNullEscape == ID_FALSE )
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sEscape,
                                              sEscapeLen );
            }
            else
            {
                sEqual = ID_FALSE;
            }
                
            if( sEqual == ID_TRUE ) 
            {
                (void)mtf::nextChar( sFormatFence,
                                     &sFormat,
                                     &sFormatPrev,
                                     sLanguage);

                sSize =  mtl::getOneCharSize( sFormat,
                                              sFormatFence,
                                              sLanguage );

                // escape ������ ���,
                // escape ���� ���ڰ� '%','_' �������� �˻�
                sEqual1 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_PC_IDX],
                                               sLanguage->specialCharSize );
            
                sEqual2 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_UB_IDX],
                                               sLanguage->specialCharSize );
            
                sEqual3 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sEscape,
                                               sEscapeLen );
                
                IDE_TEST_RAISE( (sEqual1 != ID_TRUE) && 
                                (sEqual2 != ID_TRUE) && 
                                (sEqual3 != ID_TRUE), 
                                ERR_INVALID_LITERAL );

                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sString,
                                              sSize2 );
                                
                if( sEqual != ID_TRUE )
                {
                    // Bug-11942 fix
                    // 'aaab' like 'aaa__' escape '_' ó����
                    // sFormat�� ���� aaa__�� ������ _�ε�
                    // b != '_' �̹Ƿ� break�� ������
                    // sFormat�� �ٽ� ������ ��ĭ ��ܾ� �Ѵ�.
                    sFormat = sFormatPrev;
                    break;
                }
            }
            else 
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sLanguage->specialCharSet[MTL_PC_IDX],
                                              sLanguage->specialCharSize );
                            
                if( sEqual != ID_TRUE )
                {
                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sLanguage->specialCharSet[MTL_UB_IDX],
                                                  sLanguage->specialCharSize );
                    
                    if ( sEqual != ID_TRUE )
                    {
                        // �Ϲ� ������ ���
                        sEqual = mtc::compareOneChar( sFormat,
                                                      sSize,
                                                      sString,
                                                      sSize2 );
                        
                        if ( sEqual != ID_TRUE )
                        {
                            break;
                        }
                        else
                        {
                            // keep going
                        }
                    }
                    else
                    {
                        // '_'������ ���
                        // keep going
                    }
                }
                else // %
                {
                    (void)mtf::nextChar( sFormatFence,
                                         &sFormat,
                                         NULL,
                                         sLanguage );
                    
                    for( sStringIntermediate = sString, sFormatIntermediate = sFormat;
                         sStringIntermediate < sStringFence;)
                    {
                        sSize =  mtl::getOneCharSize( sFormatIntermediate,
                                                      sFormatFence,
                                                      sLanguage );

                        sSize2 =  mtl::getOneCharSize( sStringIntermediate,
                                                       sStringFence,
                                                       sLanguage );
                        
                        if( sFormatIntermediate < sFormatFence )
                        {
                            if( sNullEscape == ID_FALSE )
                            {
                                sEqual = mtc::compareOneChar( sFormatIntermediate,
                                                              sSize,
                                                              sEscape,
                                                              sEscapeLen );
                            }
                            else
                            {
                                sEqual = ID_FALSE;
                            }
                            
                            if( sEqual == ID_TRUE )
                            {
                                (void)mtf::nextChar( sFormatFence,
                                                     &sFormatIntermediate,
                                                     NULL,
                                                     sLanguage );
                                
                                sSize =  mtl::getOneCharSize( sFormatIntermediate,
                                                              sFormatFence,
                                                              sLanguage );

                                IDE_TEST_RAISE( sFormatIntermediate >= sFormatFence, ERR_INVALID_LITERAL );

                                sEqual1 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sLanguage->specialCharSet[MTL_PC_IDX],
                                                               sLanguage->specialCharSize );
            
                                sEqual2 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sLanguage->specialCharSet[MTL_UB_IDX],
                                                               sLanguage->specialCharSize );
            
                                sEqual3 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sEscape,
                                                               sEscapeLen );

                                IDE_TEST_RAISE( (sEqual1 != ID_TRUE) && 
                                                (sEqual2 != ID_TRUE) && 
                                                (sEqual3 != ID_TRUE),
                                                ERR_INVALID_LITERAL );

                                sEqual = mtc::compareOneChar( sFormatIntermediate,
                                                              sSize,
                                                              sStringIntermediate,
                                                              sSize2 );
                                
                                if( sEqual != ID_TRUE )
                                {
                                    (void)mtf::nextChar( sStringFence,
                                                         &sString,
                                                         NULL,
                                                         sLanguage );
                                    
                                    sStringIntermediate = sString;
                                    sFormatIntermediate = sFormat;
                                }
                                else
                                {
                                    (void)mtf::nextChar( sStringFence,
                                                         &sStringIntermediate,
                                                         NULL,
                                                         sLanguage );
                                    
                                    (void)mtf::nextChar( sFormatFence,
                                                         &sFormatIntermediate,
                                                         NULL,
                                                         sLanguage );
                                }
                            }
                            else 
                            {
                                sEqual = mtc::compareOneChar( sFormatIntermediate,
                                                              sSize,
                                                              sLanguage->specialCharSet[MTL_PC_IDX],
                                                              sLanguage->specialCharSize );
                                
                                if( sEqual != ID_TRUE )
                                {
                                    sEqual1 = mtc::compareOneChar( sFormatIntermediate,
                                                                   sSize,
                                                                   sLanguage->specialCharSet[MTL_UB_IDX],
                                                                   sLanguage->specialCharSize );
                                    

                                    sEqual2 = mtc::compareOneChar( sFormatIntermediate,
                                                                   sSize,
                                                                   sStringIntermediate,
                                                                   sSize2 );
                                    
                                    if( (sEqual1 != ID_TRUE) && 
                                        (sEqual2 != ID_TRUE) )
                                    {
                                        (void)mtf::nextChar( sStringFence,
                                                             &sString,
                                                             NULL,
                                                             sLanguage );
                                        
                                        sStringIntermediate = sString;
                                        sFormatIntermediate = sFormat;
                                    }
                                    else
                                    {
                                        (void)mtf::nextChar( sStringFence,
                                                             &sStringIntermediate,
                                                             NULL,
                                                             sLanguage );
                                        
                                        (void)mtf::nextChar( sFormatFence,
                                                             &sFormatIntermediate,
                                                             NULL,
                                                             sLanguage );
                                    }
                                }
                                else // %
                                {
                                    sString = sStringIntermediate;
                                    sFormat = sFormatIntermediate;
                                    
                                    (void)mtf::nextChar( sFormatFence,
                                                         &sFormat,
                                                         NULL,
                                                         sLanguage );
                                    
                                    sStringIntermediate = sString;
                                    sFormatIntermediate = sFormat;
                                }
                            }
                        }
                        else
                        {
                            (void)mtf::nextChar( sStringFence,
                                                 &sString,
                                                 NULL,
                                                 sLanguage );
                            
                            sStringIntermediate = sString;
                            sFormatIntermediate = sFormat;
                        }
                    }
                    
                    sString = sStringIntermediate;
                    sFormat = sFormatIntermediate;
                    break;
                }
            }

            (void)mtf::nextChar( sStringFence,
                                 &sString,
                                 NULL,
                                 sLanguage );
            
            (void)mtf::nextChar( sFormatFence,
                                 &sFormat,
                                 NULL,
                                 sLanguage );
        }

        if( ((sModule->id == MTD_CHAR_ID) || (sModule->id == MTD_NCHAR_ID)) &&
            (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE))
        {
            for( ; sFormat < sFormatFence; )
            {
                sSize =  mtl::getOneCharSize( sFormat,
                                              sFormatFence,
                                              sLanguage );
                
                if( sNullEscape == ID_FALSE )
                {
                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sEscape,
                                                  sEscapeLen );
                }
                else
                {
                    sEqual = ID_FALSE;
                }

                if( sEqual == ID_TRUE )
                {
                    (void)mtf::nextChar( sFormatFence,
                                         &sFormat,
                                         &sFormatPrev,
                                         sLanguage );
                    
                    sSize =  mtl::getOneCharSize( sFormat,
                                                  sFormatFence,
                                                  sLanguage );

                    sEqual1 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_PC_IDX],
                                                   sLanguage->specialCharSize );
            
                    sEqual2 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_UB_IDX],
                                                   sLanguage->specialCharSize );
            
                    sEqual3 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sEscape,
                                                   sEscapeLen );
                    
                    IDE_TEST_RAISE( (sEqual1 != ID_TRUE) && 
                                    (sEqual2 != ID_TRUE) && 
                                    (sEqual3 != ID_TRUE),
                                    ERR_INVALID_LITERAL );

                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sLanguage->specialCharSet[MTL_SP_IDX],
                                                  sLanguage->specialCharSize );

                    if( sEqual != ID_TRUE )
                    {
                        // Bug-11942 fix
                        sFormat = sFormatPrev;
                        break;
                    }
                }
                else
                {
                    sEqual1 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_PC_IDX],
                                                   sLanguage->specialCharSize );

                    sEqual2 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_SP_IDX],
                                                   sLanguage->specialCharSize );

                    if( (sEqual1 != ID_TRUE) && 
                        (sEqual2 != ID_TRUE) )
                    {
                        break;
                    }
                }

                (void)mtf::nextChar( sFormatFence,
                                     &sFormat,
                                     NULL,
                                     sLanguage );
            }
        }
        else
        {
            for( ; sFormat < sFormatFence; )
            {
                sSize =  mtl::getOneCharSize( sFormat,
                                              sFormatFence,
                                              sLanguage );
                
                if( sNullEscape == ID_FALSE )
                {
                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sEscape,
                                                  sEscapeLen );
                }
                else
                {
                    sEqual = ID_FALSE;
                }

                if( sEqual == ID_TRUE )
                {
                    (void)mtf::nextChar( sFormatFence,
                                         &sFormat,
                                         &sFormatPrev,
                                         sLanguage );
                    
                    sSize =  mtl::getOneCharSize( sFormat,
                                                  sFormatFence,
                                                  sLanguage );

                    sEqual1 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_PC_IDX],
                                                   sLanguage->specialCharSize );
            
                    sEqual2 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_UB_IDX],
                                                   sLanguage->specialCharSize );
            
                    sEqual3 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sEscape,
                                                   sEscapeLen );

                    IDE_TEST_RAISE( (sEqual1 != ID_TRUE) &&
                                    (sEqual2 != ID_TRUE) &&
                                    (sEqual3 != ID_TRUE), 
                                    ERR_INVALID_LITERAL );

                    // Bug-11942 fix
                    sFormat = sFormatPrev;
                    break;
                }
                else 
                {
                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sLanguage->specialCharSet[MTL_PC_IDX],
                                                  sLanguage->specialCharSize );

                    if( sEqual != ID_TRUE )
                    {
                        break;
                    }
                }

                (void)mtf::nextChar( sFormatFence,
                                     &sFormat,
                                     NULL,
                                     sLanguage );
            }
        }
        
        if( ((sModule->id == MTD_CHAR_ID) || (sModule->id == MTD_NCHAR_ID)) &&
            (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE))
        {
            for( ; sString < sStringFence; )
            {
                sSize2 =  mtl::getOneCharSize( sString,
                                               sStringFence,
                                               sLanguage );

                sEqual = mtc::compareOneChar( sString,
                                              sSize2,
                                              sLanguage->specialCharSet[MTL_SP_IDX],
                                              sLanguage->specialCharSize );
                
                if( sEqual != ID_TRUE )
                {
                    break;
                }

                (void)mtf::nextChar( sStringFence,
                                     &sString,
                                     NULL,
                                     sLanguage );
            }
        }
        else
        {
            // nothing to do
        }
        
        if( (sString >= sStringFence) && (sFormat >= sFormatFence) )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));
    
    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
} 

IDE_RC mtfLikeCalculate4XlobLocatorNormal( mtcNode*     aNode,
                                           mtcStack*    aStack,
                                           SInt         aRemain,
                                           void*        aInfo,
                                           mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/

    mtdCharType       * sVarchar;
    mtcLobCursor        sString;
    mtcLobCursor        sStringIntermediate;
    UInt                sStringFence;
    UChar             * sFormat;
    UChar             * sFormatIntermediate;
    UChar             * sFormatFence;
    UChar               sEscape  = 0;
    idBool              sNullEscape;
    idBool              sEqual   = ID_TRUE;
    UChar               sBuf[MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION] = {0,};
    mtcLobBuffer        sBuffer;    
    mtdClobLocatorType  sLocator = MTD_LOCATOR_NULL;
    SLong               sLobLength;
    idBool              sIsNullLob;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLocator = *(mtdClobLocatorType*)aStack[1].value;

    // BUG-48005 Partition Table���� select /*+ use_one_pass_hash() */ ���� error�� ���ϴ�.
    if ( sLocator == MTD_LOCATOR_NULL )
    {
        sIsNullLob = ID_TRUE;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthWithLocator( mtc::getStatistics(aTemplate),
                                                sLocator,
                                                & sLobLength,
                                                & sIsNullLob )
                  != IDE_SUCCESS );
    }

    if( (sIsNullLob == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sVarchar     = (mtdCharType*)aStack[2].value;
        sFormat      = sVarchar->value;
        sFormatFence = sFormat + sVarchar->length;

        // escape ����
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            sEscape = sVarchar->value[0];
            sNullEscape = ID_FALSE;
        }
        else
        {
            sNullEscape = ID_TRUE;
        }

        // buffer �ʱ�ȭ
        sBuffer.locator = sLocator;
        sBuffer.length  = sLobLength;
        sBuffer.buf     = sBuf;
        sBuffer.offset  = 0;
        sBuffer.fence   = NULL;
        sBuffer.size    = 0;

        // ���۸� �д´�.
        IDE_TEST( mtfLikeReadLob( &sBuffer, aTemplate ) != IDE_SUCCESS );

        sString.offset = sBuffer.offset;
        sString.index = sBuffer.buf;

        sStringFence = sLobLength;

        for( ; (sString.offset + (sString.index - sBuffer.buf) < sStringFence) &&
                 (sFormat < sFormatFence); )
        {
            if( (sNullEscape == ID_FALSE) && (*sFormat == sEscape) )
            {
                sFormat++;
                IDE_TEST_RAISE( sFormat >= sFormatFence,
                                ERR_INVALID_LITERAL );
                IDE_TEST_RAISE( (*sFormat != '%') &&
                                (*sFormat != '_') &&
                                (*sFormat != sEscape), // sEsacpe�� null�� �ƴ�
                                ERR_INVALID_LITERAL );

                // compare
                IDE_TEST( compareCharWithBuffer( &sBuffer,
                                                 &sString,
                                                 sFormat,
                                                 &sEqual,
                                                 aTemplate )
                          != IDE_SUCCESS );

                if( sEqual != ID_TRUE )
                {
                    // Bug-11942 fix
                    // 'aaab' like 'aaa__' escape '_' ó����
                    // sFormat�� ���� aaa__�� ������ _�ε�
                    // b != '_' �̹Ƿ� break�� ������
                    // sFormat�� �ٽ� ������ ��ĭ ��ܾ� �Ѵ�.
                    sFormat--;
                    break;
                }
            }
            else if( *sFormat != '%' )
            {
                /*
                To Fix BUG-12306
                if( *sFormat != '_' && *sFormat != *sString )
                {
                    break;
                }
                */
                if ( *sFormat != '_' )
                {
                    // �Ϲ� ������ ���
                    // compare
                    IDE_TEST( compareCharWithBuffer( &sBuffer,
                                                     &sString,
                                                     sFormat,
                                                     &sEqual,
                                                     aTemplate )
                              != IDE_SUCCESS );

                    if ( sEqual != ID_TRUE )
                    {
                        break;
                    }
                    else
                    {
                        // keep going
                    }
                }
                else
                {
                    // '_'������ ���
                    // keep going
                }
            }
            else // %
            {
                sFormat += 1;

                sStringIntermediate.offset = sString.offset;
                sStringIntermediate.index = sString.index;

                for( sFormatIntermediate = sFormat;
                     sStringIntermediate.offset + (sStringIntermediate.index - sBuffer.buf) < sStringFence; )
                {
                    if( sFormatIntermediate < sFormatFence )
                    {
                        if( (sNullEscape == ID_FALSE) &&
                            (*sFormatIntermediate == sEscape) )
                        {
                            sFormatIntermediate++;
                            IDE_TEST_RAISE( sFormatIntermediate >= sFormatFence,
                                            ERR_INVALID_LITERAL );

                            IDE_TEST_RAISE( (*sFormatIntermediate != '%') &&
                                            (*sFormatIntermediate != '_') &&
                                            (*sFormatIntermediate != sEscape), // sEsacpe�� null�� �ƴ�
                                            ERR_INVALID_LITERAL );

                            // compare
                            IDE_TEST( compareCharWithBuffer( &sBuffer,
                                                             &sStringIntermediate,
                                                             sFormatIntermediate,
                                                             &sEqual,
                                                             aTemplate )
                                      != IDE_SUCCESS );

                            if( sEqual != ID_TRUE )
                            {
                                // move next
                                IDE_TEST( nextCharWithBuffer( &sBuffer,
                                                              &sString,
                                                              NULL,
                                                              aTemplate )
                                          != IDE_SUCCESS );

                                sStringIntermediate.offset = sString.offset;
                                sStringIntermediate.index = sString.index;
                                
                                sFormatIntermediate = sFormat;
                            }
                            else
                            {
                                // move next
                                IDE_TEST( nextCharWithBuffer( &sBuffer,
                                                              &sStringIntermediate,
                                                              NULL,
                                                              aTemplate )
                                          != IDE_SUCCESS );
                                
                                sFormatIntermediate++;
                            }
                        }
                        else if( *sFormatIntermediate != '%' )
                        {
                            // compare
                            IDE_TEST( compareCharWithBuffer( &sBuffer,
                                                             &sStringIntermediate,
                                                             sFormatIntermediate,
                                                             &sEqual,
                                                             aTemplate )
                                      != IDE_SUCCESS );
                            
                            if( (*sFormatIntermediate != '_') &&
                                (sEqual != ID_TRUE) )
                            {
                                // move next
                                IDE_TEST( nextCharWithBuffer( &sBuffer,
                                                              &sString,
                                                              NULL,
                                                              aTemplate )
                                          != IDE_SUCCESS );
                                
                                sStringIntermediate.offset = sString.offset;
                                sStringIntermediate.index = sString.index;
                                
                                sFormatIntermediate = sFormat;
                            }
                            else
                            {
                                // move next
                                IDE_TEST( nextCharWithBuffer( &sBuffer,
                                                              &sStringIntermediate,
                                                              NULL,
                                                              aTemplate )
                                          != IDE_SUCCESS );
                                
                                sFormatIntermediate++;
                            }
                        }
                        else // %
                        {
                            sString.offset = sStringIntermediate.offset;
                            sString.index = sStringIntermediate.index;
                            
                            sFormat = sFormatIntermediate + 1;
                            
                            sStringIntermediate.offset = sString.offset;
                            sStringIntermediate.index = sString.index;
                            
                            sFormatIntermediate = sFormat;
                        }
                    }
                    else
                    {
                        // move next
                        IDE_TEST( nextCharWithBuffer( &sBuffer,
                                                      &sString,
                                                      NULL,
                                                      aTemplate )
                                  != IDE_SUCCESS );

                        sStringIntermediate.offset = sString.offset;
                        sStringIntermediate.index = sString.index;
                            
                        sFormatIntermediate = sFormat;
                    }
                }
                    
                sString.offset = sStringIntermediate.offset;
                sString.index = sStringIntermediate.index;
                    
                sFormat = sFormatIntermediate;
                break;
            }

            // move next
            IDE_TEST( nextCharWithBuffer( &sBuffer,
                                          &sString,
                                          NULL,
                                          aTemplate )
                      != IDE_SUCCESS );

            sFormat++;
        }
        
        for( ; sFormat < sFormatFence; sFormat++ )
        {
            if( (sNullEscape == ID_FALSE) && (*sFormat == sEscape) )
            {
                sFormat++;
                IDE_TEST_RAISE( sFormat >= sFormatFence,
                                ERR_INVALID_LITERAL );
                
                IDE_TEST_RAISE( (*sFormat != '%') &&
                                (*sFormat != '_') &&
                                (*sFormat != sEscape), // sEsacpe�� null�� �ƴ�
                                ERR_INVALID_LITERAL );
                
                // Bug-11942 fix
                sFormat--;
                break;
            }
            else if( *sFormat != '%' )
            {
                break;
            }
        }        
        
        if( (sString.offset + (sString.index - sBuffer.buf) >= sStringFence) &&
            (sFormat >= sFormatFence) )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE));

    IDE_EXCEPTION_END;

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4XlobLocatorMBNormal( mtcNode*     aNode,
                                             mtcStack*    aStack,
                                             SInt         aRemain,
                                             void*        aInfo,
                                             mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    const mtlModule   * sLanguage;
    mtdCharType       * sVarchar;
    mtcLobCursor        sString;
    mtcLobCursor        sStringIntermediate;
    UInt                sStringFence;
    UChar             * sFormat;
    UChar             * sFormatPrev;
    UChar             * sFormatIntermediate;
    UChar             * sFormatFence;
    UChar             * sEscape = NULL;
    idBool              sNullEscape;
    idBool              sEqual;
    idBool              sEqual1  = ID_TRUE;
    idBool              sEqual2  = ID_TRUE;
    idBool              sEqual3  = ID_TRUE;
    UChar               sBuf[MTC_LOB_BUFFER_SIZE + MTL_MAX_PRECISION];
    mtcLobBuffer        sBuffer;    
    mtdClobLocatorType  sLocator = MTD_LOCATOR_NULL;
    SLong               sLobLength;
    UChar               sSize;
    UShort              sEscapeLen = 0;
    idBool              sIsNullLob;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLocator = *(mtdClobLocatorType*)aStack[1].value;

    // BUG-48005 Partition Table���� select /*+ use_one_pass_hash() */ ���� error�� ���ϴ�.
    if ( sLocator == MTD_LOCATOR_NULL )
    {
        sIsNullLob = ID_TRUE;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthWithLocator( mtc::getStatistics(aTemplate),
                                                sLocator,
                                                & sLobLength,
                                                & sIsNullLob )
                  != IDE_SUCCESS );
    }

    if( (sIsNullLob == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sLanguage    = aStack[0].column->language;
        sVarchar     = (mtdCharType*)aStack[2].value;
        sFormat      = sVarchar->value;
        sFormatPrev  = sFormat;
        sFormatFence = sFormat + sVarchar->length;

        // escape ����
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            sEscape = sVarchar->value;
            sEscapeLen = sVarchar->length;
            
            sNullEscape = ID_FALSE;
        }
        else
        {
            sNullEscape = ID_TRUE;
        }

        // buffer �ʱ�ȭ
        sBuffer.locator = sLocator;
        sBuffer.length  = sLobLength;
        sBuffer.buf     = sBuf;
        sBuffer.offset  = 0;
        sBuffer.fence   = NULL;
        sBuffer.size    = 0;

        // ���۸� �д´�.
        IDE_TEST( mtfLikeReadLob( &sBuffer, aTemplate ) != IDE_SUCCESS );

        sString.offset = sBuffer.offset;
        sString.index = sBuffer.buf;
        
        sStringFence = sLobLength;
        
        for( ; (sString.offset + (sString.index - sBuffer.buf) < sStringFence) &&
                 (sFormat < sFormatFence); )
        {
            sSize =  mtl::getOneCharSize( sFormat,
                                          sFormatFence,
                                          sLanguage );

            if( sNullEscape == ID_FALSE )
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sEscape,
                                              sEscapeLen );
            }
            else
            {
                sEqual = ID_FALSE;
            }
            
            if( sEqual == ID_TRUE )  // _
            {
                (void)mtf::nextChar( sFormatFence,
                                     &sFormat,
                                     &sFormatPrev,
                                     sLanguage);
                
                sSize =  mtl::getOneCharSize( sFormat,
                                              sFormatFence,
                                              sLanguage );

                sEqual1 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_PC_IDX],
                                               sLanguage->specialCharSize );
            
                sEqual2 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_UB_IDX],
                                               sLanguage->specialCharSize );
            
                sEqual3 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sEscape,
                                               sEscapeLen );
                
                IDE_TEST_RAISE( (sEqual1 != ID_TRUE) && 
                                (sEqual2 != ID_TRUE) && 
                                (sEqual3 != ID_TRUE), 
                                ERR_INVALID_LITERAL );

                // compare
                IDE_TEST( compareCharWithBufferMB( &sBuffer,
                                                   &sString,
                                                   sFormat,
                                                   sSize,
                                                   &sEqual,
                                                   sLanguage,
                                                   aTemplate )
                          != IDE_SUCCESS );
                
                if( sEqual != ID_TRUE )
                {
                    // Bug-11942 fix
                    // 'aaab' like 'aaa__' escape '_' ó����
                    // sFormat�� ���� aaa__�� ������ _�ε�
                    // b != '_' �̹Ƿ� break�� ������
                    // sFormat�� �ٽ� ������ ��ĭ ��ܾ� �Ѵ�.
                    sFormat = sFormatPrev;
                    break;
                }
            }
            else 
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sLanguage->specialCharSet[MTL_PC_IDX],
                                              sLanguage->specialCharSize );
                
                if( sEqual != ID_TRUE )  // normal char
                {
                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sLanguage->specialCharSet[MTL_UB_IDX],
                                                  sLanguage->specialCharSize );
                    
                    if ( sEqual != ID_TRUE )  // normal char
                    {
                        // �Ϲ� ������ ���
                        // compare
                        IDE_TEST( compareCharWithBufferMB( &sBuffer,
                                                           &sString,
                                                           sFormat,
                                                           sSize,
                                                           &sEqual,
                                                           sLanguage,
                                                           aTemplate )
                                  != IDE_SUCCESS );
                        
                        if ( sEqual != ID_TRUE )
                        {
                            break;
                        }
                        else
                        {
                            // keep going
                        }
                    }
                    else
                    {
                        // '_'������ ���
                        // keep going
                    }
                }
                else // %
                {
                    (void)mtf::nextChar( sFormatFence,
                                         &sFormat,
                                         NULL,
                                         sLanguage );
                    
                    sStringIntermediate.offset = sString.offset;
                    sStringIntermediate.index = sString.index;
                    
                    for( sFormatIntermediate = sFormat;
                         sStringIntermediate.offset + (sStringIntermediate.index - sBuffer.buf) < sStringFence; )
                    {
                        sSize =  mtl::getOneCharSize( sFormatIntermediate,
                                                      sFormatFence,
                                                      sLanguage );
                        
                        if( sFormatIntermediate < sFormatFence )
                        {
                            if( sNullEscape == ID_FALSE )
                            {
                                sEqual = mtc::compareOneChar( sFormatIntermediate,
                                                              sSize,
                                                              sEscape,
                                                              sEscapeLen );
                            }
                            else
                            {
                                sEqual = ID_FALSE;
                            }
                            
                            if( sEqual == ID_TRUE )
                            {
                                (void)mtf::nextChar( sFormatFence,
                                                     &sFormatIntermediate,
                                                     NULL,
                                                     sLanguage );
                                
                                IDE_TEST_RAISE( sFormatIntermediate >= sFormatFence, ERR_INVALID_LITERAL );

                                sSize =  mtl::getOneCharSize( sFormatIntermediate,
                                                              sFormatFence,
                                                              sLanguage );

                                sEqual1 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sLanguage->specialCharSet[MTL_PC_IDX],
                                                               sLanguage->specialCharSize );
            
                                sEqual2 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sLanguage->specialCharSet[MTL_UB_IDX],
                                                               sLanguage->specialCharSize );
            
                                sEqual3 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sEscape,
                                                               sEscapeLen );

                                IDE_TEST_RAISE( (sEqual1 != ID_TRUE) && 
                                                (sEqual2 != ID_TRUE) && 
                                                (sEqual3 != ID_TRUE),
                                                ERR_INVALID_LITERAL );

                                // compare
                                IDE_TEST( compareCharWithBufferMB( &sBuffer,
                                                                   &sStringIntermediate,
                                                                   sFormatIntermediate,
                                                                   sSize,
                                                                   &sEqual,
                                                                   sLanguage,
                                                                   aTemplate )
                                          != IDE_SUCCESS );

                                if( sEqual != ID_TRUE )
                                {
                                    // move next
                                    IDE_TEST( nextCharWithBufferMB( &sBuffer,
                                                                    &sString,
                                                                    NULL,
                                                                    sLanguage,
                                                                    aTemplate )
                                              != IDE_SUCCESS );
                                    
                                    sStringIntermediate.offset = sString.offset;
                                    sStringIntermediate.index = sString.index;
                                    
                                    sFormatIntermediate = sFormat;
                                }
                                else
                                {
                                    // move next
                                    IDE_TEST( nextCharWithBufferMB( &sBuffer,
                                                                    &sStringIntermediate,
                                                                    NULL,
                                                                    sLanguage,
                                                                    aTemplate )
                                              != IDE_SUCCESS );
                                    
                                    (void)mtf::nextChar( sFormatFence,
                                                         &sFormatIntermediate,
                                                         NULL,
                                                         sLanguage );
                                }
                            }
                            else // not escape
                            {
                                sEqual = mtc::compareOneChar( sFormatIntermediate,
                                                              sSize,
                                                              sLanguage->specialCharSet[MTL_PC_IDX],
                                                              sLanguage->specialCharSize );
                                
                                if( sEqual != ID_TRUE )
                                {
                                    sEqual1 = mtc::compareOneChar( sFormatIntermediate,
                                                                   sSize,
                                                                   sLanguage->specialCharSet[MTL_UB_IDX],
                                                                   sLanguage->specialCharSize );
                                    
                                    // compare
                                    IDE_TEST( compareCharWithBufferMB( &sBuffer,
                                                                       &sStringIntermediate,
                                                                       sFormatIntermediate,
                                                                       sSize,
                                                                       &sEqual2,
                                                                       sLanguage,
                                                                       aTemplate )
                                              != IDE_SUCCESS );
                                    
                                    if( (sEqual1 != ID_TRUE) && 
                                        (sEqual2 != ID_TRUE) )
                                    {
                                        // move next
                                        IDE_TEST( nextCharWithBufferMB( &sBuffer,
                                                                        &sString,
                                                                        NULL,
                                                                        sLanguage,
                                                                        aTemplate )
                                                  != IDE_SUCCESS );
                                        
                                        sStringIntermediate.offset = sString.offset;
                                        sStringIntermediate.index = sString.index;
                                        
                                        sFormatIntermediate = sFormat;
                                    }
                                    else
                                    {
                                        // move next
                                        IDE_TEST( nextCharWithBufferMB( &sBuffer,
                                                                        &sStringIntermediate,
                                                                        NULL,
                                                                        sLanguage,
                                                                        aTemplate )
                                                  != IDE_SUCCESS );
                                        
                                        (void)mtf::nextChar( sFormatFence,
                                                             &sFormatIntermediate,
                                                             NULL,
                                                             sLanguage );
                                    }
                                }
                                else // %
                                {
                                    sString.offset = sStringIntermediate.offset;
                                    sString.index = sStringIntermediate.index;
                                    
                                    sFormat = sFormatIntermediate;
                                    
                                    (void)mtf::nextChar( sFormatFence,
                                                         &sFormat,
                                                         NULL,
                                                         sLanguage );
                                    
                                    sStringIntermediate.offset = sString.offset;
                                    sStringIntermediate.index = sString.index;
                                    
                                    sFormatIntermediate = sFormat;
                                }
                            }
                        }
                        else
                        {
                            // move next
                            IDE_TEST( nextCharWithBufferMB( &sBuffer,
                                                            &sString,
                                                            NULL,
                                                            sLanguage,
                                                            aTemplate )
                                      != IDE_SUCCESS );

                            sStringIntermediate.offset = sString.offset;
                            sStringIntermediate.index = sString.index;
                            
                            sFormatIntermediate = sFormat;
                        }
                    }
                    
                    sString.offset = sStringIntermediate.offset;
                    sString.index = sStringIntermediate.index;
                    
                    sFormat = sFormatIntermediate;
                    break;
                }
            }

            // move next
            IDE_TEST( nextCharWithBufferMB( &sBuffer,
                                            &sString,
                                            NULL,
                                            sLanguage,
                                            aTemplate )
                      != IDE_SUCCESS );

            (void)mtf::nextChar( sFormatFence,
                                 &sFormat,
                                 NULL,
                                 sLanguage );
        }

        for( ; sFormat < sFormatFence; )
        {
            sSize =  mtl::getOneCharSize( sFormat,
                                          sFormatFence,
                                          sLanguage );
                
            if( sNullEscape == ID_FALSE )
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sEscape,
                                              sEscapeLen );
            }
            else
            {
                sEqual = ID_FALSE;
            }
            
            if( sEqual == ID_TRUE )
            {
                (void)mtf::nextChar( sFormatFence,
                                     &sFormat,
                                     &sFormatPrev,
                                     sLanguage );
                
                sSize =  mtl::getOneCharSize( sFormat,
                                              sFormatFence,
                                              sLanguage );

                sEqual1 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_PC_IDX],
                                               sLanguage->specialCharSize );
            
                sEqual2 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_UB_IDX],
                                               sLanguage->specialCharSize );
            
                sEqual3 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sEscape,
                                               sEscapeLen );
                
                IDE_TEST_RAISE( (sEqual1 != ID_TRUE) &&
                                (sEqual2 != ID_TRUE) &&
                                (sEqual3 != ID_TRUE), 
                                ERR_INVALID_LITERAL );
                
                // Bug-11942 fix
                sFormat = sFormatPrev;
                break;
            }
            else 
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sLanguage->specialCharSet[MTL_PC_IDX],
                                              sLanguage->specialCharSize );
            
                if( sEqual != ID_TRUE )
                {
                    break;
                }
            }
            
            (void)mtf::nextChar( sFormatFence,
                                 &sFormat,
                                 NULL,
                                 sLanguage );
        }
        
        if( (sString.offset + (sString.index - sBuffer.buf) >= sStringFence) &&
            (sFormat >= sFormatFence) )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        IDE_TEST( aTemplate->closeLobLocator( sLocator )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE));

    IDE_EXCEPTION_END;

    if ( sLocator != MTD_LOCATOR_NULL )
    {
        (void) aTemplate->closeLobLocator( sLocator );
    }

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4EcharNormal( mtcNode*     aNode,
                                     mtcStack*    aStack,
                                     SInt         aRemain,
                                     void*        aInfo,
                                     mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    const mtdModule * sModule;

    mtdEcharType    * sEchar;
    mtdCharType     * sVarchar;
    UChar           * sString;
    UChar           * sStringIntermediate;
    UChar           * sStringFence;
    UChar           * sFormat;
    UChar           * sFormatIntermediate;
    UChar           * sFormatFence;
    UChar             sEscape = '\0';
    idBool            sNullEscape;

    mtcNode         * sEncNode;
    mtcColumn       * sEncColumn;
    mtcEncryptInfo    sDecryptInfo;
    UShort            sStringPlainLength;
    UChar           * sStringPlain;
    UChar             sStringDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    UShort            sFormatPlainLength;
    UChar           * sFormatPlain;
    UChar             sFormatDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule = aStack[1].column->module;
    
    if( ( sModule->isNull( aStack[1].column,
                           aStack[1].value ) == ID_TRUE ) ||
        ( sModule->isNull( aStack[2].column,
                           aStack[2].value ) == ID_TRUE ) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        //--------------------------------------------------
        // search string�� plain text�� ��´�.
        //--------------------------------------------------

        sEchar   = (mtdEcharType*)aStack[1].value;
        
        sEncNode = aNode->arguments;
        sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
            + sEncNode->baseColumn;
        
        if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
        {
            sStringPlain = sStringDecryptedBuf;
            
            if( sEchar->mCipherLength > 0 )
            {
                IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                     sEncNode->baseTable,
                                                     sEncNode->baseColumn,
                                                     & sDecryptInfo )
                          != IDE_SUCCESS );
                
                IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                              sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                              sEchar->mValue,
                                              sEchar->mCipherLength,
                                              sStringPlain,
                                              & sStringPlainLength )
                          != IDE_SUCCESS );
                
                IDE_ASSERT_MSG( sStringPlainLength <= sEncColumn->precision,
                                "sStringPlainLength : %"ID_UINT32_FMT"\n"
                                "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                sStringPlainLength, sEncColumn->precision );
            }
            else
            {
                sStringPlainLength = 0;
            }
        }
        else
        {
            sStringPlain = sEchar->mValue;
            sStringPlainLength = sEchar->mCipherLength;
        }
        
        sString      = sStringPlain;
        sStringFence = sStringPlain + sStringPlainLength;
        
        //--------------------------------------------------
        // format string�� plain text�� ��´�.
        //--------------------------------------------------
        
        sEchar = (mtdEcharType*)aStack[2].value;

        sEncNode = sEncNode->next;

        // TASK-3876 codesonar
        IDE_TEST_RAISE( sEncNode == NULL,
                        ERR_INVALID_FUNCTION_ARGUMENT );

        if ( sEncNode->conversion == NULL )
        {
            // conversion�� null�� ��� default policy�� �ƴ�
            // �÷��� format string�� ���� ��찡 �ִ�.
            sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
                + sEncNode->baseColumn;
        
            if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
            {
                sFormatPlain = sFormatDecryptedBuf;
                
                if( sEchar->mCipherLength > 0 )
                {
                    IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                         sEncNode->baseTable,
                                                         sEncNode->baseColumn,
                                                         & sDecryptInfo )
                              != IDE_SUCCESS );
                    
                    IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                                  sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                                  sEchar->mValue,
                                                  sEchar->mCipherLength,
                                                  sFormatPlain,
                                                  & sFormatPlainLength )
                              != IDE_SUCCESS );
                    
                    IDE_ASSERT_MSG( sFormatPlainLength <= sEncColumn->precision,
                                    "sFormatPlainLength : %"ID_UINT32_FMT"\n"
                                    "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                    sFormatPlainLength, sEncColumn->precision );
                }
                else
                {
                    sFormatPlainLength = 0;
                }
            }
            else
            {
                sFormatPlain = sEchar->mValue;
                sFormatPlainLength = sEchar->mCipherLength;
            }
        }
        else
        {
            // conversion�� �ִ� ��� conversion�� �׻�
            // default policy�� ��ȯ�ȴ�.
            sFormatPlain = sEchar->mValue;
            sFormatPlainLength = sEchar->mCipherLength;
        }
        
        sFormat      = sFormatPlain;
        sFormatFence = sFormatPlain + sFormatPlainLength;
        
        //--------------------------------------------------
        // escape ����
        //--------------------------------------------------
        
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            sEscape = sVarchar->value[0];
            sNullEscape = ID_FALSE;
        }
        else
        {
            sNullEscape = ID_TRUE;
        }
        
        //--------------------------------------------------
        // like ����
        //--------------------------------------------------
        
        for( ; (sString < sStringFence) && (sFormat < sFormatFence);
             sString++, sFormat++ )
        {
            if( (sNullEscape == ID_FALSE) && (*sFormat == sEscape) )
            {
                sFormat++;
                IDE_TEST_RAISE( sFormat >= sFormatFence,
                                ERR_INVALID_LITERAL );
                IDE_TEST_RAISE( (*sFormat != '%') &&
                                (*sFormat != '_') &&
                                (*sFormat != sEscape), // sEsacpe�� null�� �ƴ�
                                ERR_INVALID_LITERAL );

                if( *sFormat != *sString )
                {
                    // Bug-11942 fix
                    // 'aaab' like 'aaa__' escape '_' ó����
                    // sFormat�� ���� aaa__�� ������ _�ε�
                    // b != '_' �̹Ƿ� break�� ������
                    // sFormat�� �ٽ� ������ ��ĭ ��ܾ� �Ѵ�.
                    sFormat--;
                    break;
                }
            }
            else if( *sFormat != '%' )
            {
                /*
                  To Fix BUG-12306
                  if( *sFormat != '_' && *sFormat != *sString )
                  {
                  break;
                  }
                */
                if ( *sFormat != '_' )
                {
                    // �Ϲ� ������ ���
                    if ( *sFormat != *sString )
                    {
                        break;
                    }
                    else
                    {
                        // keep going
                    }
                }
                else
                {
                    // '_'������ ���
                    // keep going
                }
            }
            else // %
            {
                sFormat += 1;
                for( sStringIntermediate = sString,
                         sFormatIntermediate = sFormat;
                     sStringIntermediate < sStringFence; )
                {
                    if( sFormatIntermediate < sFormatFence )
                    {
                        if( (sNullEscape == ID_FALSE) &&
                            (*sFormatIntermediate == sEscape) )
                        {
                            sFormatIntermediate++;
                            IDE_TEST_RAISE( sFormatIntermediate >= sFormatFence,
                                            ERR_INVALID_LITERAL );
                            
                            IDE_TEST_RAISE( (*sFormatIntermediate != '%') && 
                                            (*sFormatIntermediate != '_') &&
                                            (*sFormatIntermediate != sEscape), // sEsacpe�� null�� �ƴ�
                                            ERR_INVALID_LITERAL );
                            
                            if( *sStringIntermediate !=
                                *sFormatIntermediate )
                            {
                                sString++;
                                sStringIntermediate = sString;
                                sFormatIntermediate = sFormat;
                            }
                            else
                            {    
                                sStringIntermediate++;
                                sFormatIntermediate++;
                            }
                        }
                        else if( *sFormatIntermediate != '%' )
                        {
                            if( (*sFormatIntermediate != '_') &&
                                (*sFormatIntermediate != *sStringIntermediate) )
                            {
                                sString++;
                                sStringIntermediate = sString;
                                sFormatIntermediate = sFormat;
                            }
                            else
                            {    
                                sStringIntermediate++;
                                sFormatIntermediate++;
                            }
                        }
                        else // %
                        {
                            sString             = sStringIntermediate;
                            sFormat             = sFormatIntermediate + 1;
                            sStringIntermediate = sString;
                            sFormatIntermediate = sFormat;
                        }
                    }
                    else
                    {
                        sString++;
                        sStringIntermediate = sString;
                        sFormatIntermediate = sFormat;
                    }
                }
                sString = sStringIntermediate;
                sFormat = sFormatIntermediate;
                break;
            }
        }

        if ( (sModule->id == MTD_ECHAR_ID) &&
             (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE))
        {
            for( ; sFormat < sFormatFence; sFormat++ )
            {
                if( (sNullEscape == ID_FALSE) && (*sFormat == sEscape) )
                {
                    sFormat++;
                    IDE_TEST_RAISE( sFormat >= sFormatFence, 
                                    ERR_INVALID_LITERAL );

                    IDE_TEST_RAISE( (*sFormat != '%') &&
                                    (*sFormat != '_') &&
                                    (*sFormat != sEscape), // sEsacpe�� null�� �ƴ�
                                    ERR_INVALID_LITERAL );
                    if( *sFormat != ' ' )
                    {
                        // Bug-11942 fix
                        sFormat--;
                        break;
                    }
                }
                else
                {
                    if( (*sFormat != '%') && (*sFormat != ' ') )
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            for( ; sFormat < sFormatFence; sFormat++ )
            {
                if( (sNullEscape == ID_FALSE) && (*sFormat == sEscape) )
                {
                    sFormat++;
                    IDE_TEST_RAISE( sFormat >= sFormatFence,
                                    ERR_INVALID_LITERAL );

                    IDE_TEST_RAISE( (*sFormat != '%') &&
                                    (*sFormat != '_') &&
                                    (*sFormat != sEscape), // sEsacpe�� null�� �ƴ�
                                    ERR_INVALID_LITERAL );

                    // Bug-11942 fix
                    sFormat--;
                    break;
                }
                else if( *sFormat != '%' )
                {
                    break;
                }
            }
        }
        
        if ( (sModule->id == MTD_ECHAR_ID) && (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE))
        {
            for( ; sString < sStringFence; sString++ )
            {
                if( *sString != ' ' )
                {
                    break;
                }
            }
        }
        else
        {
            // nothing to do
        }
        
        if( (sString >= sStringFence) && (sFormat >= sFormatFence) )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));
    
    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE));
    
    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4EcharMBNormal( mtcNode*     aNode,
                                       mtcStack*    aStack,
                                       SInt         aRemain,
                                       void*        aInfo,
                                       mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� ) 
 *
 ***********************************************************************/
    
    const mtdModule * sModule;
    const mtlModule * sLanguage;

    mtdEcharType    * sEchar;
    mtdCharType     * sVarchar;
    UChar           * sString;
    UChar           * sStringIntermediate;
    UChar           * sStringFence;
    UChar           * sFormat;
    UChar           * sFormatPrev = NULL;
    UChar           * sFormatIntermediate;
    UChar           * sFormatFence;
    UChar           * sEscape = '\0';
    idBool            sNullEscape;
    idBool            sEqual;
    idBool            sEqual1;
    idBool            sEqual2;
    idBool            sEqual3;
    UChar             sSize;
    UChar             sSize2;
    UShort            sEscapeLen = 0;    
    
    mtcNode         * sEncNode;
    mtcColumn       * sEncColumn;
    mtcEncryptInfo    sDecryptInfo;
    UShort            sStringPlainLength;
    UChar           * sStringPlain;
    UChar             sStringDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    UShort            sFormatPlainLength;
    UChar           * sFormatPlain;
    UChar             sFormatDecryptedBuf[MTD_ECHAR_DECRYPT_BUFFER_SIZE];
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    sModule   = aStack[1].column->module;
    sLanguage = aStack[1].column->language;

    if( (sModule->isNull( aStack[1].column,
                          aStack[1].value ) == ID_TRUE) ||
        (sModule->isNull( aStack[2].column,
                          aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        //--------------------------------------------------
        // search string�� plain text�� ��´�.
        //--------------------------------------------------

        sEchar   = (mtdEcharType*)aStack[1].value;
        
        sEncNode = aNode->arguments;
        sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
            + sEncNode->baseColumn;
        
        if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
        {
            sStringPlain = sStringDecryptedBuf;
            
            if( sEchar->mCipherLength > 0 )
            {
                IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                     sEncNode->baseTable,
                                                     sEncNode->baseColumn,
                                                     & sDecryptInfo )
                          != IDE_SUCCESS );
                
                IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                              sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                              sEchar->mValue,
                                              sEchar->mCipherLength,
                                              sStringPlain,
                                              & sStringPlainLength )
                          != IDE_SUCCESS );
                
                IDE_ASSERT_MSG( sStringPlainLength <= sEncColumn->precision,
                                "sStringPlainLength : %"ID_UINT32_FMT"\n"
                                "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                sStringPlainLength, sEncColumn->precision );
            }
            else
            {
                sStringPlainLength = 0;
            }
        }
        else
        {
            sStringPlain = sEchar->mValue;
            sStringPlainLength = sEchar->mCipherLength;
        }
        
        sString      = sStringPlain;
        sStringFence = sStringPlain + sStringPlainLength;
        
        //--------------------------------------------------
        // format string�� plain text�� ��´�.
        //--------------------------------------------------
        
        sEchar = (mtdEcharType*)aStack[2].value;

        sEncNode = sEncNode->next;
        
        // TASK-3876 codesonar
        IDE_TEST_RAISE( sEncNode == NULL,
                        ERR_INVALID_FUNCTION_ARGUMENT );
        
        if ( sEncNode->conversion == NULL )
        {
            // conversion�� null�� ��� default policy�� �ƴ�
            // �÷��� format string�� ���� ��찡 �ִ�.
            sEncColumn = aTemplate->rows[sEncNode->baseTable].columns
                + sEncNode->baseColumn;
        
            if ( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] != '\0' )
            {
                sFormatPlain = sFormatDecryptedBuf;
            
                if( sEchar->mCipherLength > 0 )
                {
                    IDE_TEST( aTemplate->getDecryptInfo( aTemplate,
                                                         sEncNode->baseTable,
                                                         sEncNode->baseColumn,
                                                         & sDecryptInfo )
                              != IDE_SUCCESS );
                
                    IDE_TEST( aTemplate->decrypt( & sDecryptInfo,
                                                  sEncColumn->mColumnAttr.mEncAttr.mPolicy,
                                                  sEchar->mValue,
                                                  sEchar->mCipherLength,
                                                  sFormatPlain,
                                                  & sFormatPlainLength )
                              != IDE_SUCCESS );
                
                    IDE_ASSERT_MSG( sFormatPlainLength <= sEncColumn->precision,
                                    "sFormatPlainLength : %"ID_UINT32_FMT"\n"
                                    "sEncColumn->precision : %"ID_UINT32_FMT"\n",
                                    sFormatPlainLength, sEncColumn->precision );
                }
                else
                {
                    sFormatPlainLength = 0;
                }
            }
            else
            {
                sFormatPlain = sEchar->mValue;
                sFormatPlainLength = sEchar->mCipherLength;
            }
        }
        else
        {
            // conversion�� �ִ� ��� conversion�� �׻�
            // default policy�� ��ȯ�ȴ�.
            sFormatPlain = sEchar->mValue;
            sFormatPlainLength = sEchar->mCipherLength;
        }
        
        sFormat      = sFormatPlain;
        sFormatFence = sFormatPlain + sFormatPlainLength;
        
        //--------------------------------------------------
        // escape ����
        //--------------------------------------------------
        
        if( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
        {
            sVarchar = (mtdCharType*)aStack[3].value;
            if( sLanguage->id == MTL_UTF16_ID )
            {
                IDE_TEST_RAISE( sVarchar->length != 2, ERR_INVALID_ESCAPE );
            }
            else
            {
                IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            }
            
            sEscape = sVarchar->value;
            sEscapeLen = sVarchar->length;
            
            sNullEscape = ID_FALSE;
        }
        else
        {
            sNullEscape = ID_TRUE;
        }
        
        //--------------------------------------------------
        // like ����
        //--------------------------------------------------
        
        for( ; (sString < sStringFence) && (sFormat < sFormatFence); )
        {
            sSize =  mtl::getOneCharSize( sFormat,
                                          sFormatFence,
                                          sLanguage );

            sSize2 =  mtl::getOneCharSize( sString,
                                           sStringFence,
                                           sLanguage );

            if( sNullEscape == ID_FALSE )
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sEscape,
                                              sEscapeLen );
            }
            else
            {
                sEqual = ID_FALSE;
            }
                
            if( sEqual == ID_TRUE ) 
            {
                (void)mtf::nextChar( sFormatFence,
                                     &sFormat,
                                     &sFormatPrev,
                                     sLanguage);

                sSize =  mtl::getOneCharSize( sFormat,
                                              sFormatFence,
                                              sLanguage );

                // escape ������ ���,
                // escape ���� ���ڰ� '%','_' �������� �˻�
                sEqual1 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_PC_IDX],
                                               sLanguage->specialCharSize );
            
                sEqual2 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_UB_IDX],
                                               sLanguage->specialCharSize );
            
                sEqual3 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sEscape,
                                               sEscapeLen );
                
                IDE_TEST_RAISE( (sEqual1 != ID_TRUE) && 
                                (sEqual2 != ID_TRUE) && 
                                (sEqual3 != ID_TRUE), 
                                ERR_INVALID_LITERAL );

                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sString,
                                              sSize2 );
                                
                if( sEqual != ID_TRUE )
                {
                    // Bug-11942 fix
                    // 'aaab' like 'aaa__' escape '_' ó����
                    // sFormat�� ���� aaa__�� ������ _�ε�
                    // b != '_' �̹Ƿ� break�� ������
                    // sFormat�� �ٽ� ������ ��ĭ ��ܾ� �Ѵ�.
                    sFormat = sFormatPrev;
                    break;
                }
            }
            else 
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sLanguage->specialCharSet[MTL_PC_IDX],
                                              sLanguage->specialCharSize );
                            
                if( sEqual != ID_TRUE )
                {
                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sLanguage->specialCharSet[MTL_UB_IDX],
                                                  sLanguage->specialCharSize );
                    
                    if ( sEqual != ID_TRUE )
                    {
                        // �Ϲ� ������ ���
                        sEqual = mtc::compareOneChar( sFormat,
                                                      sSize,
                                                      sString,
                                                      sSize2 );
                        
                        if ( sEqual != ID_TRUE )
                        {
                            break;
                        }
                        else
                        {
                            // keep going
                        }
                    }
                    else
                    {
                        // '_'������ ���
                        // keep going
                    }
                }
                else // %
                {
                    (void)mtf::nextChar( sFormatFence,
                                         &sFormat,
                                         NULL,
                                         sLanguage );
                    
                    for( sStringIntermediate = sString, sFormatIntermediate = sFormat;
                         sStringIntermediate < sStringFence;)
                    {
                        sSize =  mtl::getOneCharSize( sFormatIntermediate,
                                                      sFormatFence,
                                                      sLanguage );

                        sSize2 =  mtl::getOneCharSize( sStringIntermediate,
                                                       sStringFence,
                                                       sLanguage );
                        
                        if( sFormatIntermediate < sFormatFence )
                        {
                            if( sNullEscape == ID_FALSE )
                            {
                                sEqual = mtc::compareOneChar( sFormatIntermediate,
                                                              sSize,
                                                              sEscape,
                                                              sEscapeLen );
                            }
                            else
                            {
                                sEqual = ID_FALSE;
                            }
                            
                            if( sEqual == ID_TRUE )
                            {
                                (void)mtf::nextChar( sFormatFence,
                                                     &sFormatIntermediate,
                                                     NULL,
                                                     sLanguage );
                                
                                sSize =  mtl::getOneCharSize( sFormatIntermediate,
                                                              sFormatFence,
                                                              sLanguage );

                                IDE_TEST_RAISE( sFormatIntermediate >= sFormatFence, ERR_INVALID_LITERAL );

                                sEqual1 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sLanguage->specialCharSet[MTL_PC_IDX],
                                                               sLanguage->specialCharSize );
            
                                sEqual2 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sLanguage->specialCharSet[MTL_UB_IDX],
                                                               sLanguage->specialCharSize );
            
                                sEqual3 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sEscape,
                                                               sEscapeLen );

                                IDE_TEST_RAISE( (sEqual1 != ID_TRUE) && 
                                                (sEqual2 != ID_TRUE) && 
                                                (sEqual3 != ID_TRUE),
                                                ERR_INVALID_LITERAL );

                                sEqual = mtc::compareOneChar( sFormatIntermediate,
                                                              sSize,
                                                              sStringIntermediate,
                                                              sSize2 );
                                
                                if( sEqual != ID_TRUE )
                                {
                                    (void)mtf::nextChar( sStringFence,
                                                         &sString,
                                                         NULL,
                                                         sLanguage );
                                    
                                    sStringIntermediate = sString;
                                    sFormatIntermediate = sFormat;
                                }
                                else
                                {
                                    (void)mtf::nextChar( sStringFence,
                                                         &sStringIntermediate,
                                                         NULL,
                                                         sLanguage );
                                    
                                    (void)mtf::nextChar( sFormatFence,
                                                         &sFormatIntermediate,
                                                         NULL,
                                                         sLanguage );
                                }
                            }
                            else 
                            {
                                sEqual = mtc::compareOneChar( sFormatIntermediate,
                                                              sSize,
                                                              sLanguage->specialCharSet[MTL_PC_IDX],
                                                              sLanguage->specialCharSize );
                                
                                if( sEqual != ID_TRUE )
                                {
                                    sEqual1 = mtc::compareOneChar( sFormatIntermediate,
                                                                   sSize,
                                                                   sLanguage->specialCharSet[MTL_UB_IDX],
                                                                   sLanguage->specialCharSize );
                                    

                                    sEqual2 = mtc::compareOneChar( sFormatIntermediate,
                                                                   sSize,
                                                                   sStringIntermediate,
                                                                   sSize2 );
                                    
                                    if( (sEqual1 != ID_TRUE) && 
                                        (sEqual2 != ID_TRUE) )
                                    {
                                        (void)mtf::nextChar( sStringFence,
                                                             &sString,
                                                             NULL,
                                                             sLanguage );
                                        
                                        sStringIntermediate = sString;
                                        sFormatIntermediate = sFormat;
                                    }
                                    else
                                    {
                                        (void)mtf::nextChar( sStringFence,
                                                             &sStringIntermediate,
                                                             NULL,
                                                             sLanguage );
                                        
                                        (void)mtf::nextChar( sFormatFence,
                                                             &sFormatIntermediate,
                                                             NULL,
                                                             sLanguage );
                                    }
                                }
                                else // %
                                {
                                    sString = sStringIntermediate;
                                    sFormat = sFormatIntermediate;
                                    
                                    (void)mtf::nextChar( sFormatFence,
                                                         &sFormat,
                                                         NULL,
                                                         sLanguage );
                                    
                                    sStringIntermediate = sString;
                                    sFormatIntermediate = sFormat;
                                }
                            }
                        }
                        else
                        {
                            (void)mtf::nextChar( sStringFence,
                                                 &sString,
                                                 NULL,
                                                 sLanguage );
                            
                            sStringIntermediate = sString;
                            sFormatIntermediate = sFormat;
                        }
                    }
                    
                    sString = sStringIntermediate;
                    sFormat = sFormatIntermediate;
                    break;
                }
            }

            (void)mtf::nextChar( sStringFence,
                                 &sString,
                                 NULL,
                                 sLanguage );
            
            (void)mtf::nextChar( sFormatFence,
                                 &sFormat,
                                 NULL,
                                 sLanguage );
        }

        if( (sModule->id == MTD_ECHAR_ID) && (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE))
        {
            for( ; sFormat < sFormatFence; )
            {
                sSize =  mtl::getOneCharSize( sFormat,
                                              sFormatFence,
                                              sLanguage );
                
                if( sNullEscape == ID_FALSE )
                {
                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sEscape,
                                                  sEscapeLen );
                }
                else
                {
                    sEqual = ID_FALSE;
                }

                if( sEqual == ID_TRUE )
                {
                    (void)mtf::nextChar( sFormatFence,
                                         &sFormat,
                                         &sFormatPrev,
                                         sLanguage );
                    
                    sSize =  mtl::getOneCharSize( sFormat,
                                                  sFormatFence,
                                                  sLanguage );

                    sEqual1 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_PC_IDX],
                                                   sLanguage->specialCharSize );
            
                    sEqual2 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_UB_IDX],
                                                   sLanguage->specialCharSize );
            
                    sEqual3 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sEscape,
                                                   sEscapeLen );
                    
                    IDE_TEST_RAISE( (sEqual1 != ID_TRUE) && 
                                    (sEqual2 != ID_TRUE) && 
                                    (sEqual3 != ID_TRUE),
                                    ERR_INVALID_LITERAL );

                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sLanguage->specialCharSet[MTL_SP_IDX],
                                                  sLanguage->specialCharSize );

                    if( sEqual != ID_TRUE )
                    {
                        // Bug-11942 fix
                        sFormat = sFormatPrev;
                        break;
                    }
                }
                else
                {
                    sEqual1 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_PC_IDX],
                                                   sLanguage->specialCharSize );

                    sEqual2 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_SP_IDX],
                                                   sLanguage->specialCharSize );

                    if( (sEqual1 != ID_TRUE) && 
                        (sEqual2 != ID_TRUE) )
                    {
                        break;
                    }
                }

                (void)mtf::nextChar( sFormatFence,
                                     &sFormat,
                                     NULL,
                                     sLanguage );
            }
        }
        else
        {
            for( ; sFormat < sFormatFence; )
            {
                sSize =  mtl::getOneCharSize( sFormat,
                                              sFormatFence,
                                              sLanguage );
                
                if( sNullEscape == ID_FALSE )
                {
                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sEscape,
                                                  sEscapeLen );
                }
                else
                {
                    sEqual = ID_FALSE;
                }

                if( sEqual == ID_TRUE )
                {
                    (void)mtf::nextChar( sFormatFence,
                                         &sFormat,
                                         &sFormatPrev,
                                         sLanguage );
                    
                    sSize =  mtl::getOneCharSize( sFormat,
                                                  sFormatFence,
                                                  sLanguage );

                    sEqual1 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_PC_IDX],
                                                   sLanguage->specialCharSize );
            
                    sEqual2 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sLanguage->specialCharSet[MTL_UB_IDX],
                                                   sLanguage->specialCharSize );
            
                    sEqual3 = mtc::compareOneChar( sFormat,
                                                   sSize,
                                                   sEscape,
                                                   sEscapeLen );

                    IDE_TEST_RAISE( (sEqual1 != ID_TRUE) &&
                                    (sEqual2 != ID_TRUE) &&
                                    (sEqual3 != ID_TRUE), 
                                    ERR_INVALID_LITERAL );

                    // Bug-11942 fix
                    sFormat = sFormatPrev;
                    break;
                }
                else 
                {
                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sLanguage->specialCharSet[MTL_PC_IDX],
                                                  sLanguage->specialCharSize );

                    if( sEqual != ID_TRUE )
                    {
                        break;
                    }
                }

                (void)mtf::nextChar( sFormatFence,
                                     &sFormat,
                                     NULL,
                                     sLanguage );
            }
        }
        
        if( (sModule->id == MTD_ECHAR_ID) && (MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_OLD_MODULE))
        {
            for( ; sString < sStringFence; )
            {
                sSize2 =  mtl::getOneCharSize( sString,
                                               sStringFence,
                                               sLanguage );

                sEqual = mtc::compareOneChar( sString,
                                              sSize2,
                                              sLanguage->specialCharSet[MTL_SP_IDX],
                                              sLanguage->specialCharSize );
                
                if( sEqual != ID_TRUE )
                {
                    break;
                }

                (void)mtf::nextChar( sStringFence,
                                     &sString,
                                     NULL,
                                     sLanguage );
            }
        }
        else
        {
            // nothing to do
        }
        
        if( (sString >= sStringFence) && (sFormat >= sFormatFence) )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_ESCAPE));
    
    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE));
    
    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4ClobValue( mtcNode     * aNode,
                                   mtcStack    * aStack,
                                   SInt          aRemain,
                                   void        * aInfo,
                                   mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *               ������ ���̿� ��� ��⿡ ���� �б⸦ �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� )
 *
 ***********************************************************************/

    const mtcColumn  * sColumn;
    mtdClobType      * sClobValue;
    mtdCharType      * sVarchar;
    UChar            * sString;
    UChar            * sStringFence;
    UChar            * sFormat;
    UChar              sEscape = 0;
    mtdBinaryType    * sTempBinary;
    UInt               sBlockCnt;
    mtcLikeBlockInfo * sBlock;
    UChar            * sRefineString;
    UShort             sFormatLen;
    mtdBooleanType     sResult;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sClobValue   = (mtdClobType *)aStack[1].value;
        sString      = sClobValue->value;
        sStringFence = sString + sClobValue->length;
        sVarchar     = (mtdCharType *)aStack[2].value;
        sFormat      = sVarchar->value;
        sFormatLen   = sVarchar->length;

        sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
        sTempBinary = (mtdBinaryType *)
            ((UChar *)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);

        sBlock = (mtcLikeBlockInfo *)(sTempBinary->mValue);

        sTempBinary = (mtdBinaryType *)
            ((UChar *)aTemplate->rows[aNode->table].row + sColumn[2].column.offset);

        sRefineString = (UChar *)(sTempBinary->mValue);

        if ( (aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK) == 3 )
        {
            sVarchar = (mtdCharType *)aStack[3].value;
            IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            sEscape = sVarchar->value[0];
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( getMoreInfoFromPattern( sFormat,
                                          sFormatLen,
                                          &sEscape,
                                          1,
                                          sBlock,
                                          &sBlockCnt,
                                          sRefineString,
                                          NULL )
                  != IDE_SUCCESS );

        IDE_TEST( mtfLikeCalculateOnePass( sString,
                                           sStringFence,
                                           sBlock,
                                           sBlockCnt,
                                           &sResult )
                  != IDE_SUCCESS );

        *(mtdBooleanType *)aStack[0].value = sResult;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_ESCAPE ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4ClobValueMB( mtcNode     * aNode,
                                     mtcStack    * aStack,
                                     SInt          aRemain,
                                     void        * aInfo,
                                     mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *               ������ ���̿� ��� ��⿡ ���� �б⸦ �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� )
 *
 ***********************************************************************/

    const mtlModule  * sLanguage;
    const mtcColumn  * sColumn;
    mtdClobType      * sClobValue;
    mtdCharType      * sVarchar;
    UChar            * sString;
    UChar            * sStringFence;
    UChar            * sFormat;
    UChar            * sEscape = NULL;
    UShort             sEscapeLen = 0;
    UInt               sFormatLen;
    mtdBinaryType    * sTempBinary;
    UInt               sBlockCnt;
    mtcLikeBlockInfo * sBlock;
    UChar            * sRefineString;
    mtdBooleanType     sResult;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLanguage = aStack[1].column->language;

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sClobValue   = (mtdClobType *)aStack[1].value;
        sString      = sClobValue->value;
        sStringFence = sString + sClobValue->length;
        sVarchar     = (mtdCharType *)aStack[2].value;
        sFormat      = sVarchar->value;
        sFormatLen   = sVarchar->length;

        sColumn = aTemplate->rows[aNode->table].columns + aNode->column;
        sTempBinary = (mtdBinaryType *)
            ((UChar *)aTemplate->rows[aNode->table].row + sColumn[1].column.offset);

        sBlock = (mtcLikeBlockInfo *)(sTempBinary->mValue);

        sTempBinary = (mtdBinaryType *)
            ((UChar *)aTemplate->rows[aNode->table].row + sColumn[2].column.offset);

        sRefineString = (UChar *)(sTempBinary->mValue);

        if ( (aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK) == 3 )
        {
            sVarchar = (mtdCharType *)aStack[3].value;
            if ( sLanguage->id == MTL_UTF16_ID )
            {
                IDE_TEST_RAISE( sVarchar->length != 2, ERR_INVALID_ESCAPE );
            }
            else
            {
                IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            }

            sEscape = sVarchar->value;
            sEscapeLen = sVarchar->length;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( getMoreInfoFromPatternMB( sFormat,
                                            sFormatLen,
                                            sEscape,
                                            sEscapeLen,
                                            sBlock,
                                            &sBlockCnt,
                                            sRefineString,
                                            sLanguage )
                  != IDE_SUCCESS );

        IDE_TEST( mtfLikeCalculateMBOnePass( sString,
                                             sStringFence,
                                             sBlock,
                                             sBlockCnt,
                                             &sResult,
                                             sLanguage )
                  != IDE_SUCCESS );

        *(mtdBooleanType *)aStack[0].value = sResult;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_ESCAPE ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4ClobValueNormal( mtcNode     * aNode,
                                         mtcStack    * aStack,
                                         SInt          aRemain,
                                         void        * aInfo,
                                         mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *               ������ ���̿� ��� ��⿡ ���� �б⸦ �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� )
 *
 ***********************************************************************/

    mtdClobType     * sClobValue;
    mtdCharType     * sVarchar;
    UChar           * sString;
    UChar           * sStringIntermediate;
    UChar           * sStringFence;
    UChar           * sFormat;
    UChar           * sFormatIntermediate;
    UChar           * sFormatFence;
    UChar             sEscape = 0;
    idBool            sNullEscape;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sClobValue   = (mtdClobType *)aStack[1].value;
        sString      = sClobValue->value;
        sStringFence = sString + sClobValue->length;
        sVarchar     = (mtdCharType *)aStack[2].value;
        sFormat      = sVarchar->value;
        sFormatFence = sFormat + sVarchar->length;

        if ( (aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK) == 3 )
        {
            sVarchar = (mtdCharType *)aStack[3].value;
            IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            sEscape = sVarchar->value[0];
            sNullEscape = ID_FALSE;
        }
        else
        {
            sNullEscape = ID_TRUE;
        }

        for ( ; (sString < sStringFence) && (sFormat < sFormatFence);
              sString++, sFormat++ )
        {
            if ( (sNullEscape == ID_FALSE) && (*sFormat == sEscape) )
            {
                sFormat++;
                IDE_TEST_RAISE( sFormat >= sFormatFence,
                                ERR_INVALID_LITERAL );
                IDE_TEST_RAISE( (*sFormat != '%') &&
                                (*sFormat != '_') &&
                                (*sFormat != sEscape), // sEsacpe�� null�� �ƴ�
                                ERR_INVALID_LITERAL );

                if ( *sFormat != *sString )
                {
                    // Bug-11942 fix
                    // 'aaab' like 'aaa__' escape '_' ó����
                    // sFormat�� ���� aaa__�� ������ _�ε�
                    // b != '_' �̹Ƿ� break�� ������
                    // sFormat�� �ٽ� ������ ��ĭ ��ܾ� �Ѵ�.
                    sFormat--;
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else if ( *sFormat != '%' )
            {
                /* To Fix BUG-12306
                 * if( *sFormat != '_' && *sFormat != *sString )
                 * {
                 *     break;
                 * }
                 */
                if ( *sFormat != '_' )
                {
                    // �Ϲ� ������ ���
                    if ( *sFormat != *sString )
                    {
                        break;
                    }
                    else
                    {
                        // keep going
                    }
                }
                else
                {
                    // '_'������ ���
                    // keep going
                }
            }
            else // %
            {
                sFormat += 1;
                for ( sStringIntermediate = sString,
                      sFormatIntermediate = sFormat;
                      sStringIntermediate < sStringFence; )
                {
                    if ( sFormatIntermediate < sFormatFence )
                    {
                        if ( (sNullEscape == ID_FALSE) &&
                             (*sFormatIntermediate == sEscape) )
                        {
                            sFormatIntermediate++;
                            IDE_TEST_RAISE( sFormatIntermediate >= sFormatFence,
                                            ERR_INVALID_LITERAL );

                            IDE_TEST_RAISE( (*sFormatIntermediate != '%') &&
                                            (*sFormatIntermediate != '_') &&
                                            (*sFormatIntermediate != sEscape), // sEsacpe�� null�� �ƴ�
                                            ERR_INVALID_LITERAL );

                            if ( *sStringIntermediate !=
                                 *sFormatIntermediate )
                            {
                                sString++;
                                sStringIntermediate = sString;
                                sFormatIntermediate = sFormat;
                            }
                            else
                            {
                                sStringIntermediate++;
                                sFormatIntermediate++;
                            }
                        }
                        else if ( *sFormatIntermediate != '%' )
                        {
                            if ( (*sFormatIntermediate != '_') &&
                                 (*sFormatIntermediate != *sStringIntermediate) )
                            {
                                sString++;
                                sStringIntermediate = sString;
                                sFormatIntermediate = sFormat;
                            }
                            else
                            {
                                sStringIntermediate++;
                                sFormatIntermediate++;
                            }
                        }
                        else // %
                        {
                            sString             = sStringIntermediate;
                            sFormat             = sFormatIntermediate + 1;
                            sStringIntermediate = sString;
                            sFormatIntermediate = sFormat;
                        }
                    }
                    else
                    {
                        sString++;
                        sStringIntermediate = sString;
                        sFormatIntermediate = sFormat;
                    }
                }
                sString = sStringIntermediate;
                sFormat = sFormatIntermediate;
                break;
            }
        }

        for ( ; sFormat < sFormatFence; sFormat++ )
        {
            if ( (sNullEscape == ID_FALSE) && (*sFormat == sEscape) )
            {
                sFormat++;
                IDE_TEST_RAISE( sFormat >= sFormatFence,
                                ERR_INVALID_LITERAL );

                IDE_TEST_RAISE( (*sFormat != '%') &&
                                (*sFormat != '_') &&
                                (*sFormat != sEscape), // sEsacpe�� null�� �ƴ�
                                ERR_INVALID_LITERAL );

                // Bug-11942 fix
                sFormat--;
                break;
            }
            else if( *sFormat != '%' )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }

        if ( (sString >= sStringFence) && (sFormat >= sFormatFence) )
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType*)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_ESCAPE ) );

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4ClobValueMBNormal( mtcNode     * aNode,
                                           mtcStack    * aStack,
                                           SInt          aRemain,
                                           void        * aInfo,
                                           mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : Like�� Calculate ����
 *               ������ ���̿� ��� ��⿡ ���� �б⸦ �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� )
 *
 ***********************************************************************/

    const mtlModule * sLanguage;
    mtdClobType     * sClobValue;
    mtdCharType     * sVarchar;
    UChar           * sString;
    UChar           * sStringIntermediate;
    UChar           * sStringFence;
    UChar           * sFormat;
    UChar           * sFormatPrev;
    UChar           * sFormatIntermediate;
    UChar           * sFormatFence;
    UChar           * sEscape = NULL;
    idBool            sNullEscape;
    idBool            sEqual;
    idBool            sEqual1;
    idBool            sEqual2;
    idBool            sEqual3;
    UChar             sSize;
    UChar             sSize2;
    UShort            sEscapeLen = 0;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLanguage = aStack[1].column->language;

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sClobValue   = (mtdClobType *)aStack[1].value;
        sString      = sClobValue->value;
        sStringFence = sString + sClobValue->length;
        sVarchar     = (mtdCharType *)aStack[2].value;
        sFormat      = sVarchar->value;
        sFormatPrev  = sFormat;
        sFormatFence = sFormat + sVarchar->length;

        if ( (aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK) == 3 )
        {
            sVarchar = (mtdCharType *)aStack[3].value;
            if ( sLanguage->id == MTL_UTF16_ID )
            {
                IDE_TEST_RAISE( sVarchar->length != 2, ERR_INVALID_ESCAPE );
            }
            else
            {
                IDE_TEST_RAISE( sVarchar->length != 1, ERR_INVALID_ESCAPE );
            }

            sEscape = sVarchar->value;
            sEscapeLen = sVarchar->length;

            sNullEscape = ID_FALSE;
        }
        else
        {
            sNullEscape = ID_TRUE;
        }

        for ( ; (sString < sStringFence) && (sFormat < sFormatFence); )
        {
            sSize = mtl::getOneCharSize( sFormat,
                                         sFormatFence,
                                         sLanguage );

            sSize2 = mtl::getOneCharSize( sString,
                                          sStringFence,
                                          sLanguage );

            if ( sNullEscape == ID_FALSE )
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sEscape,
                                              sEscapeLen );
            }
            else
            {
                sEqual = ID_FALSE;
            }

            if ( sEqual == ID_TRUE )
            {
                (void)mtf::nextChar( sFormatFence,
                                     &sFormat,
                                     &sFormatPrev,
                                     sLanguage );

                sSize = mtl::getOneCharSize( sFormat,
                                             sFormatFence,
                                             sLanguage );

                // escape ������ ���,
                // escape ���� ���ڰ� '%','_' �������� �˻�
                sEqual1 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_PC_IDX],
                                               sLanguage->specialCharSize );

                sEqual2 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_UB_IDX],
                                               sLanguage->specialCharSize );

                sEqual3 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sEscape,
                                               sEscapeLen );

                IDE_TEST_RAISE( (sEqual1 != ID_TRUE) &&
                                (sEqual2 != ID_TRUE) &&
                                (sEqual3 != ID_TRUE),
                                ERR_INVALID_LITERAL );

                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sString,
                                              sSize2 );

                if ( sEqual != ID_TRUE )
                {
                    // Bug-11942 fix
                    // 'aaab' like 'aaa__' escape '_' ó����
                    // sFormat�� ���� aaa__�� ������ _�ε�
                    // b != '_' �̹Ƿ� break�� ������
                    // sFormat�� �ٽ� ������ ��ĭ ��ܾ� �Ѵ�.
                    sFormat = sFormatPrev;
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sLanguage->specialCharSet[MTL_PC_IDX],
                                              sLanguage->specialCharSize );

                if ( sEqual != ID_TRUE )
                {
                    sEqual = mtc::compareOneChar( sFormat,
                                                  sSize,
                                                  sLanguage->specialCharSet[MTL_UB_IDX],
                                                  sLanguage->specialCharSize );

                    if ( sEqual != ID_TRUE )
                    {
                        // �Ϲ� ������ ���
                        sEqual = mtc::compareOneChar( sFormat,
                                                      sSize,
                                                      sString,
                                                      sSize2 );

                        if ( sEqual != ID_TRUE )
                        {
                            break;
                        }
                        else
                        {
                            // keep going
                        }
                    }
                    else
                    {
                        // '_'������ ���
                        // keep going
                    }
                }
                else // %
                {
                    (void)mtf::nextChar( sFormatFence,
                                         &sFormat,
                                         NULL,
                                         sLanguage );

                    for ( sStringIntermediate = sString, sFormatIntermediate = sFormat;
                          sStringIntermediate < sStringFence;)
                    {
                        sSize = mtl::getOneCharSize( sFormatIntermediate,
                                                     sFormatFence,
                                                     sLanguage );

                        sSize2 = mtl::getOneCharSize( sStringIntermediate,
                                                      sStringFence,
                                                      sLanguage );

                        if ( sFormatIntermediate < sFormatFence )
                        {
                            if ( sNullEscape == ID_FALSE )
                            {
                                sEqual = mtc::compareOneChar( sFormatIntermediate,
                                                              sSize,
                                                              sEscape,
                                                              sEscapeLen );
                            }
                            else
                            {
                                sEqual = ID_FALSE;
                            }

                            if ( sEqual == ID_TRUE )
                            {
                                (void)mtf::nextChar( sFormatFence,
                                                     &sFormatIntermediate,
                                                     NULL,
                                                     sLanguage );

                                sSize = mtl::getOneCharSize( sFormatIntermediate,
                                                             sFormatFence,
                                                             sLanguage );

                                IDE_TEST_RAISE( sFormatIntermediate >= sFormatFence, ERR_INVALID_LITERAL );

                                sEqual1 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sLanguage->specialCharSet[MTL_PC_IDX],
                                                               sLanguage->specialCharSize );

                                sEqual2 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sLanguage->specialCharSet[MTL_UB_IDX],
                                                               sLanguage->specialCharSize );

                                sEqual3 = mtc::compareOneChar( sFormatIntermediate,
                                                               sSize,
                                                               sEscape,
                                                               sEscapeLen );

                                IDE_TEST_RAISE( (sEqual1 != ID_TRUE) &&
                                                (sEqual2 != ID_TRUE) &&
                                                (sEqual3 != ID_TRUE),
                                                ERR_INVALID_LITERAL );

                                sEqual = mtc::compareOneChar( sFormatIntermediate,
                                                              sSize,
                                                              sStringIntermediate,
                                                              sSize2 );

                                if ( sEqual != ID_TRUE )
                                {
                                    (void)mtf::nextChar( sStringFence,
                                                         &sString,
                                                         NULL,
                                                         sLanguage );

                                    sStringIntermediate = sString;
                                    sFormatIntermediate = sFormat;
                                }
                                else
                                {
                                    (void)mtf::nextChar( sStringFence,
                                                         &sStringIntermediate,
                                                         NULL,
                                                         sLanguage );

                                    (void)mtf::nextChar( sFormatFence,
                                                         &sFormatIntermediate,
                                                         NULL,
                                                         sLanguage );
                                }
                            }
                            else
                            {
                                sEqual = mtc::compareOneChar( sFormatIntermediate,
                                                              sSize,
                                                              sLanguage->specialCharSet[MTL_PC_IDX],
                                                              sLanguage->specialCharSize );

                                if ( sEqual != ID_TRUE )
                                {
                                    sEqual1 = mtc::compareOneChar( sFormatIntermediate,
                                                                   sSize,
                                                                   sLanguage->specialCharSet[MTL_UB_IDX],
                                                                   sLanguage->specialCharSize );


                                    sEqual2 = mtc::compareOneChar( sFormatIntermediate,
                                                                   sSize,
                                                                   sStringIntermediate,
                                                                   sSize2 );

                                    if ( (sEqual1 != ID_TRUE) &&
                                         (sEqual2 != ID_TRUE) )
                                    {
                                        (void)mtf::nextChar( sStringFence,
                                                             &sString,
                                                             NULL,
                                                             sLanguage );

                                        sStringIntermediate = sString;
                                        sFormatIntermediate = sFormat;
                                    }
                                    else
                                    {
                                        (void)mtf::nextChar( sStringFence,
                                                             &sStringIntermediate,
                                                             NULL,
                                                             sLanguage );

                                        (void)mtf::nextChar( sFormatFence,
                                                             &sFormatIntermediate,
                                                             NULL,
                                                             sLanguage );
                                    }
                                }
                                else // %
                                {
                                    sString = sStringIntermediate;
                                    sFormat = sFormatIntermediate;

                                    (void)mtf::nextChar( sFormatFence,
                                                         &sFormat,
                                                         NULL,
                                                         sLanguage );

                                    sStringIntermediate = sString;
                                    sFormatIntermediate = sFormat;
                                }
                            }
                        }
                        else
                        {
                            (void)mtf::nextChar( sStringFence,
                                                 &sString,
                                                 NULL,
                                                 sLanguage );

                            sStringIntermediate = sString;
                            sFormatIntermediate = sFormat;
                        }
                    }

                    sString = sStringIntermediate;
                    sFormat = sFormatIntermediate;
                    break;
                }
            }

            (void)mtf::nextChar( sStringFence,
                                 &sString,
                                 NULL,
                                 sLanguage );

            (void)mtf::nextChar( sFormatFence,
                                 &sFormat,
                                 NULL,
                                 sLanguage );
        }

        for ( ; sFormat < sFormatFence; )
        {
            sSize = mtl::getOneCharSize( sFormat,
                                         sFormatFence,
                                         sLanguage );

            if( sNullEscape == ID_FALSE )
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sEscape,
                                              sEscapeLen );
            }
            else
            {
                sEqual = ID_FALSE;
            }

            if ( sEqual == ID_TRUE )
            {
                (void)mtf::nextChar( sFormatFence,
                                     &sFormat,
                                     &sFormatPrev,
                                     sLanguage );

                sSize = mtl::getOneCharSize( sFormat,
                                             sFormatFence,
                                             sLanguage );

                sEqual1 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_PC_IDX],
                                               sLanguage->specialCharSize );

                sEqual2 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sLanguage->specialCharSet[MTL_UB_IDX],
                                               sLanguage->specialCharSize );

                sEqual3 = mtc::compareOneChar( sFormat,
                                               sSize,
                                               sEscape,
                                               sEscapeLen );

                IDE_TEST_RAISE( (sEqual1 != ID_TRUE) &&
                                (sEqual2 != ID_TRUE) &&
                                (sEqual3 != ID_TRUE),
                                ERR_INVALID_LITERAL );

                // Bug-11942 fix
                sFormat = sFormatPrev;
                break;
            }
            else
            {
                sEqual = mtc::compareOneChar( sFormat,
                                              sSize,
                                              sLanguage->specialCharSet[MTL_PC_IDX],
                                              sLanguage->specialCharSize );

                if( sEqual != ID_TRUE )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }

            (void)mtf::nextChar( sFormatFence,
                                 &sFormat,
                                 NULL,
                                 sLanguage );
        }

        if ( (sString >= sStringFence) && (sFormat >= sFormatFence) )
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_ESCAPE ) );

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLikeEstimateClobValueFast( mtcNode     * aNode,
                                     mtcTemplate * aTemplate,
                                     mtcStack    * aStack,
                                     SInt          /* aRemain */,
                                     mtcCallBack * aCallBack )
{
    mtcColumn         * sIndexColumn;
    mtcLikeFormatInfo * sFormatInfo;
    UShort              sFormatLen;

    // BUG-40992 FATAL when using _prowid
    // ������ ��� mtcStack �� column ���� �̿��ϸ� �ȴ�.
    sIndexColumn     = aStack[1].column;

    if ( mtfLikeFormatInfo( aNode, aTemplate, aStack, & sFormatInfo, &sFormatLen, aCallBack) == IDE_SUCCESS )
    {
        if ( sFormatInfo != NULL )
        {
            switch ( sFormatInfo->type )
            {
                case MTC_FORMAT_NORMAL:
                {
                    // search_value = format pattern
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateEqual4ClobValueFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_UNDER:
                {
                    // character_length( search_value ) = underCnt
                    if ( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4ClobValueFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4ClobValueMBFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    break;
                }
                case MTC_FORMAT_PERCENT:
                {
                    // search_value is not null
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateIsNotNull4ClobValueFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_NORMAL_ONE_PERCENT:
                {
                    // search_value = [head]%[tail]
                    aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                        mtfLikeCalculateOnePercent4ClobValueFast;
                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                        sFormatInfo;
                    break;
                }
                case MTC_FORMAT_UNDER_PERCENT:
                {
                    // character_length( search_value ) >= underCnt
                    if ( sIndexColumn->language == &mtlAscii )
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4ClobValueFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    else
                    {
                        aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                            mtfLikeCalculateLength4ClobValueMBFast;
                        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                            sFormatInfo;
                    }
                    break;
                }
                case MTC_FORMAT_ALL:
                case MTC_FORMAT_NORMAL_MANY_PERCENT:
                case MTC_FORMAT_NORMAL_UNDER:
                {
                    if ( MTU_LIKE_OP_USE_MODULE == MTU_LIKE_USE_NEW_MODULE )
                    {
                        if ( sIndexColumn->language == &mtlAscii )
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                                mtfLikeCalculate4ClobValueNormalFast;
                            aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                                sFormatInfo;
                        }
                        else
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column].calculate =
                                mtfLikeCalculate4ClobValueMBNormalFast;
                            aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo =
                                sFormatInfo;
                        }
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                    break;
                }
                case MTC_FORMAT_NULL:
                {
                    /* Nothing to do */
                    break;
                }
                default:
                {
                    ideLog::log( IDE_ERR_0,
                                 "sFormatInfo->type : %u\n",
                                 sFormatInfo->type );

                    IDE_ASSERT( 0 );
                    /* break; */
                }
            }
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;
}

IDE_RC mtfLikeCalculate4ClobValueNormalFast( mtcNode     * aNode,
                                             mtcStack    * aStack,
                                             SInt          aRemain,
                                             void        * aInfo,
                                             mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : �ܼ��� ������ �ƴ� ���
 *                Like�� Calculate ����
 *                ���� ���̿� ��� ��⿡ ���� ó�� ����� �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� )
 *
 ***********************************************************************/

    mtdClobType       * sClobValue;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    mtdBooleanType      sResult;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sClobValue   = (mtdClobType *)aStack[1].value;
        sString      = sClobValue->value;
        sStringFence = sString + sClobValue->length;

        sFormatInfo = (mtcLikeFormatInfo *) aInfo;

        IDE_TEST( mtfLikeCalculateOnePass( sString,
                                           sStringFence,
                                           (sFormatInfo->blockInfo),
                                           sFormatInfo->blockCnt,
                                           &sResult )
                  != IDE_SUCCESS );

        *(mtdBooleanType *)aStack[0].value = sResult;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateEqual4ClobValueFast( mtcNode     * aNode,
                                            mtcStack    * aStack,
                                            SInt          aRemain,
                                            void        * aInfo,
                                            mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *               PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdClobType       * sClobValue;
    UChar             * sString;
    mtcLikeFormatInfo * sFormatInfo;
    SInt                sCompare;
    UInt                sStringLen;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sClobValue   = (mtdClobType *)aStack[1].value;
        sString      = sClobValue->value;
        sStringLen   = sClobValue->length;
        sFormatInfo  = (mtcLikeFormatInfo *) aInfo;

        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_NORMAL,
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );

        if ( sStringLen == sFormatInfo->patternSize )
        {
            sCompare = idlOS::memcmp( sString,
                                      sFormatInfo->pattern,
                                      sFormatInfo->patternSize );
        }
        else
        {
            sCompare = -1;
        }

        if ( sCompare == 0 )
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateIsNotNull4ClobValueFast( mtcNode     * aNode,
                                                mtcStack    * aStack,
                                                SInt          aRemain,
                                                void        * aInfo,
                                                mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *               PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdClobType       * sClobValue;
    mtcLikeFormatInfo * sFormatInfo;
    UInt                sStringLen;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sClobValue  = (mtdClobType *)aStack[1].value;
        sStringLen  = sClobValue->length;

        sFormatInfo = (mtcLikeFormatInfo *) aInfo;

        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_PERCENT,
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );

        if ( sStringLen > 0 )
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateLength4ClobValueFast( mtcNode     * aNode,
                                             mtcStack    * aStack,
                                             SInt          aRemain,
                                             void        * aInfo,
                                             mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *               PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdClobType       * sClobValue;
    mtcLikeFormatInfo * sFormatInfo;
    SInt                sCompare;
    UInt                sStringLen;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sClobValue   = (mtdClobType *)aStack[1].value;
        sStringLen   = sClobValue->length;
        sFormatInfo  = (mtcLikeFormatInfo *) aInfo;

        IDE_ASSERT( sFormatInfo != NULL );

        if ( sFormatInfo->type == MTC_FORMAT_UNDER )
        {
            // character_length( search_value ) = underCnt

            if ( sStringLen == sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }
        else
        {
            IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_UNDER_PERCENT,
                            "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                            sFormatInfo->type );

            // character_length( search_value ) >= underCnt

            if ( sStringLen >= sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }

        if ( sCompare == 0 )
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateOnePercent4ClobValueFast( mtcNode     * aNode,
                                                 mtcStack    * aStack,
                                                 SInt          aRemain,
                                                 void        * aInfo,
                                                 mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *               PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdClobType       * sClobValue;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    SInt                sCompare;
    UInt                sStringLen;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sClobValue   = (mtdClobType *)aStack[1].value;
        sString      = sClobValue->value;
        sStringFence = sString + sClobValue->length;
        sStringLen   = sClobValue->length;
        sFormatInfo  = (mtcLikeFormatInfo *) aInfo;

        IDE_ASSERT( sFormatInfo != NULL );
        IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_NORMAL_ONE_PERCENT,
                        "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                        sFormatInfo->type );
        IDE_ASSERT_MSG( sFormatInfo->percentCnt == 1,
                        "sFormatInfo->percentCnt : %"ID_UINT32_FMT"\n",
                        sFormatInfo->percentCnt );

        // search_value = [head]%[tail]

        if ( sStringLen >= sFormatInfo->headSize + sFormatInfo->tailSize )
        {
            if ( sFormatInfo->headSize > 0 )
            {
                sCompare = idlOS::memcmp( sString,
                                          sFormatInfo->head,
                                          sFormatInfo->headSize );
            }
            else
            {
                sCompare = 0;
            }

            if ( sCompare == 0 )
            {
                if ( sFormatInfo->tailSize > 0 )
                {
                    sCompare = idlOS::memcmp( sStringFence - sFormatInfo->tailSize,
                                              sFormatInfo->tail,
                                              sFormatInfo->tailSize );
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            sCompare = -1;
        }

        if ( sCompare == 0 )
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculate4ClobValueMBNormalFast( mtcNode     * aNode,
                                               mtcStack    * aStack,
                                               SInt          aRemain,
                                               void        * aInfo,
                                               mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : �ܼ��� ������ �ƴ� ���
 *                Like�� Calculate ����
 *                ���� ���̿� ��� ��⿡ ���� ó�� ����� �����Ѵ�.
 *
 * Implementation :
 *    ex ) WHERE dname LIKE '%\_dep%' ESCAPE '\'
 *
 *    aStack[0] : ���
 *    aStack[1] : �о�� ��Ʈ��
 *               ( eg. 'develop_dep'�� ���� dname Į����)
 *    aStack[2] : ���� ��Ʈ��
 *               ( eg. '\_dep'�� ���� ���� ��ġ �˻� ���ǰ� )
 *    aStack[3] : escape ����
 *               ( eg. '_dep'�� ���� ��ġ �˻� ���ǰ����� ����� ���,
 *                     '_'�� ���� Ư�� ���ڸ� �Ϲ� ���ڷ� �ν��ϵ��� �ϴ�
 *                     '\' ���ڸ� ������ ��� )
 *
 ***********************************************************************/

    const mtlModule   * sLanguage;
    mtdClobType       * sClobValue;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    mtdBooleanType      sResult;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    sLanguage = aStack[1].column->language;

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {

        sClobValue   = (mtdClobType *)aStack[1].value;
        sString      = sClobValue->value;
        sStringFence = sString + sClobValue->length;

        sFormatInfo = (mtcLikeFormatInfo *) aInfo;

        IDE_TEST( mtfLikeCalculateMBOnePass( sString,
                                             sStringFence,
                                             (sFormatInfo->blockInfo),
                                             sFormatInfo->blockCnt,
                                             &sResult,
                                             sLanguage )
                  != IDE_SUCCESS );

        *(mtdBooleanType *)aStack[0].value = sResult;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtfLikeCalculateLength4ClobValueMBFast( mtcNode     * aNode,
                                               mtcStack    * aStack,
                                               SInt          aRemain,
                                               void        * aInfo,
                                               mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1755 Like�� Calculate Fast ����
 *               PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    const mtlModule   * sLanguage;
    mtdClobType       * sClobValue;
    UChar             * sString;
    UChar             * sStringFence;
    mtcLikeFormatInfo * sFormatInfo;
    mtdBigintType       sLength;
    SInt                sCompare;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( (aStack[1].column->module->isNull( aStack[1].column,
                                            aStack[1].value ) == ID_TRUE) ||
         (aStack[2].column->module->isNull( aStack[2].column,
                                            aStack[2].value ) == ID_TRUE) )
    {
        *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_NULL;
    }
    else
    {
        sLanguage   = aStack[1].column->language;
        sLength     = 0;

        sClobValue  = (mtdClobType *)aStack[1].value;
        sFormatInfo = (mtcLikeFormatInfo *) aInfo;

        sString      = sClobValue->value;
        sStringFence = sString + sClobValue->length;

        IDE_ASSERT( sFormatInfo != NULL );

        if ( sFormatInfo->type == MTC_FORMAT_UNDER )
        {
            // character_length( search_value ) = underCnt

            while ( sString < sStringFence )
            {
                (void)sLanguage->nextCharPtr( &sString, sStringFence );

                sLength++;

                if ( sLength > sFormatInfo->underCnt )
                {
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }

            if ( sLength == sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }
        else
        {
            IDE_ASSERT_MSG( sFormatInfo->type == MTC_FORMAT_UNDER_PERCENT,
                            "sFormatInfo->type : %"ID_UINT32_FMT"\n",
                            sFormatInfo->type );

            // character_length( search_value ) >= underCnt

            while ( sString < sStringFence )
            {
                (void)sLanguage->nextCharPtr( &sString, sStringFence );

                sLength++;

                if ( sLength > sFormatInfo->underCnt )
                {
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }

            if ( sLength >= sFormatInfo->underCnt )
            {
                sCompare = 0;
            }
            else
            {
                sCompare = -1;
            }
        }

        if ( sCompare == 0 )
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_TRUE;
        }
        else
        {
            *(mtdBooleanType *)aStack[0].value = MTD_BOOLEAN_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
