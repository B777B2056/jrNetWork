#pragma once
#include <string>
#include <memory>
#include <variant>
#include "../network/Socket.h"

namespace jrHTTP
{
	enum class HttpMethod { GET, POST };

	namespace HttpReqParser
	{
		struct Result
		{
			int retCode;
			HttpMethod method;
			std::string url;
			std::string content;
		};

		std::string buildReqResponse(int retCode, const std::string& content);
		Result parserReq(std::shared_ptr<jrNetWork::TCP::Socket> client);
	}
}
