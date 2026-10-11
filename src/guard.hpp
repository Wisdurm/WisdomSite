#include "crow/http_request.h"
#include <crow.h>
#include <unordered_map>
#include <chrono>
#include <vector>

struct SpamGuard
{
	using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

	struct ConnectionInfo {
		int timeOuts;
		TimePoint firstConnection;
		std::vector<TimePoint> reqs;
	};

	inline static std::unordered_map<std::string, ConnectionInfo> memory;

	struct context
	{};

	void before_handle(crow::request& req, crow::response& res, context& ctx)
	{
		const std::string addr = req.get_header_value("X-Real-IP");
		const auto now = std::chrono::system_clock::now();
		const auto m = memory.find(addr);

		if (m == memory.end()) {
			memory.insert({addr,
					{
						0,
							std::chrono::system_clock::now(),
						{}}
				});
			return;
		}

		m->second.reqs.push_back(std::chrono::system_clock::now());

		std::erase_if(m->second.reqs, [&](std::chrono::time_point<std::chrono::system_clock> t){
			return now-t > std::chrono::seconds(20);
		});

		// Timeout
		if ((now - m->second.firstConnection < std::chrono::minutes(1) and
		     m->second.reqs.size() > 50) or
		    (now - m->second.firstConnection < std::chrono::minutes(10) and
		     m->second.reqs.size() > 100) or
		    (now - m->second.firstConnection > std::chrono::minutes(10) and
		     m->second.reqs.size() > 200)) {
			m->second.timeOuts++;

			auto page = crow::mustache::load(
				"limbo.html");
			res.body = page.render_string();
			res.code = 403;
			res.end();
			return;
		}
	}

	void after_handle(crow::request& req, crow::response& res, context& ctx)
	{}
};
