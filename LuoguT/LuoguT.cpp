﻿#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define reqok "HTTP/1.0 200 OK\r\n\r\n"

#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <map>
#include <winsock2.h>
#include <shellapi.h>
#pragma comment(lib,"ws2_32.lib")
using namespace std;
using namespace boost;
char chs[100000];

namespace tools {
	vector<string> Split(const string& str, const string& pattern) {
		vector<string> res;
		if (str == "")
			return res;
		string strs = str + pattern;
		size_t pos = strs.find(pattern);
		while (pos != strs.npos) {
			string temp = strs.substr(0, pos);
			res.push_back(temp);
			strs = strs.substr(pos + 1, strs.size());
			pos = strs.find(pattern);
		}
		return res;
	}
	map<string, string>& GetHttpMessage(map<string, string>& re, string Res) {
		istringstream istr(Res);
		istr >> re["Method"] >> re["Mes"] >> re["Http"];
		if (re["Method"] == "GET")
			if (auto In = Split(re["Mes"], "?"); In.size() >= 1)
				if (re["Url"] = In[0]; In.size() >= 2)
					re["Par"] = In[1];
		return re;
	}

}

namespace Client {
	template<char* buf>
	class HttpConnect {
	public:
		std::string State;
		HttpConnect() {
			WSADATA wsa = { 0 };
			WSAStartup(MAKEWORD(2, 2), &wsa);
		};
		~HttpConnect() {};
		const char* socketHttp(std::string host, std::string request) {
			int sockfd;
			struct sockaddr_in address;
			struct hostent* server;
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			address.sin_family = AF_INET;
			address.sin_port = htons(80);
			server = gethostbyname(host.c_str());
			memcpy((char*)&address.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
			if (-1 == connect(sockfd, (struct sockaddr*)&address, sizeof(address))) {
				State = "connection error!";
				return "connection error!";
			}
			State = request;
			send(sockfd, request.c_str(), request.size(), 0);
			int offset = 0;
			int rc;
			while (rc = recv(sockfd, buf + offset, 1024, 0))
				offset += rc;
			closesocket(sockfd);
			buf[offset] = 0;
			return buf;
		}
		const char* postData(std::string host, std::string path, std::string post_content) {
			std::stringstream stream;
			stream << "POST " << path;
			stream << " HTTP/1.0\r\n";
			stream << "Host: " << host << "\r\n";
			stream << "User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN; rv:1.9.2.3) Gecko/20100401 Firefox/3.6.3\r\n";
			stream << "Content-Type:application/x-www-form-urlencoded\r\n";
			stream << "Content-Length:" << post_content.length() << "\r\n";
			stream << "Connection:close\r\n\r\n";
			stream << post_content.c_str();
			return socketHttp(host, stream.str());
		};
		const char* getData(std::string host, std::string path, std::string get_content) {
			std::stringstream stream;
			stream << "GET " << path << "?" << get_content;
			stream << " HTTP/1.0\r\n";
			stream << "Host: " << host << "\r\n";
			stream << "User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN; rv:1.9.2.3) Gecko/20100401 Firefox/3.6.3\r\n";
			stream << "Connection:close\r\n\r\n";
			return socketHttp(host, stream.str());
		}
	};
}

namespace Server {
	class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
	public:
		asio::const_buffer(*mytod)(string&);
		HttpConnection(asio::io_context& io, asio::const_buffer(*fun)(string&)) : socket_(io), mytod(fun) {}
		void Start() {
			auto p = shared_from_this();
			asio::async_read_until(socket_, asio::dynamic_buffer(request_), "\r\n\r\n",
				[p, this](const system::error_code& err, size_t len) {
					if (err)
						return cout << "recv err:" << err.message() << "\n", void(0);
					string first_line = request_.substr(0, request_.find("\r\n")); // should be like: GET / HTTP/1.0
					cout << first_line << "\n";
					asio::async_write(socket_, mytod(request_), [p, this](const system::error_code& err, size_t len) {
						socket_.close();
						});
				});
		}

		asio::ip::tcp::socket& Socket() { return socket_; }
	private:
		asio::ip::tcp::socket socket_;
		string request_;
	};
	class HttpServer
	{
	public:
		asio::const_buffer(*mytod)(string&);
		HttpServer(asio::io_context& io, asio::ip::tcp::endpoint ep, asio::const_buffer(*fun)(string&)) : io_(io), acceptor_(io, ep), mytod(fun) {}

		void Start() {
			auto p = std::make_shared<HttpConnection>(io_, mytod);
			acceptor_.async_accept(p->Socket(), [p, this](const system::error_code& err) {
				if (err)
					return cout << "accept err:" << err.message() << "\n", void(0);
				p->Start();
				Start();
				});
		}
	private:
		asio::io_context& io_;
		asio::ip::tcp::acceptor acceptor_;
	};
	inline void StartServer(string ip, short port, asio::const_buffer(*resfun)(string&)) {
		asio::io_context io;
		asio::ip::tcp::endpoint ep(asio::ip::make_address(/*argv[1] */ip), port);
		HttpServer hs(io, ep, resfun);
		hs.Start();
		io.run();
	}
}

asio::const_buffer todo(string& str) {
	map<string, string> Info;
	tools::GetHttpMessage(Info, str);
	if (Info["Par"].length() != 0) {
		Client::HttpConnect<chs> geter;
		geter.getData("mate.orlicz.top", Info["Url"], "");
		return asio::buffer(chs);
	}
	memcpy(chs, reqok, sizeof reqok);
	std::ifstream in(Info["Url"].substr(1));
	std::istreambuf_iterator<char> begin(in), end;
	std::string mystr(begin, end);
	memcpy(chs + sizeof reqok - 1, mystr.c_str(), mystr.length());
	chs[mystr.length() + sizeof reqok] = 0;
	cout << chs << mystr;
	return asio::buffer(chs);
}
int main(int argc, const char* argv[]) {
	(ofstream("Welcome.html") << R"(<!DOCTYPE html><html><head><meta http-equiv="content-type" content="text/html; charset=utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no"><title>WelCome!</title><link rel="stylesheet" href="https://orlicz.gitee.io/Demos/css/41.css"></head><body><h2><span>i</span>m<span>Helloing</span></h2></body></html>)").close();
	ShellExecute(NULL, L"open", L"http://127.0.0.1/Welcome.html", NULL, NULL, SW_SHOWDEFAULT);
	ShellExecute(NULL, L"open", L"http://127.0.0.1/?Web", NULL, NULL, SW_SHOWDEFAULT);
	return Server::StartServer("127.0.0.1", 80, todo), 0;
}