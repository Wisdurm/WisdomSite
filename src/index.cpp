#include "crow.h"
#include "markdown.hpp"
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cctype>

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
    std::unordered_map<std::string, std::string> blogPosts = {};
    const char* blogPath = "blog";

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

    CROW_ROUTE(app, "/blog")([&dailyMsg, &blogPosts]
        (const crow::request& req){       

	    auto page = crow::mustache::load("blog.html");
        crow::mustache::context ctx;
        ctx["msg-daily"] = dailyMsg;
        int i = 0;
        std::unordered_map<std::string, std::string>::iterator it;
        for (it = blogPosts.begin(); it != blogPosts.end(); it++)   
        {
            // Links
            std::string postName = it->first.substr(0, it->first.find_last_of('.'));
            ctx["posts"][i]["link"] = postName;

            // Posts

            // Replace underscore with space
            std::transform(postName.begin(), postName.end(), postName.begin(),
                [](unsigned char c) { if (c == '_') return ' '; else return (char)c; });
            ctx["posts"][i]["name"] = postName;
            i++;
        }
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/blog/<string>")([&dailyMsg, &blogPosts]
    (const crow::request& req, crow::response& res, std::string str) {

        str += ".md";
        std::unordered_map<std::string, std::string>::iterator it = blogPosts.find(str);
        if (it != blogPosts.end()) // Exists
        {
	        auto page = crow::mustache::load("blogPost.html");
            crow::mustache::context ctx({ {"msg-daily", dailyMsg}, {"blogText", parse(blogPosts.at(str))} });
	        res.body = page.render(ctx).body_;
        }
        else // Blog post does not exist
        {
            res.redirect("/404");
        }
        res.end();
    });

    CROW_ROUTE(app, "/send")
	    .methods("GET"_method, "POST"_method)([&dailyMsg, &pass](const crow::request& req, crow::response& res){

        const std::vector<std::string>& keys = req.url_params.keys();
        
        if (std::find(keys.begin(), keys.end(), "pass") != keys.end())
        {
            if (req.url_params.get("pass") == pass) // For whaetever reason, these two conditionals don't "fit" in the same if statement :thinking:
                dailyMsg = process_text(req.url_params.get("msg"));
        }
        res.redirect("/");
        res.end();
	});

    CROW_CATCHALL_ROUTE(app)
        ([&dailyMsg](crow::response& res) {
        if (res.code == 404)
        {
            auto page = crow::mustache::load("404.html");
            crow::mustache::context ctx({ {"msg-daily", dailyMsg} });
	        res.body = page.render(ctx).body_;
        }
        else if (res.code == 405)
        {
            res.body = "The HTTP method does not seem to be correct.";
        }
        res.end();
    });

    // Find blog files
    for (const auto& entry : std::filesystem::directory_iterator(blogPath))
    {
        std::cout << "Loading file: " << entry.path() << std::endl;
        std::ifstream file;
        // Open file
        file.open(entry.path(), std::fstream::in);
        if (file.fail())
        {
            std::cout << "Unable to open file " << entry.path();
            return EXIT_FAILURE;
        }
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        std::string fileText(size, ' ');
        file.seekg(0);
        file.read(&fileText[0], size);
        file.close();

        std::string filePath = entry.path().string();
        blogPosts[filePath.substr(filePath.find_last_of("/\\") + 1)] = fileText; // File name
    }

    app.port(18080).multithreaded().run();
}
