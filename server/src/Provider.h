#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include "../third/json.hpp"

namespace jrRPC
{
	enum class ErrorCode : std::uint16_t
	{
		Normal = 0x01,
		Exception = 0x02,
		SegmantFault = 0x03
	};

	void handleRemoteCallException();

	void handleRemoteCallSegmantFault();

	class Provider
	{
	private:
		using ProcType = std::function<nlohmann::json(const nlohmann::json&)>;
		std::unordered_map<std::string, ProcType> _procList;

	private:
		Provider();

		/* Create a non-member function call with json format from a original function */
		template<typename Ret, typename... Args, std::size_t... N>
		ProcType _addNonMemberFuncHelper(Ret(*f)(Args...), std::index_sequence<N...>)
		{
			return [f](const nlohmann::json& parameters)->nlohmann::json
			{
				return f(parameters[N].get<typename std::decay<Args>::type>()...);
			};
		}

		/* Register non-member function */
		template<typename Ret, typename ... Args>
		void _addNonMemberFunc(const std::string& name, Ret(*f)(Args...))
		{
			this->_procList[name] = this->_addNonMemberFuncHelper(f, std::index_sequence_for<Args...>{});
		}

		/* Register member function */
		template<typename C, typename Ret, typename ... Args>
		void _addMemberFunc(const std::string& name, Ret(C::*f)(Args...))
		{
			return;
		}

	public:
		static Provider& instance();

		/* Do proc call */
		std::string callProc(const std::string& proc);
	};
}
