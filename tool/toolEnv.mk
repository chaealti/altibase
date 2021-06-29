include $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))/../env.mk
# QP�� �ΰ��ɼ� ó�� 

INCLUDES := $(IDROPT)$(MM_DIR)/src/include/ $(IDROPT)$(QP_DIR)/src/include/ $(IDROPT)$(SM_DIR)/src/include/ $(INCLUDES)

ifeq "$(OS_TARGET)" "X86_64_DARWIN"
	INCLUDES += $(IDROPT)/opt/local/include
endif

LIBDIRS  := $(LDROPT)$(ID_DIR)/src/lib $(LDROPT)$(SM_DIR)/src/lib/ $(LDROPT)$(QP_DIR)/src/lib/ $(LDROPT)$(MM_DIR)/src/lib/ $(LIBDIRS)

ifeq "$(OS_TARGET)" "X86_64_DARWIN"
	LIBDIRS += $(IDROPT)/opt/local/lib
endif

SERVER_LIBS = $(LIBOPT)mm$(LIBAFT) $(LIBOPT)qp$(LIBAFT) $(LIBOPT)sm$(LIBAFT) $(SERVER_ID_LIB) $(LIBS)  

CLI_SA_LIBS = $(CLI_ID_SA_LIB) $(LIBS)
CLI_MT_LIBS = $(CLI_ID_MT_LIB) $(LIBS)
QPLIB=$(QP_DIR)/src/lib/libqp.$(LIBEXT)
CLILIB=$(UT_DIR)/sqlcli/lib/libsqlcli.$(LIBEXT)

# ���߿� ������ �� �ֵ��� : ���� ȣȯ���� ���� ������ ��
COMPILE=compile
INSTALL=install

#########################################
# implicit rules for TOOL only
#########################################
$(TARGET_DIR)/%.$(OBJEXT): $(DEV_DIR)/%.cpp
	$(COMPILE_IT)

$(TARGET_DIR)/%.$(OBJEXT): $(DEV_DIR)/%.c
	mkdir -p $(dir $@)
	$(COMPILE.c) $(INCLUDES) $(CC_OUTPUT_FLAG)$@ $<

$(TARGET_DIR)/%.$(OBJEXT): $(DEV_DIR)/%.s
	mkdir -p $(dir $@)
	$(COMPILE.c) $(INCLUDES) $(CC_OUTPUT_FLAG)$@ $<

$(TARGET_DIR)/%.$(OBJEXT): $(DEV_DIR)/%.S
	mkdir -p $(dir $@)
	$(COMPILE.c) $(INCLUDES) $(CC_OUTPUT_FLAG)$@ $<

$(TARGET_DIR)/%_shobj.$(OBJEXT): $(DEV_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(COMPILE.cc) $(INCLUDES) $(PIC) $(CC_OUTPUT_FLAG)$@ $<

$(TARGET_DIR)/%_shobj.$(OBJEXT): $(DEV_DIR)/%.c
	mkdir -p $(dir $@)
	$(COMPILE.c) $(INCLUDES) $(PIC) $(CC_OUTPUT_FLAG)$@ $<
