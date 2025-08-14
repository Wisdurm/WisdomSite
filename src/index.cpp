#include "crow.h"
#include <string>

std::string process_text(std::string str)
{
    if (str.length() > 100)
        str.erase(100);
    return str;
}

int main()
{
    crow::SimpleApp app;
    std::string text = "Bruh";

    CROW_ROUTE(app, "/")([&text]
        (const crow::request& req){
	    auto page = crow::mustache::load("begin.html");
        crow::mustache::context ctx({{"msg", text}});
        text += "a";
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/send")
	    .methods("GET"_method, "POST"_method)([&text](const crow::request& req, crow::response& res){
		text = process_text(req.url_params.get("msg"));
        res.redirect("/");
        res.end();
	});

    app.port(18080).multithreaded().run();
}
