// Internal
#include "SQLiteCpp/Transaction.h"
#include "markdown.hpp"
// C++
#include <array>
#include <exception>
#include <numeric>
#include <string>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>
#include <format>
#include <random>
#include <chrono>
// C
#include <cctype>
#include <cstdlib>
// External
#include "../../libbcrypt-src/include/bcrypt/BCrypt.hpp"
#include "pugixml.hpp"
#include <sqlite3.h>
#include "crow/app.h"
#include "crow/http_request.h"
#include "crow/http_response.h"
#include "crow/json.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include "SQLiteCpp/Database.h"
#include "SQLiteCpp/Statement.h"
#include "crow/logging.h"
#include "crow/query_string.h"

// Source - https://stackoverflow.com/a/60337372
// Posted by Tomáš Zato
// Retrieved 2026-05-28, License - CC BY-SA 4.0
struct xml_string_writer: pugi::xml_writer
{
	std::string result;
	
	virtual void write(const void* data, size_t size)
	{
		result.append(static_cast<const char*>(data), size);
	}
};

static std::string InnerXML(pugi::xml_node target)
{
	xml_string_writer writer;
	target.print(writer, "");
	return writer.result;
}

static crow::json::wvalue npestaQuip(SQLite::Database& dbNpesta) noexcept
{
	try {
		// Get message count
		SQLite::Statement cquery (dbNpesta,
					  "SELECT count(id) FROM quips;");
		cquery.executeStep();
		const int count = cquery.getColumn(0);
		// Get random message
		std::default_random_engine rde {std::random_device{}()};
		const int index = std::uniform_int_distribution<int>(1, count)(rde);
		SQLite::Statement mquery(dbNpesta,
					 "SELECT msg FROM quips "
					 "WHERE id=?;");
		mquery.bind(1, index);
		mquery.executeStep();
		const std::string message = mquery.getColumn(0).getString();
		return crow::json::wvalue{{"success", true}, {"message", message}, {"index", index}};
	} catch (std::exception& e) {
		CROW_LOG_ERROR << e.what();
		return crow::json::wvalue{{"success", false}, {"message", "Server error"}};
	}
}

// Turns xml item into a wvalue
static crow::json::wvalue toCard(pugi::xml_node item) {
	crow::json::wvalue json;
	static constexpr auto params = std::to_array<std::string>({
			"name",
			"desc",
			"img",
			"imgform",
			"alt"
		});
	for (auto p : params) {
		json[p] = item.attribute(p).as_string();
	}
	// Links need to be parsed seperately :DDDDDDDDDDDDDDDDDD
	const auto links = item.children("link");
	int i = 0;
	for (pugi::xml_named_node_iterator it = links.begin(); it != links.end(); ++it) {
		json["links"][i]["text"] = it->child_value();
		json["links"][i]["href"] = it->attribute("href").as_string();
		i++;
	}
	return json;
}


// Generate a list of web badge elements
static crow::json::wvalue webBadgeArray(const std::vector<std::pair<std::string, std::string>>& webBadges, int amount) {
	crow::json::wvalue result;
	// Random indeces
	std::vector<int> random(webBadges.size());
	std::iota(random.begin(), random.end(), 0);
	std::random_device rd;
	std::shuffle(random.begin(), random.end(), rd);
	const std::vector<int> indeces(random.begin(), random.begin() + amount);
	int j = 0;
	for (auto i : indeces) {
		const std::pair<std::string, std::string>& pair = webBadges[i];
		result[j]["img"] = pair.first;
		result[j]["href"] = pair.second;
		j++;
	}
	return result;
}

template<typename... Args>
std::string dyna_print(std::string_view rt_fmt_str, Args&&... args)
{
    return std::vformat(rt_fmt_str, std::make_format_args(args...));
}

// Format unix time stamp THE CORRECT way
static std::string formatDate(int unixt) {
	const auto time {std::chrono::seconds(unixt)};
	const auto t = std::chrono::sys_time<std::chrono::seconds>{time};
	return dyna_print("{:%d.%m.%Y}", t);
}

