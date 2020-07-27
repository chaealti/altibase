#
# $Id: sd_objs.mk 77538 2016-10-10 02:28:36Z timmy.kim $
#

SDI_SRCS = $(SD_DIR)/sdi/sdi.cpp                     \
           $(SD_DIR)/sdi/sdiStatementManager.cpp     \
           $(SD_DIR)/sdi/sdiFailoverTypeStorage.cpp

SDU_SRCS = $(SD_DIR)/sdu/sduProperty.cpp

SDM_SRCS = $(SD_DIR)/sdm/sdm.cpp                     \
           $(SD_DIR)/sdm/sdmFixedTable.cpp           \
           $(SD_DIR)/sdm/sdmShardPin.cpp

SDP_SRCS = $(SD_DIR)/sdp/sdpjl.cpp                   \
           $(SD_DIR)/sdp/sdpjy.cpp                   \
           $(SD_DIR)/sdp/sdpjManager.cpp

SDF_SRCS = $(SD_DIR)/sdf/sdf.cpp                     \
           $(SD_DIR)/sdf/sdfCreateMeta.cpp           \
           $(SD_DIR)/sdf/sdfResetMetaNodeId.cpp      \
           $(SD_DIR)/sdf/sdfSetNode.cpp              \
           $(SD_DIR)/sdf/sdfUnsetNode.cpp            \
           $(SD_DIR)/sdf/sdfSetShardTable.cpp        \
           $(SD_DIR)/sdf/sdfSetShardProcedure.cpp    \
           $(SD_DIR)/sdf/sdfSetShardRange.cpp        \
           $(SD_DIR)/sdf/sdfSetShardClone.cpp        \
           $(SD_DIR)/sdf/sdfSetShardSolo.cpp         \
           $(SD_DIR)/sdf/sdfResetShardResidentNode.cpp \
           $(SD_DIR)/sdf/sdfUnsetShardTable.cpp      \
           $(SD_DIR)/sdf/sdfUnsetShardTableByID.cpp  \
           $(SD_DIR)/sdf/sdfUnsetOldSmn.cpp          \
           $(SD_DIR)/sdf/sdfExecImmediate.cpp        \
           $(SD_DIR)/sdf/sdfShardKey.cpp             \
           $(SD_DIR)/sdf/sdfShardNodeName.cpp        \
           $(SD_DIR)/sdf/sdfShardCondition.cpp       \
           $(SD_DIR)/sdf/sdfResetNodeInternal.cpp    \
           $(SD_DIR)/sdf/sdfResetNodeExternal.cpp

SDL_SRCS = $(SD_DIR)/sdl/sdl.cpp                     \
           $(SD_DIR)/sdl/sdlDataTypes.cpp			 \
           $(SD_DIR)/sdl/sdlStatementManager.cpp

SDA_SRCS = $(SD_DIR)/sda/sda.cpp

SD_SRCS = $(SDI_SRCS) $(SDU_SRCS) $(SDM_SRCS) $(SDP_SRCS) $(SDF_SRCS) $(SDL_SRCS) $(SDA_SRCS)

SD_OBJS = $(SD_SRCS:$(DEV_DIR)/%.cpp=$(TARGET_DIR)/%.$(OBJEXT))
SD_SHOBJS = $(SD_SRCS:$(DEV_DIR)/%.cpp=$(TARGET_DIR)/%_shobj.$(OBJEXT))
