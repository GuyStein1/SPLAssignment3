CFLAGS:=-c -Wall -Weffc++ -g -std=c++11 -Iinclude
LDFLAGS:=-lboost_system -lpthread

all: StompEMIClient

bin/ConnectionHandler.o: src/ConnectionHandler.cpp
	g++ $(CFLAGS) -o bin/ConnectionHandler.o src/ConnectionHandler.cpp

bin/event.o: src/event.cpp
	g++ $(CFLAGS) -o bin/event.o src/event.cpp

bin/StompProtocol.o: src/StompProtocol.cpp
	g++ $(CFLAGS) -o bin/StompProtocol.o src/StompProtocol.cpp

bin/keyboardInput.o: src/keyboardInput.cpp
	g++ $(CFLAGS) -o bin/keyboardInput.o src/keyboardInput.cpp

bin/StompClient.o: src/StompClient.cpp src/StompProtocol.cpp src/ConnectionHandler.cpp src/keyboardInput.cpp
	g++ $(CFLAGS) -o bin/StompClient.o src/StompClient.cpp

StompEMIClient: bin/ConnectionHandler.o bin/StompClient.o bin/event.o bin/StompProtocol.o bin/keyboardInput.o
	g++ -o bin/StompEMIClient bin/ConnectionHandler.o bin/StompClient.o bin/event.o bin/StompProtocol.o bin/keyboardInput.o $(LDFLAGS)

.PHONY: clean
# Delete all files in the bin/ directory except StompESClient 
clean:
	find bin -type f ! -name "StompESClient" -delete 
	