CXX = g++
# Adaugam -pthread aici
CXXFLAGS = -Wall -Werror -Wextra -O3 -g -fno-omit-frame-pointer -march=native -std=c++14 -pthread -Iinclude

SRC_DIR  := src
OBJ_DIR  := build
BIN_DIR  := build
INC_DIR  := include
LOG_DIR  := logs

TARGET   := $(BIN_DIR)/HiveMindApp

SRCS     := $(wildcard $(SRC_DIR)/*.cpp)
OBJS     := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))


all: directories $(TARGET)

directories:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(LOG_DIR)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)/* $(LOG_DIR)/* perf.data*

run: all
	./$(TARGET)

bench: all
	./$(TARGET) --benchmark

.PHONY: all clean run bench directories
