# --- Variables ---
CC = gcc

# Compilation flags
# -Wall -Wextra = Show all warnings
# -g = Include debug symbols (for gdb)
# -std=c11 = Use the 2011 C standard
CFLAGS = -Wall -Wextra -g -std=c11

# The name of our final executable program
TARGET = stp_sim

OBJS = main.o stp.o

# --- Rules (Targets) ---
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c stp.h
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all run clean