// Format unix time stamp like a sociopath
static std::string formatDateRss(int unixt) {
	const auto time {std::chrono::seconds(unixt)};
	const auto t = std::chrono::sys_time<std::chrono::seconds>{time};
	return dyna_print("{:%a, %d %b %Y %T} +0300", t);
}

int main()
{
	crow::SimpleApp app;
	// Database
	SQLite::Database dbComments("db.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
	SQLite::Database dbBlog("blog.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
	SQLite::Database dbNpesta("npesta.db", SQLite::OPEN_READONLY);
	// Password things
	const std::string salt = "montakymmentätuhattavoileipäsämpylää"; // Password salt
	const std::string pass = "$2a$12$HDOM/d1alYPXxWsLpiH1CuR4Pq0WiBkIrPmP5tl7s2fKLNRfOamC2"; // TODO: Maybe should be in .env file? Idk
	// Message of the day (motd)
	std::string dailyMsg;
	std::vector<std::string> motdBackup; // Old motds to use in the abscence of a new one
	// When motd last updated
	auto lastUpdate = std::chrono::system_clock::now();
	const auto getMotd = [&dailyMsg, &motdBackup, &lastUpdate]() -> std::string {
		using namespace std::chrono;
		const auto now = system_clock::now();
		// If motd not set, use old one
		if (dailyMsg.empty()) {
			const auto day = duration_cast<days>(now.time_since_epoch()).count();
			return motdBackup.at(day % motdBackup.size());
		}
		// If motd is set, use it.
		CROW_LOG_INFO << dyna_print("Time since last update: {:%H h %M m %S s}", now - lastUpdate);
		// If more than 30 hours has passed, reset the daily word
		if (duration_cast<hours>(now - lastUpdate).count() > 30) {
			lastUpdate = now;
			dailyMsg.clear();
		}
		return dailyMsg;
	};
	// Xml
	pugi::xml_document pdoc;
	crow::json::wvalue projectStructure; // yadadadadada blah blah blah lalalalalalala
	std::vector<crow::json::wvalue> projects;
	// Badges
	const std::vector<std::pair<std::string, std::string>> webBadges = {
		{"powered-by-openbsd.webp", "https://openbsd.org"},
		{"7zip.webp", "https://www.7-zip.org"},
		{"winrar.webp", "https://www.win-rar.com/"},
		{"linux_now.gif", "https://kernel.org"},
		{"emacs2.webp", "https://www.gnu.org/software/emacs"},
		{"vim.webp", "https://www.vim.org/"},
		{"any_browser.webp", ""}, // I'll come up with something later
		{"powered.gif", ""},
		{"blender.webp" , "https://www.blender.org/"},
		{"cc-button.webp", "https://skp.fi/"},
		{"firefox_now.webp", "https://www.firefox.com/"},
		{"best_viewed_with_eyes.gif", ""},
		{"button38.webp", "https://github.com/Wisdurm/"},
		{"gue.jpg", "/guestbook"},
		{"powered-cpp.webp", "https://legacy.cplusplus.com/"},
		{"powered-cpp.webp", "https://cppreference.com/"},
		{"sdl.webp", "https://www.libsdl.org/"},
		{"winrar3.webp", "https://www.win-rar.com/"},
		{"winrar4.webp", "https://www.win-rar.com/"},
		{"winxp.webp", "https://endof10.org/"},
		{"tor.webp", "https://www.torproject.org"},
		{"valid-rss-rogers.webp", "http://validator.w3.org/feed/check.cgi?url=https%3A//wisdurm.fi/rss/canon"},
	};
	// Error messages
	const std::unordered_map<std::string, std::string> errMsgs {
		{"evil", "You did not promise to be kind."},
		{"name", "Your name did not pass validation."},
		{"msg", "Your message did not pass validation."},
		{"internal", "The server encountered an internal error, "
		 "you should probably checkback later."},
		{"ip", "Your public ip address has already been used to "
		 "make a comment, sorry."},
	};

	CROW_ROUTE(app, "/")([&getMotd, &projects, &webBadges, &dbNpesta]
			     (const crow::request& req){
		// Project of the day
		const unsigned long day = duration_cast<std::chrono::days>(std::chrono::system_clock::now().time_since_epoch()).count();
		std::default_random_engine rde (day);
		const auto potd = projects.at(std::uniform_int_distribution<int>(0, projects.size() - 1)(rde));
		const auto page = crow::mustache::load("index.html");
		crow::mustache::context ctx({
				{"msg-daily", getMotd()},
				{"project-daily", potd},
				{"badges", webBadgeArray(webBadges, 8)},
				{"npesta", npestaQuip(dbNpesta)}
			});
		return page.render(ctx);
	});

	CROW_ROUTE(app, "/write")([&getMotd]
			     (const crow::request& req){
		auto page = crow::mustache::load("write.html");
		crow::mustache::context ctx({
				{"msg-daily", getMotd()}
			});
		return page.render(ctx);
	});

	CROW_ROUTE(app, "/projects")([&getMotd, &projectStructure, &webBadges]
				     (const crow::request& req){
		// List of projects
		auto page = crow::mustache::load("projects.html");
		crow::mustache::context ctx({
				{"msg-daily", getMotd()},
				{"projects", projectStructure},
				{"badges", webBadgeArray(webBadges, 8)}
			});
		return page.render(ctx);
	});

	CROW_ROUTE(app, "/contact")([&getMotd]
				    (const crow::request& req){       
		// TODO: Contact form
		auto page = crow::mustache::load("contact.html");
		crow::mustache::context ctx({
				{"msg-daily", getMotd()}
			});
		return page.render(ctx);
	});

	CROW_ROUTE(app, "/admin")([&lastUpdate]
				  (const crow::request& req){       
		// Admin page, plain
		auto page = crow::mustache::load("motd.html");
		crow::mustache::context ctx({
				{"time-since", dyna_print("{:%H h %M m %S s}", (std::chrono::system_clock::now() - lastUpdate)) }
			});
		return page.render(ctx);
	});

	CROW_ROUTE(app, "/blog")([&getMotd, &dbBlog]
				 (const crow::request& req){       
		// List of posts
		auto page = crow::mustache::load("blog.html");
		crow::mustache::context ctx;
		ctx["msg-daily"] = getMotd();
		try {
			// Get list of categories
			struct Category {
				const int id;
				const std::string name;
				const std::string desc;
			};
			std::vector<Category> categories;
			SQLite::Statement cquery(dbBlog, "SELECT * FROM categories;");
			while (cquery.executeStep()) {
				const int id = cquery.getColumn(0);
				std::string name = cquery.getColumn(1);
				name.at(0) = std::toupper(name.at(0));
				const std::string desc = cquery.getColumn(2);
				categories.push_back({id, name, desc});
			}
			// Get all posts
			for (auto cat : categories) {
				// Name
				if (cat.id != 1) {
					// First category is canonical, which is ordered so done seperately
					// -2 so it begins at 0
					ctx["categories"][cat.id-2]["name"] = cat.name;
					ctx["categories"][cat.id-2]["desc"] = cat.desc;
				}
				SQLite::Statement pquery(dbBlog,
							 "SELECT * FROM posts "
							 "WHERE category_id=? "
							 "ORDER BY id DESC;");
				pquery.bind(1, cat.id);
				// Step forward to get results
				int index = 0;
				while (pquery.executeStep()) {
					const std::string title = pquery.getColumn(1);
					const std::string date = formatDate(pquery.getColumn(2));
					const int id = pquery.getColumn(0).getInt();
					std::string link = std::format("{}. {}", id, title);
					std::transform(link.begin(), link.end(), link.begin(),
						       [](char c) { return (c == ' ') ? '_' : c; });
					if (cat.id == 1) { // Canonical
						ctx["main"]["posts"][index]["name"] = title;
						ctx["main"]["posts"][index]["date"] = date;
						ctx["main"]["posts"][index]["link"] = link;
					}
					else {
						ctx["categories"][cat.id-2]["posts"][index]["name"] = title;
						ctx["categories"][cat.id-2]["posts"][index]["date"] = date;
						ctx["categories"][cat.id-2]["posts"][index]["link"] = link;
					}
					index++;
				}
			}
		} catch(std::exception& e) {
			CROW_LOG_ERROR << e.what();
			ctx["main"]["posts"][0]["name"] = "Something has gone slightly wrong";
			ctx["main"]["posts"][0]["date"] = "???";
			ctx["main"]["posts"][0]["link"] = "/404";
		}
		return page.render(ctx);
	});

	CROW_ROUTE(app, "/postblog")
		.methods("GET"_method, "POST"_method)([&pass, &salt, &dbBlog] (const crow::request& req, crow::response& res){
		// Post blog post
		auto re = req.get_body_params();
		if (const char* p = re.get("pass"),
		    *title = re.get("title"),
		    *text = re.get("text"),
		    *desc = re.get("desc"),
		    *n = re.get("nro"),
		    *category = re.get("cat");
		    p and n and desc and category and title and text)
		{
			const std::string url_pass = p;
			if (not BCrypt::validatePassword(url_pass + salt, pass)) {				
				res.redirect("/");
				res.end();
				return;
			}
			// Validation not really needed because we can assume I'm not stupid
			try {
				const int nro = std::stoi(n);
				SQLite::Statement pquery(dbBlog,
							 "INSERT INTO posts "
							 "(name, posted, category_id, nro, desc)"
							 "VALUES(?, unixepoch('now'), ?, ?, ?);");
				pquery.bind(1, title);
				pquery.bind(2, std::stoi(category)); // TODO: Map category
				pquery.bind(3, nro); // TODO: Automatic numbering
				pquery.bind(4, desc);
				pquery.executeStep();
				const int post_id = [&](){
					SQLite::Statement query(dbBlog,
								"SELECT id FROM posts "
								"WHERE nro=?;");
					query.bind(1, nro);
					query.executeStep();
					return query.getColumn(0);
				}();
				SQLite::Statement tquery(dbBlog,
							 "INSERT INTO post_texts "
							 "(lang, text, post_id)"
							 "VALUES('en', ?, ?);");
				tquery.bind(1, text);
				tquery.bind(2, post_id);
				tquery.executeStep();
			} catch (std::exception& e) {
				CROW_LOG_ERROR << e.what();
			}
		}
		res.redirect("/blog");
		res.end();
	});

	CROW_ROUTE(app, "/rss/<string>")([&dbBlog]
					 (const crow::request& req, crow::response& res, std::string category) {
		// Rss feeds
		category<:0:> = std::tolower(category<:0:>);
		int category_id;
		std::string description = "Wisdurm blog posts, universal feed";
		if (category != "universal") {
			try {
				SQLite::Statement cquery(dbBlog,
							 "SELECT * FROM categories "
							 "WHERE name=?;");
				cquery.bind(1, category);			
				if (not cquery.executeStep()) {
					CROW_LOG_ERROR << "Channel: " << category << " does not exist";
					res.code = 400;
					res.end();
					return;
				}
				// Get info
				category_id = cquery.getColumn(0);
				description = cquery.getColumn(2).getString();
			} catch (std::exception& e) {
				CROW_LOG_ERROR << e.what();
				res.code = 500;
				res.end();
				return;
			}
		}
		//
		// Get posts
		//
		struct post {
			const std::string title;
			const std::string desc;
			const std::string link;
			const int posted;
		};
		std::vector<post> posts;
		try {
			auto pquery = (category == "universal") ?
				SQLite::Statement(dbBlog,
						  "SELECT * FROM posts "
						  "ORDER BY id DESC;") :
				SQLite::Statement(dbBlog,
						  "SELECT * FROM posts "
						  "WHERE category_id=? "
						  "ORDER BY id DESC;");
			// Add values to statement
			if (category != "universal") {
				pquery.bind(1, category_id);
			}
			// Step forward to get results
			while (pquery.executeStep()) {
				const std::string title = pquery.getColumn(1);
				const std::string desc = pquery.getColumn(5);
				const int posted = pquery.getColumn(2);
				std::string link = std::format("{}. {}", pquery.getColumn(0).getInt(), title);
				std::transform(link.begin(), link.end(), link.begin(),
					       [](char c) { return (c == ' ') ? '_' : c; });
				posts.push_back(<%
						.title = title,
						.desc = desc,
						.link = "https://wisdurm.fi/blog/" + link,
						.posted = posted
						%>);
			}
		} catch (std::exception& e) {
			CROW_LOG_ERROR << e.what();
			res.code = 500;
			res.end();
			return;
		}
                // Format response
		pugi::xml_document rss;
		auto ch = rss.append_child("channel");
		ch.append_attribute("xml:base") = "https://wisdurm.fi/blog";
		ch.append_child("title").append_child(pugi::node_pcdata).set_value("Wisdurm blog: " + category + " posts");
		ch.append_child("description").append_child(pugi::node_pcdata).set_value(description);
		ch.append_child("link").append_child(pugi::node_pcdata).set_value("https://wisdurm.fi/blog");
		auto atom = ch.append_child("atom:link");
		atom.append_attribute("href") = std::format("https://wisdurm.fi/rss/{}", category);
		atom.append_attribute("rel") = "self";
		atom.append_attribute("type") = "application/rss+xml";
		// Items
		for (auto post : posts) {
			auto item = ch.append_child("item");
			item.append_child("title").append_child(pugi::node_pcdata).set_value(post.title);
			item.append_child("description").append_child(pugi::node_pcdata).set_value(post.desc);
			item.append_child("link").append_child(pugi::node_pcdata).set_value(post.link);
			item.append_child("guid").append_child(pugi::node_pcdata).set_value(post.link);
			item.append_child("pubDate").append_child(pugi::node_pcdata).set_value(formatDateRss(post.posted));
		}
		res.body = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \n"
			"<rss version=\"2.0\" xmlns:atom=\"http://www.w3.org/2005/Atom\"> \n"
			+ InnerXML(rss) + "</rss>";
		res.end();
	});

	CROW_ROUTE(app, "/blog/<int>.<string>")([&getMotd, &dbBlog]
						(const crow::request& req, crow::response& res, int postId, std::string _) {
		// Get blog post text
		try {
			SQLite::Statement pquery(dbBlog,
						 "SELECT * FROM post_texts "
						 "WHERE post_id=?;");
			pquery.bind(1, postId);
			if (not pquery.executeStep()) {
				res.redirect("/404");
				res.end();
				return;
			}
			const std::string postText = pquery.getColumn(1);
			// Get date
			SQLite::Statement dquery(dbBlog,
						 "SELECT posted FROM posts "
						 "WHERE id=?;");
			dquery.bind(1, postId);
			dquery.executeStep();
			const std::string date = formatDate(dquery.getColumn(0));
			//
			auto page = crow::mustache::load("blogPost.html");
			crow::mustache::context ctx({
					{"msg-daily", getMotd()},
					{"blogText", parse(postText)},
					{"posted", date}});
			res.body = page.render(ctx).body_;
			res.end();
		} catch (std::exception& e) {
			CROW_LOG_ERROR << e.what();
			res.redirect("/404");
			res.end();
		}
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
					dailyMsg = m;
					if (dailyMsg.length() > 100) {
						dailyMsg.erase(dailyMsg.begin() + 100, dailyMsg.end());
					}
					CROW_LOG_INFO << "Updated motd : \"" << dailyMsg << "\"";
					motdBackup.push_back(dailyMsg);
					// Update date
					lastUpdate = std::chrono::system_clock::now();
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

	CROW_ROUTE(app, "/guestbook")
		.methods("GET"_method, "POST"_method)([&getMotd, &dbComments, &errMsgs]
						      (const crow::request& req) {
			// Page for comments and form
			auto page = crow::mustache::load("guestbook.html");
			crow::mustache::context ctx({
					{"msg-daily", getMotd()}
				});
			// If error message
			if (const char* e = req.url_params.get("err")) {
				ctx["error"] = errMsgs.at(e);
			}
			// Get comments
			try {
				SQLite::Statement query(dbComments,
							"SELECT * FROM comments "
							"ORDER BY posted DESC;");
				int i = 0;
				while (query.executeStep()) {
					ctx["comments"][i]["msg"] = query.getColumn(0).getString();
					ctx["comments"][i]["name"] = query.getColumn(1).getString();
					ctx["comments"][i]["posted"] = formatDate(query.getColumn(2));
					i++;
				}
			} catch (std::exception& e) {
				CROW_LOG_ERROR << e.what();
			}
			return page.render(ctx);
		});

	CROW_ROUTE(app, "/comment").methods("GET"_method, "POST"_method)
		([&pass, &salt, &dbComments]
		 (const crow::request& req, crow::response& res)
		{
			// Post comment
			// remote_ip_address does not work because of reverse proxy
			// X-Real-IP needs to be configued in Nginx
			const std::string addr = req.get_header_value("X-Real-IP");
			// Actual params
			auto qs = req.get_body_params();
			if (const char* n = qs.get("name"),
			    *m = qs.get("msg");
			    n and m)
			{
				const std::string name = n;
				const std::string msg = m;
				time_t now = time(nullptr);
				// Validation
				if (name.size() == 0 or
				    name.size() > 50) {
					// name len wrong
					res.redirect("/guestbook?err=name");
					res.end();
					return;
				}
				if (msg.size() < 5 or
				    msg.size() > 250) {
					// msg len wrong
					res.redirect("/guestbook?err=msg");
					res.end();
					return;
				}
				// Correct checkmarks
				const int trick = localtime(&now)->tm_hour % 6;
				for (uint8_t i = 0; i < 8; i++) {
					// bruh
					const auto e = std::string("evil") +
						std::string{static_cast<char>(i + '0')};
					// Skip 6 because front end does aswell
					if (i != 6 and (i != trick and not qs.get(e))
					    or (i == trick and qs.get(e))) {
						res.redirect("/guestbook?err=evil");
						res.end();
						return;
					}
				}
				try {
					// Check ip not already used
					SQLite::Statement ipquery(dbComments,
								  "SELECT * FROM comments "
								  "WHERE ip=?;");
					ipquery.bind(1, addr);
					if (ipquery.executeStep()) {
						res.redirect("/guestbook?err=ip");
						res.end();
						return;
					}
					// Add to database
					SQLite::Statement query(dbComments,
								"INSERT INTO comments "
								"VALUES(?, ?, unixepoch('now'), ?);");
					query.bind(1, msg);
					query.bind(2, name);
					query.bind(3, addr);
					query.executeStep();
				} catch (std::exception& e) {
					CROW_LOG_ERROR << e.what();
					res.redirect("/guestbook?err=internal");
					res.end();
					return;
				}
				// Success
				res.redirect("/guestbook");
				res.end();
			} else {
				// Missing params
				res.redirect("/guestbook?err=missing");
				res.end();
			}
		});

	CROW_ROUTE(app, "/npesta")
		([&dbNpesta] {			
			return npestaQuip(dbNpesta);
		});

	CROW_CATCHALL_ROUTE(app)
		([&dailyMsg](crow::response& res) {
			if (res.code == 404) {
				auto page = crow::mustache::load("404.html");
				crow::mustache::context ctx({ {"msg-daily", dailyMsg} });
				res.body = page.render(ctx).body_;
			} else if (res.code == 405) {
				res.body = "The HTTP method does not seem to be correct.";
			} else if (res.code == 500) {
				res.body = "500 Intenal error. Something serious is going on";
			}		
			res.end();
		});
	// Motd
	CROW_LOG_INFO << "Loading motd.txt";
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
	CROW_LOG_INFO << "Loading projects";
	const pugi::xml_parse_result result = pdoc.load_file("projects.xml");
	if (!result) {
		CROW_LOG_CRITICAL << "Unable to open projects.xml file";
		return EXIT_FAILURE;
	}
	// Parse items
	int i = 0;
	for (pugi::xml_node cat : pdoc.child("projects"))
	{
		// Make sections for each category
		projectStructure[i]["name"] = cat.attribute("name").as_string();
		projectStructure[i]["desc"] = cat.attribute("desc").as_string();
		// Items
		int j = 0;
		for (pugi::xml_node item : cat.children("item")) {
			projectStructure[i]["items"][j] = toCard(item);
			projects.push_back(toCard(item)); // Think I kind of have to do this unfortunately
			j++;
		}
		i++;
	}
	// Other stuff finished, start server
	app.port(18080).multithreaded()
		.use_compression(crow::compression::algorithm::DEFLATE)
		.loglevel(crow::LogLevel::INFO)
		.run();
}
