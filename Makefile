CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -g
TARGET = msh

all: $(TARGET)

$(TARGET): msh.c
	$(CC) $(CFLAGS) msh.c -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

