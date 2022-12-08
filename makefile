CXX     = g++
FLAGS   = -std=c++20
OUT     = rewind
SRC     = src/main.cpp
LIBS    = src/*.hpp
OBJ     = build/main.o

.PHONY: clean

main: $(OBJ)
	$(CXX) $(FLAGS) $^ -o $(OUT)

$(OBJ): $(SRC) $(LIBS)
	mkdir -p build
	$(CXX) $(FLAGS) -c $< -o $@

clean:
	rm -rf build
	rm -rf rewind
