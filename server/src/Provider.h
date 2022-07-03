#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include "../third/json.hpp"

namespace jrRPC
{
	class Provider
	{
	private:
		using ProcType = std::function<nlohmann::json(const nlohmann::json&)>;
		std::unordered_map<std::string, ProcType> _procList;

	private:
		Provider() = default;

		/* Create a remote call with json format from a original function */
		template<typename Ret, typename... Args, std::size_t... N>
		ProcType _addRemoteProcedureHelper(std::function<Ret(Args...)> f, std::index_sequence<N...>)
		{
			return [=](const nlohmann::json& parameters)->nlohmann::json
			{
				return f(parameters[N].get<typename std::decay<Args>::type>()...);
			};
		}

	public:
		static Provider& instance();

		/* Add Procedure */
		template<typename Ret, typename ... Args>
		void addRemoteProcedure(const std::string& name, std::function<Ret(Args...)> f)
		{
			this->_procList[name] = this->_addRemoteProcedureHelper(f, std::index_sequence_for<Args...>{});
		}

		/* Do proc call */
		std::string callProc(const std::string& proc);
	};
}
