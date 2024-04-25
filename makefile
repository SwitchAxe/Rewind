CXX     = g++
FLAGS   = -std=c++20 -I. -lreadline -ggdb -ltinfo
OUT     = rewind
SRC     = src/main.cpp
LIBS    = src/*.hpp src/builtins/*.hpp
SHLIBS  = src/shell/*.hpp
OBJ     = build/main.o

.PHONY: clean install

$(OUT): $(OBJ)
	$(CXX) $(FLAGS) $^ -o $@

$(OBJ): $(SRC) $(LIBS) $(SHLIBS)
	mkdir -p build
	$(CXX) $(FLAGS) -c $< -o $@

clean:
	rm -rf build
	rm -rf rewind

install:
ifneq ($(shell id -u),0)
	install rewind ~/.local/bin/rewind
else
	install rewind /usr/bin/rewind
endif
