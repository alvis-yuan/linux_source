CC = gcc  
CFLAGS = -std=gnu11 -Wall -Wextra  -O0 -g  -I/home/xiaoyuan/workspace/printer/fw/x2600/build/zlib-1.2.8/contrib/minizip/ -I..
LDFLAGS = -lz -lminizip -L/home/xiaoyuan/workspace/printer/fw/x2600/build/zlib-1.2.8/  -lcrypto
  
SRCS = zip_utils.c  
OBJS = $(SRCS:.c=.o)  
TARGET = libuzip.a  
STREAM_EX=extract_zip
EX2MEM=extract_to_memory
LIST_FILES=list_zip
FW_TEST=fw_validate
  
.PHONY: all clean test  
  
all: $(TARGET) $(FW_TEST) #$(STREAM_EX) $(LIST_FILES) $(EX2MEM)
  
$(TARGET): $(OBJS)  
	ar rcs $@ $^  
  
$(STREAM_EX): extract_zip.c $(TARGET)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)  

$(FW_TEST): fw_validate.c $(TARGET)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)  

$(LIST_FILES): list_zip.c $(TARGET)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)  

$(EX2MEM): extract_to_memory.c $(TARGET)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)  

%.o: %.c zip_utils.h
	$(CC) $(CFLAGS) -c $< -o $@  
  
test: $(TEST)  
	./$(TEST)  
  
clean:  
	rm -f $(OBJS) $(TARGET) $(STREAM_EX) $(LIST_FILES)
