#include "crow/http_request.h"
#include "crow/logging.h"
#include <crow.h>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <chrono>
#include <vector>

struct SpamGuard
{
	using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

	struct Req {
		TimePoint t;
		int weight;
	};

	struct ConnectionInfo {
		int timeOuts;
		TimePoint firstConnection;
		std::optional<TimePoint> timeout;
		std::vector<Req> reqs;
	};

	inline static std::unordered_map<std::string, ConnectionInfo> memory;

	struct context
	{
		int getTrust(std::string ip) const
		{
			return memory.at(ip).timeOuts;
		}

		TimePoint getFirstConnTime(std::string ip) const
		{
			return memory.at(ip).firstConnection;
		}

		bool canTrust(std::string ip) const
		{
			const auto now = std::chrono::system_clock::now();
			const ConnectionInfo& conn = memory.at(ip);
			return now-conn.firstConnection > std::chrono::minutes(3);
		}
	};

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

		if (m->second.timeout) {
			if (now - m->second.timeout.value() < std::chrono::seconds(10)) {
				auto page = crow::mustache::load(
					"limbo.html");
				res.body = page.render_string();
				res.code = 403;
				res.end();
				return;
			}
			m->second.timeout = std::nullopt;
		}

		auto& reqs = m->second.reqs;

		// Weight request
		const bool newC = now - m->second.firstConnection
			< std::chrono::seconds(10);
		const int weight =
			req.url.ends_with("ttf") ? (newC ? 0 : 30)
			: req.url.ends_with("otf") ? (newC ? 0 : 30)
			: req.url.ends_with("jpg") ? 40
			: req.url.contains("components") ? 3
			: req.url.ends_with("js") ? 5
			: req.url.ends_with("css") ? 10
			: req.url.ends_with("webp") ? 3
			: req.url.ends_with("png") ? 5
			: req.url.ends_with("gif") ? 3
			: 15;

		reqs.push_back( {
				std::chrono::system_clock::now(),
				weight
			});

		std::erase_if(m->second.reqs, [&](Req r){
			return now - r.t > std::chrono::seconds(10);
		});

		// Timeout
		const int sum = std::accumulate(reqs.begin(), reqs.end(), 0,
						[&](const int sum, const Req& x) -> int
						{
							return sum + x.weight;
						});
		// CROW_LOG_INFO << "Sum: " << sum;
		// CROW_LOG_INFO << "Len: " << reqs.size();

		const int limit =
			now - m->second.firstConnection < std::chrono::seconds(30) ? 100
			: now - m->second.firstConnection < std::chrono::minutes(1) ? 150
			: now - m->second.firstConnection < std::chrono::minutes(10) ? 200
			: 250;

		if (sum > limit) {
			m->second.timeout = now;
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
