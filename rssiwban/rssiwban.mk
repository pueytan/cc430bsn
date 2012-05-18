RSSIWBAN_OBJS += \
	$(LIB_OBJS) \
	rssiwban/rssiwban.o

rssiwban: $(addprefix $(BUILD_DIR)/, $(RSSIWBAN_OBJS))
	$(CC) $(CFLAGS) $(addprefix $(BUILD_DIR)/, $(RSSIWBAN_OBJS)) -o \
		$(addprefix $(BUILD_DIR)/, program.elf) $(LFLAGS)