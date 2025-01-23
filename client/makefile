CFLAGS:=-c -Wall -Weffc++ -g -std=c++11 -Iinclude
LDFLAGS:=-lboost_system -lpthread
# TODO change the makefile for StompEMIlient
all: StompWCIClient

bin/ConnectionHandler.o: src/ConnectionHandler.cpp
	g++ $(CFLAGS) -o bin/ConnectionHandler.o src/ConnectionHandler.cpp

bin/echoClient.o: src/echoClient.cpp
	g++ $(CFLAGS) -o bin/echoClient.o src/echoClient.cpp

bin/event.o: src/event.cpp
	g++ $(CFLAGS) -o bin/event.o src/event.cpp

bin/StompClient.o: src/StompClient.cpp
	g++ $(CFLAGS) -o bin/StompClient.o src/StompClient.cpp

# Added
bin/StompProtocol.o: src/StompProtocol.cpp include/StompProtocol.h include/ConnectionHandler.h
	g++ $(CFLAGS) -o bin/StompProtocol.o src/StompProtocol.cpp

bin/KeyboardInput.o: src/KeyboardInput.cpp include/KeyboardInput.h
	g++ $(CFLAGS) -o bin/KeyboardInput.o src/KeyboardInput.cpp

StompWCIClient: bin/ConnectionHandler.o bin/StompClient.o bin/event.o bin/StompProtocol.o bin/KeyboardInput.o
	g++ -o bin/StompWCIClient bin/ConnectionHandler.o bin/StompClient.o bin/event.o bin/StompProtocol.o bin/KeyboardInput.o $(LDFLAGS)

.PHONY: clean
# Delete all files in the bin/ directory except StompESClient
clean:
	find bin -type f ! -name "StompESClient" -delete 
	