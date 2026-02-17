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
#include <ctime>
#include "pugixml.hpp"
#include <format>

std::string process_text(std::string str)
{
    if (str.length() > 100)
        str.erase(100);
    return str;
}

std::string getMotd(std::string& daily, std::vector<std::string>& motdBackup, time_t& lastUpdate)
{
	time_t now;
	time(&now);
	// If message of the day is set, use it.
	if (daily != "") {
		CROW_LOG_INFO << "Hours since last update: " << ((now - lastUpdate)/60.0/60);
		if (((now - lastUpdate)/60.0/60) > 30) { // If more than 30 hours has passed, reset the daily word
			lastUpdate = now;
			daily= "";
		}
		else {
			return daily;
		}
	}
 	// Otherwise, fallback to older ones
	long day = now/60/60/24; // How many days since Jan 1 1900
	return motdBackup.at(day % motdBackup.size());
}

// Turns xml item into a <card-component> element
inline std::string htmlCard(pugi::xml_node item) {
	std::string html = std::format("<card-component id='{0}'>"
			"<div slot='name'>{0}</div>"
			"<div slot='text'>{1}</div>"
			"<img slot='image' src='{2}' class='{3}'>"
			"<span slot='links'>",
			item.attribute("name").as_string(),
			item.attribute("desc").as_string(),
			item.attribute("img").as_string(),
			item.attribute("imgform").as_string()
		);
	// Links need to be parsed seperately :DDDDDDDDDDDDDDDDDD
	auto links = item.children("link");
	for (pugi::xml_named_node_iterator it = links.begin(); it != links.end();) {
		html += std::format("<a class='spanlink' href='{1}'>{0}</a>", it->child_value(), it->attribute("href").as_string());
		if (++it != links.end()) { // TODO: does i++ iterate it?
			html += " • ";
		}
	}
	html += "</span> </card-component>";
	return html;
}

int main()
{
    crow::SimpleApp app;
    // Password things
    const std::string salt = "mirri"; // Password salt
    const std::string pass = "$2a$12$VZOmbvUUaMNmafKN3nynAuZtlJ6SKLJrB25G3Ssm/zFPtFbr8owGG"; // TODO: Maybe should be in .env file? Idk
    // Message of the day (motd)
    std::string dailyMsg = "";
    std::vector<std::string> motdBackup; // Old motds to use in the abscence of a new one
    time_t lastUpdate; // Time since motd last updated
    time(&lastUpdate);
    // Xml
    pugi::xml_document pdoc;
    std::vector<std::string> items; // yadadadadada blah blah blah lalalalalalala
    std::string projectPage = ""; // Just straight up store it here because why would I parse it everytime
    std::string projectLinks = "";
    // Blogs
    std::map<std::string, std::string> blogPosts = {};
    const char* blogPath = "blog";

    CROW_ROUTE(app, "/")([&dailyMsg, &motdBackup, &lastUpdate, &pdoc, &items]
        (const crow::request& req){      

		// Project of the day
		time_t now;
		time(&now);
		long day = now/60/60/24; // How many days since Jan 1 1900
		std::string potd = items.at(day % items.size()); // TODO: Randomization
		//
	    auto page = crow::mustache::load("index.html");
        crow::mustache::context ctx({{"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)}, {"project-daily", potd}});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/projects")([&dailyMsg, &motdBackup, &lastUpdate, &projectPage, &projectLinks]
        (const crow::request& req){       

		// 
	    auto page = crow::mustache::load("projects.html");
        crow::mustache::context ctx({
			{"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)},
			{"projects-text", projectPage},
			{"projects-links", projectLinks}
		});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/contact")([&dailyMsg, &motdBackup, &lastUpdate]
        (const crow::request& req){       

	    auto page = crow::mustache::load("contact.html");
        crow::mustache::context ctx({{"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)}});
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/admin")([&lastUpdate]
        (const crow::request& req){       

		time_t now;
		time(&now);
	    auto page = crow::mustache::load("motd.html");
        crow::mustache::context ctx({{"time-since", (now - lastUpdate)/60/60 }}); // Hours since last update
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/blog")([&dailyMsg, &motdBackup, &lastUpdate, &blogPosts]
        (const crow::request& req){       

	    auto page = crow::mustache::load("blog.html");
        crow::mustache::context ctx;
        ctx["msg-daily"] = getMotd(dailyMsg, motdBackup, lastUpdate);
        int i = 0;
        std::map<std::string, std::string>::reverse_iterator it;
        for (it = blogPosts.rbegin(); it != blogPosts.rend(); it++)   
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
        std::map<std::string, std::string>::iterator it = blogPosts.find(str);
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
	    .methods("GET"_method, "POST"_method)([&dailyMsg, &pass, &salt, &motdBackup, &lastUpdate](const crow::request& req, crow::response& res){

		const char* r = req.url_params.get("pass");
		if (r != nullptr)
		{
        	const std::string url_pass = r;
			if (BCrypt::validatePassword(url_pass + salt, pass))
			{
				dailyMsg = process_text(req.url_params.get("msg"));
				motdBackup.push_back(dailyMsg);
				// Update date
				time(&lastUpdate);
				// Write dailymsg to file in case it's funny :D
				std::ofstream motdFile;
				motdFile.open("motd.txt", std::fstream::app);
				if (motdFile.fail())
				{
					CROW_LOG_CRITICAL << "Unable to open motd.txt file";
				}
				else {
					motdFile << "\n" << dailyMsg;
					motdFile.close();
				}
			}
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
	    CROW_LOG_INFO << "Loading file: " << entry.path();
	    std::ifstream file;
	    // Open file
	    file.open(entry.path(), std::fstream::in);
	    if (file.fail())
	    {
		    CROW_LOG_CRITICAL << "Unable to open file " << entry.path();
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
	    CROW_LOG_CRITICAL << "Unable to open motd.txt file";
	    return EXIT_FAILURE;
    }
    std::string text;
    while (getline(motdFile, text)) { // Copy all lines to motdBackup, &lastUpdate
	    motdBackup.push_back(text);
    }
    motdFile.close();
    // Open xml document
    pugi::xml_parse_result result = pdoc.load_file("projects.xml");
    if (!result) {
	    CROW_LOG_CRITICAL << "Unable to open projects.xml file";
	    return EXIT_FAILURE;
    }
    // Parse items
    for (pugi::xml_node cat : pdoc.child("projects"))
    {
	    // Sub title for links
	    projectLinks += std::format("<strong>{0}</strong>",
					cat.attribute("name").as_string());
	    // Make sections for each category
	    projectPage += std::format("<section> <h2 id='{0}'>{0}</h2> <p>{1}</p>"
				       "<div class='card-gallery'>",
				       cat.attribute("name").as_string(),
				       cat.attribute("desc").as_string()
		    ); // Cat info
	    // Items
	    for (pugi::xml_node item : cat.children("item")) {
		    // Items
		    items.push_back(htmlCard(item));
		    projectPage += items.back();
		    // Links
		    projectLinks += std::format("<a href='{0}'>{1}</a>",
						// Get first link
						item.children("link").begin()->attribute("href").as_string(),
						item.attribute("name").as_string());
	    }
	    projectPage += "</div></section>";
    }
    // Other stuff finished, start server
    app.port(18080).multithreaded().run();
}
