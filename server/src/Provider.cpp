#include "Provider.h"

namespace jrRPC
{
	struct _ProcInfo
	{
		std::string name;
		nlohmann::json param;
	};

	/* Deserialization a received string */
	static _ProcInfo _deserialization(const std::string& proc)
	{
		_ProcInfo p;
		p.name = nlohmann::json::parse(proc).at("name");
		p.param = nlohmann::json::parse(proc).at("parameters");
		return p;
	}

	Provider& Provider::instance()
	{
		static Provider p;
		return p;
	}

	std::string Provider::callProc(const std::string& proc)
	{
		nlohmann::json msg;
		_ProcInfo p = _deserialization(proc);
		if (_procList.count(p.name) != 0)
		{
			msg["error"] = false;
			msg["return_val"] = _procList[p.name](p.param);
		}
		else
		{
			msg["error"] = true;
			msg["error_msg"] = "Remote proc " + p.name + " not found";
		}
		return msg;
	}
}
