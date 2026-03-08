# --- General Configuration ---

TARGET = hw2
SRCS = main.c functions.c
OBJS = $(SRCS:.c=.o)
CFLAGS = -Wall -std=c99 -pthread

# --- Targets ---

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(CFLAGS)

%.o: %.c hw2.h
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(TARGET) $(OBJS) count*.txt thread*.txt dispatcher.txt stats.txt *.o
