#include "crow/http_request.h"
#include <array>
#include <crow.h>

#define PRETTY_LONG_TIME "86400"
#define SOMEWHAT_LONG_TIME "3600"

struct Cache
{
	struct context
	{};

	static constexpr auto permaCache = std::to_array({
			"tff",
			"otf",
			"jpg",
			"webp",
			"webp",
			"gif"
		});

	static constexpr auto longCache = std::to_array({
			"js",
			"css"
		});


	void before_handle(crow::request& req, crow::response& res, context& ctx)
	{
		for (const auto& ext : longCache) {
			if (req.url.ends_with(ext)) {
				res.add_header("Cache-Control",
					       "max-age=" SOMEWHAT_LONG_TIME);
				return;
			}
		}
		for (const auto& ext : permaCache) {
			if (req.url.ends_with(ext)) {
				res.add_header("Cache-Control",
					       "max-age=" PRETTY_LONG_TIME);
				return;
			}
		}

	}

	void after_handle(crow::request& req, crow::response& res, context& ctx)
	{}
};
