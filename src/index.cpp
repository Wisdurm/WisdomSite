#include "crow.h"
#include "markdown.hpp"
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
    std::string dailyMsg = "Bruh";
    std::vector<std::string> blogPosts; // Array of blog posts
    std::vector<std::string> postNames = {"Example"}; // It's more convenient to have two vectors instead of a class because of how often they're used

    CROW_ROUTE(app, "/")([&dailyMsg]
        (const crow::request& req){       
	    auto page = crow::mustache::load("index.html");
        crow::mustache::context ctx({{"msg-daily", dailyMsg}});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/projects")([&dailyMsg]
        (const crow::request& req){       
	    auto page = crow::mustache::load("projects.html");
        crow::mustache::context ctx({{"msg-daily", dailyMsg}});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/contact")([&dailyMsg]
        (const crow::request& req){       
	    auto page = crow::mustache::load("contact.html");
        crow::mustache::context ctx({{"msg-daily", dailyMsg}});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/blog")([&dailyMsg, &postNames]
        (const crow::request& req){       
	    auto page = crow::mustache::load("blog.html");
        crow::mustache::context ctx;
        ctx["msg-daily"] = dailyMsg;
        for (int i = 0; i < postNames.size(); i++)   
        {
            ctx["posts"][i] = postNames.at(i);
        }
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/blog/<string>")([&dailyMsg]
        (const crow::request& req, std::string str){
	    auto page = crow::mustache::load_unsafe("blogPost.html");
        crow::mustache::context ctx({ {"msg-daily", dailyMsg}, {"blogText", "<h1>pippeli</h1>"}});
        //TODO
        // Read a list of MD files, and use those for this page
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/send")
	    .methods("GET"_method, "POST"_method)([&dailyMsg, &pass](const crow::request& req, crow::response& res){
	    if (req.url_params.get("pass")==pass)
		    dailyMsg = process_text(req.url_params.get("msg"));
        res.redirect("/");
        res.end();
	});

    app.port(18080).multithreaded().run();
}
