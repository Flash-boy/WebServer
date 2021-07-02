CXX ?= g++
DEBUG ?= 1
ifeq ($(DEBUG),1)
	CXXFLAGS += -g
else
	CXXFLAGS += -O2
endif
CXXFLAGS += -std=c++11
server: main.cpp ./lock/locker.cpp ./log/log.cpp ./timer/list_timer.cpp ./thread_pool/thread_pool.h ./http/http_conn.cpp  ./CGImysql/sql_connection_pool.cpp web_server.cpp config.cpp  
	$(CXX) -o server $^ $(CXXFLAGS) -lpthread -lmysqlclient
clean:
	rm -r server
