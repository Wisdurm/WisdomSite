// Internal
#include "markdown.hpp"
// C++
#include <numeric>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>
#include <format>
#include <random>
// C
#include <cctype>
#include <ctime>
#include <cstdlib>
// External
#include "bcrypt/BCrypt.hpp"
#include "pugixml.hpp"
#include <sqlite3.h>
#include "crow.h"
#include "crow/logging.h"

static std::string process_text(std::string str)
{
    if (str.length() > 100)
        str.erase(100);
    return str;
}

static std::string getMotd(std::string& daily, std::vector<std::string>& motdBackup, time_t& lastUpdate)
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
static std::string htmlCard(pugi::xml_node item) {
	std::string html = std::format("<card-component id='{0}'>"
				       "<div slot='name'>{0}</div>"
				       "<div slot='text'>{1}</div>"
				       "<img slot='image' src='{2}' class='{3}' alt='{4}'>"
				       "<span slot='links'>",
				       item.attribute("name").as_string(),
				       item.attribute("desc").as_string(),
				       item.attribute("img").as_string(),
				       item.attribute("imgform").as_string(),
				       item.attribute("alt").as_string()
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

// Generate a list of web badge elements
static std::string webBadgeArray(const std::vector<std::pair<std::string, std::string>>& webBadges, int amount) {
	std::string result = "";
	// Random indeces
	std::vector<int> random(webBadges.size());
	std::iota(random.begin(), random.end(), 0);
	std::random_device rd;
	std::shuffle(random.begin(), random.end(), rd);
	const std::vector<int> indeces(random.begin(), random.begin() + amount);
	for (auto i : indeces) {
		const std::pair<std::string, std::string>& pair = webBadges[i];
		result += std::format("<a href='{1}' class='i88x31'>"
				      "<img src='/static/images/badges/{0}'/> </a>",
				      pair.first,
				      pair.second
			);
	}
	return result;
}

int main()
{
	// Seed randomness
	srand(time(0));
    crow::SimpleApp app;
    // Database
    sqlite3 *db;
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
    // Badges
    const std::vector<std::pair<std::string, std::string>> webBadges = {
	    {"powered-by-debian.gif", "https://debian.org"},
	    {"7zip.gif", "https://www.7-zip.org"},
	    {"winrar.gif", ""},
	    {"linux_now.gif", "https://kernel.org"},
	    {"emacs2.gif", "https://www.gnu.org/software/emacs"},
	    {"vim.gif", "https://www.vim.org/"},
	    {"any_browser.gif", ""}, // I'll come up with something later
	    {"powered.gif", ""},
	    {"blender.gif" , "https://www.blender.org/"},
	    {"cc-button.gif", ""},
	    {"firefox_now.png", "https://www.firefox.com/"},
	    {"best_viewed_with_eyes.gif", ""},
	    {"button38.gif", "https://github.com/Wisdurm/"}
    };

    CROW_ROUTE(app, "/")([&dailyMsg, &motdBackup, &lastUpdate, &pdoc, &items, &webBadges]
			 (const crow::request& req){
	    // Project of the day
	    time_t now;
	    time(&now);
	    long day = now/60/60/24; // How many days since Jan 1 1900
	    std::string potd = items.at(day % items.size()); // TODO: Randomization
	    //
	    auto page = crow::mustache::load("index.html");
	    crow::mustache::context ctx({
			    {"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)},
			    {"project-daily", potd},
			    {"badges", webBadgeArray(webBadges, 8)}
		    });
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/projects")([&dailyMsg, &motdBackup, &lastUpdate, &projectPage, &projectLinks, &webBadges]
				 (const crow::request& req){
	    // List of projects
	    auto page = crow::mustache::load("projects.html");
	    crow::mustache::context ctx({
			    {"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)},
			    {"projects-text", projectPage},
			    {"projects-links", projectLinks},
			    {"badges", webBadgeArray(webBadges, 8)}
		    });
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/contact")([&dailyMsg, &motdBackup, &lastUpdate]
				(const crow::request& req){       
	    // TODO: Contact form
	    auto page = crow::mustache::load("contact.html");
	    crow::mustache::context ctx({
			    {"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)}
		    });
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/admin")([&lastUpdate]
			      (const crow::request& req){       
	    // Admin page, plain
	    time_t now;
	    time(&now);
	    auto page = crow::mustache::load("motd.html");
	    crow::mustache::context ctx({{"time-since", (now - lastUpdate)/60/60 }}); // Hours since last update
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/blog")([&dailyMsg, &motdBackup, &lastUpdate, &blogPosts]
			     (const crow::request& req){       
	    // List of posts
	    auto page = crow::mustache::load("blog.html");
	    crow::mustache::context ctx;
	    ctx["msg-daily"] = getMotd(dailyMsg, motdBackup, lastUpdate);
	    // ctx["badges"] = webBadgeArray(webBadges, 8);
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
		    // Remove numbers since ol
		    ctx["posts"][i]["name"] = std::string(postName.begin() + 3, postName.end());
		    i++;
	    }
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/blog/<string>")([&dailyMsg, &motdBackup, &lastUpdate, &blogPosts]
				      (const crow::request& req, crow::response& res, std::string str) {
	    // Actual blog page
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
    .methods("GET"_method, "POST"_method)([&dailyMsg, &pass, &salt, &motdBackup, &lastUpdate]
					  (const crow::request& req, crow::response& res) {
	    // Set motd
	    if (const char* p = req.url_params.get("pass"),
		*m = req.url_params.get("msg"); // I don't like this
		p and m)
	    {
		    const std::string url_pass = p;
		    if (BCrypt::validatePassword(url_pass + salt, pass))
		    {
			    dailyMsg = process_text(m);
			    CROW_LOG_INFO << "Updated motd : \"" << dailyMsg << "\"";
			    motdBackup.push_back(dailyMsg);
			    // Update date
			    time(&lastUpdate);
			    // Write dailymsg to file in case it's funny :D
			    std::ofstream motdFile;
			    motdFile.open("motd.txt", std::fstream::app);
			    if (motdFile.fail()) {
				    CROW_LOG_CRITICAL << "Unable to open motd.txt file";
			    } else {
				    motdFile << "\n" << dailyMsg;
				    motdFile.close();
			    }
		    }
	    }
	    res.redirect("/");
	    res.end();
    });

    CROW_ROUTE(app, "/guestbook")([&dailyMsg, &motdBackup, &lastUpdate, &db]
				  (const crow::request& req) {
	    // Page for comments and form
	    auto page = crow::mustache::load("guestbook.html");
	    crow::mustache::context ctx({
			    {"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)}
		    });
	    // Get comments
	    struct comment {		    
		    const std::string msg;
		    const std::string name;
		    const int posted;
	    };
	    std::vector<comment> comments;
	    // TODO: I think some tricks allow skipping *data, they're just a
	    // tad too difficult for me to figure out at the moment
	    sqlite3_exec(db, "SELECT * FROM comments;",
			 [](void* data, int argc, char** argv, char** azColName) {
				 auto comments = static_cast<std::vector<comment>*>(data);
				 comment c{
					 .msg = std::string(argv[0]),
					 .name = std::string(argv[1]),
					 .posted = std::stoi(argv[2])
				 };
				 comments->push_back(c);
				 return 0;
			 }, &comments, NULL);
	    // Format comments
	    int i = 0;
	    for (auto c : comments) {
		    ctx["comments"][i]["msg"] = c.msg;
		    ctx["comments"][i]["name"] = c.name;
		    // Format unix time
		    char buffer[256];
		    const auto t = static_cast<time_t>(c.posted);
		    std::strftime(buffer, sizeof(buffer), "%d.%m.%Y", localtime(&t));
		    ctx["comments"][i]["posted"] = buffer;
		    i++;
	    }
	    return page.render(ctx);
    });

    CROW_ROUTE(app, "/comment")
	    .methods("GET"_method, "POST"_method)(
		    [&dailyMsg, &pass, &salt, &motdBackup, &lastUpdate, &db]
		    (const crow::request& req, crow::response& res) {
	    // Post comment
	    auto qs = req.get_body_params();
	    if (const char* n = qs.get("name"),
		*m = qs.get("msg");
		n and m)
	    {
		    const std::string name = n;
		    const std::string msg = m;
		    time_t posted = time(nullptr);
		    // Validation
		    if (name.size() == 0) {
			    // name len wrong
			    res.redirect("/guestbook?err=name");
			    goto EXIT;
		    }
		    if (msg.size() < 5 or
			msg.size() > 250) {
			    // msg len wrong
			    res.redirect("/guestbook?err=desc");
			    goto EXIT;
		    }
		    // Correct checkmarks
		    for (uint8_t i = 0; i < 8; i++) {
			    // bruh
			    const auto e = std::string("evil") +
				    std::string{static_cast<char>(i + '0')};
			    // Skip 6 because front end does aswell
			    if (i != 6 and not qs.get(e)) {
				    res.redirect("/guestbook?err=evil");
				    goto EXIT;
			    }
		    }
		    // Add to database
		    sqlite3_stmt* st;
		    int rc = sqlite3_prepare_v2(db,
						"INSERT INTO comments "
						"VALUES(?, ?, ?);",
						-1, &st, NULL);
		    CROW_LOG_DEBUG << "Prepare: " << rc;
		    if (rc != SQLITE_OK) {
			    CROW_LOG_ERROR << "SQL ERROR: " << rc;
			    rc = sqlite3_finalize(st); // I think?
			    res.redirect("/guestbook?err=internal");
			    goto EXIT;
		    }
		    // Add values to statement
		    sqlite3_bind_text(st, 1, msg.c_str(), -1, SQLITE_STATIC);
		    sqlite3_bind_text(st, 2, name.c_str(), -1, SQLITE_STATIC);
		    sqlite3_bind_int(st, 3, static_cast<int>(posted));
		    // Execute statement
		    rc = sqlite3_step(st);
		    CROW_LOG_DEBUG << "Step: " << rc;
		    rc = sqlite3_finalize(st);
		    CROW_LOG_DEBUG << "Finalize: " << rc;
		    // Success
		    res.redirect("/guestbook");
	    } else {
		    // Missing params
		    res.redirect("/guestbook?err=missing");
	    }
					  EXIT: // ???? emacs ?????
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
    // Initialize database connection
    int opened = sqlite3_open("db.db", &db);
    if (opened) {
	    CROW_LOG_CRITICAL << "Unable to establish database connection: " << sqlite3_errmsg(db);
	    sqlite3_close(db);
	    return EXIT_FAILURE;
    }
    // Other stuff finished, start server
    app.loglevel(crow::LogLevel::DEBUG);
    app.port(18080).multithreaded().run();
    // Post run cleanup
    std::cout << "[CLEANUP] Closing database connection\n";
    sqlite3_close(db);
    std::cout << "[CLEANUP] Closed\n";
}
