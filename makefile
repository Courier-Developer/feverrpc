server:server.cpp feverrpc.cpp lock.cpp utils.cpp
	g++ server.cpp -o server -I msgpack/include --std=c++17  -pthread
client:client.cpp feverrpc.cpp lock.cpp utils.cpp
	g++ client.cpp -o client -I msgpack/include --std=c++17  -pthread
thread:thread-test.cpp
	g++ thread-test.cpp -o thread-test -pthread
lock:lock.cpp
	g++ lock.cpp -o lock -pthread
