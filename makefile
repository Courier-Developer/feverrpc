INCLUDE=-Iinclude
MSGPACK=-Imsgpack/include

%.o:tests/%.cpp
	g++ -c $< ${MSGPACK} ${INCLUDE} --std=c++17

%.o:src/%.cpp
	g++ -c $< ${MSGPACK} ${INCLUDE} --std=c++17 

all:bin server client 

.PHONY:clean
clean:
	-rm *.o
	-rm -r bin

server:server.o feverrpc.o threadmanager.o utils.o
	g++ $^ -o bin/server -pthread 

client:client.o feverrpc.o threadmanager.o utils.o
	g++ $^ -o bin/client -pthread

bin:
	mkdir bin

copy:
	bash scripts/copy.sh