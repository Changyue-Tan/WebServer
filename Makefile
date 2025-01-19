# 编译器
CC = clang 

# 编译选项
CFLAGS = -Wall -Wextra -fsanitize=address,leak,undefined -O2 -std=c11 -g

# 目标可执行文件
TARGET = WebServer

# 源文件
SRCS = WebServer.c

# 生成的目标文件
OBJS = $(SRCS:.c=.o)

# 默认目标
all: $(TARGET)

# 生成可执行文件
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# 生成目标文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理生成的文件
clean:
	rm -f $(OBJS) $(TARGET)

# 重新编译
rebuild: clean all

# 伪目标
.PHONY: all clean rebuild