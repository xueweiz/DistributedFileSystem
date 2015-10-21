
#C compiler
CC = g++

SRC = main.cpp connections.cpp ChronoCpu.cpp Chrono.cpp

CC_FLAGS = -pthread -std=c++11

EXE = membership

release:$(SRC)
	$(CC)    $(SRC) $(CC_FLAGS) -o $(EXE) 

clean: $(SRC)
	rm -f $(EXE) $(EXE_X) $(EXE).linkinfo 
