CXX     = g++
FLAGS   = -std=c++20 -I. -lreadline
OUT     = rewind
SRC     = src/main.cpp
LIBS    = src/*.hpp
SHLIBS  = src/shell/*.hpp
OBJ     = build/main.o

.PHONY: clean

$(OUT): $(OBJ)
	$(CXX) $(FLAGS) $^ -o $@

$(OBJ): $(SRC) $(LIBS) $(SHLIBS) src/matchit.h
	mkdir -p build
	$(CXX) $(FLAGS) -c $< -o $@

src/matchit.h:
	wget https://raw.githubusercontent.com/BowenFu/matchit.cpp/main/include/matchit.h -O $@

clean:
	rm -rf build
	rm -rf rewind
