CXX     = g++
FLAGS   = -std=c++20 -I. -ggdb
OUT     = rewind
SRC     = src/main.cpp
LIBS    = src/*.hpp
SHLIBS  = src/shell/*.hpp
OBJ     = build/main.o

.PHONY: clean

main: $(OBJ)
	$(CXX) $(FLAGS) $^ -o $(OUT)

$(OBJ): $(SRC) $(LIBS) $(SHLIBS)
	mkdir -p build
	$(CXX) $(FLAGS) -c $< -o $@

clean:
	rm -rf build
	rm -rf rewind
