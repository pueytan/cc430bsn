RSSITEST_OBJS += \
	$(LIB_OBJS) \
	rssitest/rssitest.o

rssitest: $(addprefix $(BUILD_DIR)/, $(RSSITEST_OBJS))
	$(CC) $(CFLAGS) $(addprefix $(BUILD_DIR)/, $(RSSITEST_OBJS)) -o \
		$(addprefix $(BUILD_DIR)/, program.elf) $(LFLAGS)