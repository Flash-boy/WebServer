// 连接池保证，静态大小，互斥锁保证线程安全

#ifndef WEBSERVER_CGIMYSQL_SQL_CONNECTION_POOL_H_
#define WEBSERVER_CGIMYSQL_SQL_CONNECTION_POOL_H_

#include "../lock/locker.h"
#include <mysql/mysql.h>
#include <string>
#include <list>

namespace TinyWebServer
{

class ConnectionPool
{
public:
	MYSQL* GetConnection();
	bool ReleaseConnection(MYSQL* conn);
	int GetFreeConn();
	void DestoryPool();
	// 单例模式
	static ConnectionPool* GetInstance();
	void Init(std::string url,std::string user,std::string password,std::string database_name,int port,int max_conn,int close_log);
private:
	ConnectionPool();
	~ConnectionPool();
	int max_conn_;
	int current_conn_;
	int free_conn_;
	Locker lock_;
	std::list<MYSQL*> conn_list_;
	Semaphore reserve_;
public:
	std::string url_;
	std::string port_;
	std::string user_;
	std::string password_;
	std::string database_name_;
	int close_log_;
};

class ConnectionRAII
{
public:
	ConnectionRAII(MYSQL** conn,ConnectionPool* conn_pool);
	~ConnectionRAII();
private:
	MYSQL* connRAII;
	ConnectionPool* poolRAII;
};

}

#endif
