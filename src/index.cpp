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
    const std::string pass = "MustanKissanPaksutPosket"; // This is DEFINITELY NOT secure, but it's GOOD ENOUGH for THE TIME BEING
    std::string text = "Bruh";

    CROW_ROUTE(app, "/")([&text]
        (const crow::request& req){       
	    auto page = crow::mustache::load("index.html");
        crow::mustache::context ctx({{"msg-daily", text}});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/projects")([&text]
        (const crow::request& req){       
	    auto page = crow::mustache::load("projects.html");
        crow::mustache::context ctx({{"msg-daily", text}});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/contact")([&text]
        (const crow::request& req){       
	    auto page = crow::mustache::load("contact.html");
        crow::mustache::context ctx({{"msg-daily", text}});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/blog")([&text]
        (const crow::request& req){       
	    auto page = crow::mustache::load("blog.html");
        crow::mustache::context ctx({{"msg-daily", text}});
        //TODO
        // Read a list of MD files, and use those for this page
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/send")
	    .methods("GET"_method, "POST"_method)([&text, &pass](const crow::request& req, crow::response& res){
	    if (req.url_params.get("pass")==pass)
		    text = process_text(req.url_params.get("msg"));
        res.redirect("/");
        res.end();
	});

    app.port(18080).multithreaded().run();
}
