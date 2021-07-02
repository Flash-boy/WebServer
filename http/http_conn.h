#ifndef WEBSERVER_HTTP_HTTP_CONN_H_
#define WEBSERVER_HTTP_HTTP_CONN_H_
#include "../CGImysql/sql_connection_pool.h"
#include <arpa/inet.h>
#include <string>
#include <mysql/mysql.h>
#include <sys/types.h>
#include <sys/stat.h>
namespace TinyWebServer
{
class HttpConn
{
public:
	static const int FILENAME_LEN=200;
	static const int READ_BUFFER_SIZE = 2048;
	static const int WRITE_BUFFER_SIZE = 1024;
	enum METHOD
	{
		GET = 0,
		POST,
		HEAD,
		PUT,
		DELETE,
		TRACE,
		OPTIONS,
		CONNECT,
		PATH
	};
	enum CHECK_STATE
	{
		CHECK_STATE_REQUESTLINE = 0,
		CHECK_STATE_HEADER,
		CHECK_STATE_CONTENT
	};
	enum HTTP_CODE
	{
		NO_REQUEST,
		GET_REQUEST,
		BAD_REQUEST,
		NO_RESOURCE,
		FORBIDDEN_REQUEST,
		FILE_REQUEST,
		INTERNAL_ERROR,
		CLOSED_CONNECTION
	};
	enum LINE_STATUS
	{
		LINE_OK = 0,
		LINE_BAD,
		LINE_OPEN
	};
public:
	HttpConn(){}
	~HttpConn(){}
public:
	void Init(int sockfd,const sockaddr_in& addr,char*root,int trig_mode,int close_log,std::string user,std::string passwd,std::string sql_name);
	void CloseConn(bool real_close = true);
	void Process();
	bool ReadOnce();
	bool Write();
	sockaddr_in* GetAddress()
	{
		return &address_;
	}
	void InitMysqlResult(ConnectionPool* conn_pool);
	int time_flag_;
	int improv_;
private:
	void Init();
	HTTP_CODE ProcessRead();
	bool ProcessWrite(HTTP_CODE ret);
	HTTP_CODE ParseRequestLine(char* text);
	HTTP_CODE ParseHeaders(char* text);
	HTTP_CODE ParseContent(char* text);
	HTTP_CODE DoRequest();
	LINE_STATUS ParseLine();
	char *GetLine() { return read_buf_ + start_line_; }
	void UnMap();
	bool AddResopnse(const char* format,...);
	bool AddContent(const char* content);
	bool AddStatusLine(int status,const char* title);
	bool AddHeaders(int content_length);
	bool AddContentType();
	bool AddContentLength(int content_length);
	bool AddLinger();
	bool AddBlankLine();
public:
	static int epollfd_;
	static int user_count_;
	MYSQL* mysql_;
	int state_; // 读0，写1
private:
	int sockfd_;
	sockaddr_in address_;

	char read_buf_[READ_BUFFER_SIZE];
	int read_index_;
	int checked_index_;
	int start_line_;

	char write_buf_[WRITE_BUFFER_SIZE];
	int write_index_;

	CHECK_STATE check_state_;
	METHOD method_;
	
	char real_file_[FILENAME_LEN];
	char* url_;
	char* version_;
	char* host_;
	int content_length_;
	bool linger_;
	char* file_address_;
	struct stat file_stat_;
	struct iovec iv_[2];
	int iv_count_;
	int cgi_;
	char* request_body_string_;
	
	int bytes_to_send_;
	int bytes_have_send_;

	char* doc_root_;
	int trig_mode_;
	int close_log_;
	char sql_user_[100];
	char sql_passwd_[100];
	char sql_name_[100];

	
};
}

#endif

