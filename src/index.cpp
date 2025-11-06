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
#include "bcrypt/BCrypt.hpp"
#include <cmath>

std::string process_text(std::string str)
{
    if (str.length() > 100)
        str.erase(100);
    return str;
}

std::string getMotd(std::string& daily, std::vector<std::string>& motdBackup, time_t& lastUpdate)
{
	// If message of the day is set, use it.
	if (daily != "")
		return daily;
 	else // Otherwise, fallback to older ones
	{
		return motdBackup.at(0);
	}
}

int main()
{
    crow::SimpleApp app;
	const std::string salt = "mirri"; // Password salt
    const std::string pass = "$2a$12$VZOmbvUUaMNmafKN3nynAuZtlJ6SKLJrB25G3Ssm/zFPtFbr8owGG"; // TODO: Maybe should be in .env file? Idk

    std::string dailyMsg = "";
	std::vector<std::string> motdBackup; // Old motds to use in the abscence of a new one
	time_t lastUpdate; // Time since motd last updated
	time(&lastUpdate);

    std::unordered_map<std::string, std::string> blogPosts = {};
    const char* blogPath = "blog";

    CROW_ROUTE(app, "/")([&dailyMsg, &motdBackup, &lastUpdate]
        (const crow::request& req){      

	    auto page = crow::mustache::load("index.html");
        crow::mustache::context ctx({{"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)}});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/projects")([&dailyMsg, &motdBackup, &lastUpdate]
        (const crow::request& req){       

	    auto page = crow::mustache::load("projects.html");
        crow::mustache::context ctx({{"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)}});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/contact")([&dailyMsg, &motdBackup, &lastUpdate]
        (const crow::request& req){       

	    auto page = crow::mustache::load("contact.html");
        crow::mustache::context ctx({{"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)}});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/blog")([&dailyMsg, &motdBackup, &lastUpdate, &blogPosts]
        (const crow::request& req){       

	    auto page = crow::mustache::load("blog.html");
        crow::mustache::context ctx;
        ctx["msg-daily"] = getMotd(dailyMsg, motdBackup, lastUpdate);
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

    CROW_ROUTE(app, "/blog/<string>")([&dailyMsg, &motdBackup, &lastUpdate, &blogPosts]
    (const crow::request& req, crow::response& res, std::string str) {

        str += ".md";
        std::unordered_map<std::string, std::string>::iterator it = blogPosts.find(str);
        if (it != blogPosts.end()) // Exists
        {
	        auto page = crow::mustache::load("blogPost.html");
            crow::mustache::context ctx({ {"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)}, {"blogText", parse(blogPosts.at(str))} });
	        res.body = page.render(ctx).body_;
        }
        else // Blog post does not exist
        {
            res.redirect("/404");
        }
        res.end();
    });

    CROW_ROUTE(app, "/send")
	    .methods("GET"_method, "POST"_method)([&dailyMsg, &pass, &salt](const crow::request& req, crow::response& res){

        const std::vector<std::string>& keys = req.url_params.keys();
        
        if (std::find(keys.begin(), keys.end(), "pass") != keys.end() and BCrypt::validatePassword(req.url_params.get("pass") + salt, pass))
        {
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
	// Motd
	std::ifstream motdFile;
	motdFile.open("motd.txt", std::fstream::in);
	if (motdFile.fail())
	{
		std::cout << "Unable to open motd.txt file";
		return EXIT_FAILURE;
	}
	std::string text;
	while (getline(motdFile, text)) { // Copy all lines to motdBackup, &lastUpdate
		motdBackup.push_back(text);
	}
	motdFile.close();

    app.port(18080).multithreaded().run();
}
