include $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))/../../env.mk
include $(MM_DIR)/lib/mm_objs.mk

# MM�� �ΰ��ɼ� ó��

INCLUDES += $(foreach i,$(MM_DIR)/include $(QP_DIR)/include $(SD_DIR)/include $(RP_DIR)/include $(MT_DIR)/include $(ST_DIR)/include $(SM_DIR)/include  $(CM_DIR)/include $(DK_DIR)/include $(CORE_EXT_DIR)/include .,$(IDROPT)$(i))
INCLUDES_CLI += $(foreach i,$(MM_DIR)/include $(QP_DIR)/include $(SD_DIR)/include $(RP_DIR)/include $(MT_DIR)/include $(ST_DIR)/include $(SM_DIR)/include $(CM_DIR)/include .,$(IDROPT)$(i))
LFLAGS += $(foreach i,$(ALTI_HOME)/lib $(PD_DIR)/lib $(ID_DIR)/lib $(SM_DIR)/lib $(ST_DIR)/lib $(MT_DIR)/lib $(QP_DIR)/lib $(SD_DIR)/lib $(RP_DIR)/lib $(MM_DIR)/lib $(DK_DIR)/lib,$(LDROPT)$(i))

MODULE_LIST = mm dk qp sd rp mt st sm cm id pd

SERVER_LIBS = $(foreach i,$(MODULE_LIST),$(LIBOPT)$(i)$(LIBAFT)) \
	$(ALTICORE_STATIC_LIB) $(LIBS)
SERVER_DEPS = $(foreach i,$(MODULE_LIST),$(ALTI_HOME)/lib/$(LIBPRE)$(i).$(LIBEXT))

SA_LLIBS  = $(LIBOPT)sm_sa$(LIBAFT) $(LIBS)
MMLIB=$(MM_DIR)/lib/$(LIBPRE)mm.$(LIBEXT)
CLILIB=$(UT_DIR)/cli2/lib/$(LIBPRE)cli.$(LIBEXT)
# ���߿� ������ �� �ֵ��� : ���� ȣȯ���� ���� ������ ��
COMPILE=compile
INSTALL=install
