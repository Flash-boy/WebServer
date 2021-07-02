#include "sql_connection_pool.h"
#include "../log/log.h"
namespace TinyWebServer
{
	ConnectionPool::ConnectionPool()
	{
		current_conn_=0;
		free_conn_=0;
	}
	ConnectionPool::~ConnectionPool()
	{
		DestoryPool();
	}
	ConnectionPool* ConnectionPool::GetInstance()
	{
		static ConnectionPool conn_pool;
		return &conn_pool;
	}
	void ConnectionPool::Init(std::string url,std::string user,std::string password,std::string database_name,int port,int max_conn,int close_log)
	{
		url_=url;
		user_=user;
		password_=password;
		database_name_=database_name;
		port_=port;
		close_log_=close_log;

		for(int i=0;i<max_conn;i++)
		{
			MYSQL* conn=NULL;
			conn=mysql_init(conn);
			if(conn==NULL)
			{
				LOG_ERROR("MYSQL Error");
				exit(1);	
			}
			conn=mysql_real_connect(conn,url.c_str(),user.c_str(),password_.c_str(),database_name.c_str(),port,NULL,0);
			if(conn==NULL)
			{
				LOG_ERROR("MYSQL Error");
				exit(1);
			}
			conn_list_.push_back(conn);
			++free_conn_;
		}
		reserve_ = Semaphore(free_conn_);
		max_conn_=free_conn_;
		
	}

	MYSQL* ConnectionPool::GetConnection()
	{
		MYSQL* conn=NULL;
		if(conn_list_.size()==0)
			return NULL;
		reserve_.Wait();
		lock_.Lock();
		conn = conn_list_.front();
		conn_list_.pop_front();
		--free_conn_;
		++current_conn_;
		lock_.UnLock();
		return conn;
	}
	bool ConnectionPool::ReleaseConnection(MYSQL* conn)
	{
		if(conn==NULL)
			return false;
		lock_.Lock();
		conn_list_.push_back(conn);
		++free_conn_;
		--current_conn_;
		lock_.UnLock();
		reserve_.Post();
		return true;
	}
	void ConnectionPool::DestoryPool()
	{
		lock_.Lock();
		if(conn_list_.size()>0)
		{
			for(auto it=conn_list_.begin();it!=conn_list_.end();++it)
			{
				MYSQL* conn = *it;
				mysql_close(conn);
			}
			current_conn_=0;
			free_conn_=0;
		}
		lock_.UnLock();
	}
	int ConnectionPool::GetFreeConn()
	{
		return this->free_conn_;
	}

	// ConnectionRAII
	ConnectionRAII::ConnectionRAII(MYSQL** SQL,ConnectionPool* conn_pool)
	{
		*SQL=conn_pool->GetConnection();
		connRAII = *SQL;
		poolRAII = conn_pool;
	}
	ConnectionRAII::~ConnectionRAII()
	{
		poolRAII->ReleaseConnection(connRAII);
	}
}