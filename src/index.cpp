
// Internal
#include "crow/app.h"
#include "crow/http_request.h"
#include "crow/http_response.h"
#include "crow/json.h"
#include "markdown.hpp"
// C++
#include <array>
#include <numeric>
#include <string>
#include <iostream>
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
#include "../../libbcrypt-src/include/bcrypt/BCrypt.hpp"
#include "pugixml.hpp"
#include <sqlite3.h>

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
			daily = "";
		} else {
			return daily;
		}
	}
 	// Otherwise, fallback to older ones
	long day = now/60/60/24; // How many days since Jan 1 1900
	return motdBackup.at(day % motdBackup.size());
}

static crow::json::wvalue npestaQuip(sqlite3* dbNpesta)
{
	// Get message count
	int count;
	sqlite3_exec(dbNpesta,
		     "SELECT count(id) FROM quips;",
		     [](void* data, int argc, char** argv, char** azColName) {
			     int* count = static_cast<int*>(data);
			     *count = std::stoi(argv[0]);
			     return 0;
		     }, &count, NULL);
	CROW_LOG_INFO << count;
	// Get random message
	std::default_random_engine rde {std::random_device{}()};
	const int index = std::uniform_int_distribution<int>(1, count)(rde);

	sqlite3_stmt* st;
	int rc = sqlite3_prepare_v2(dbNpesta,
				    "SELECT msg FROM quips "
				    "WHERE id=?;",
				    -1, &st, NULL);
	CROW_LOG_DEBUG << "Prepare: " << rc;
	if (rc != SQLITE_OK) {
		CROW_LOG_ERROR << "SQL ERROR: " << rc;
		rc = sqlite3_finalize(st);
                return crow::json::wvalue{{"success", false}};
	}
	// Add values to statement
	sqlite3_bind_int(st, 1, index);
	// Step forward to see if a result (1 at max)
	rc = sqlite3_step(st);
	CROW_LOG_DEBUG << "Step: " << rc;
	if (rc != SQLITE_ROW) {
		CROW_LOG_ERROR << "Message: " << index << " does not exist";
		rc = sqlite3_finalize(st);
		CROW_LOG_DEBUG << "Finalize: " << rc;				
		return crow::json::wvalue{{"success", false}};
	}
	// Get text
	const std::string message = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st, 0)));
	// Cleanup
	rc = sqlite3_finalize(st);
	CROW_LOG_DEBUG << "Finalize: " << rc;
	return crow::json::wvalue{{"success", true}, {"message", message}, {"index", index}};
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

// Format unix time stamp THE CORRECT way
static std::string formatDate(int unixt) {
	char buffer[256];
	const auto t = static_cast<time_t>(unixt);
	std::strftime(buffer, sizeof(buffer), "%d.%m.%Y", localtime(&t));
	return buffer;
}

// Format unix time stamp like a sociopath
static std::string formatDateRss(int unixt) {
	char buffer[256];
	const auto t = static_cast<time_t>(unixt);
	std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %T +0300", localtime(&t));
	return buffer;
}

int main()
{
	// Seed randomness
	srand(time(0));
	crow::SimpleApp app;
	// Database
	sqlite3 *dbComments;
        sqlite3 *dbBlog;
	sqlite3 *dbNpesta;
	// Password things
	const std::string salt = "montakymmentätuhattavoileipäsämpylää"; // Password salt
	const std::string pass = "$2a$12$HDOM/d1alYPXxWsLpiH1CuR4Pq0WiBkIrPmP5tl7s2fKLNRfOamC2"; // TODO: Maybe should be in .env file? Idk
	// Message of the day (motd)
	std::string dailyMsg = "";
	std::vector<std::string> motdBackup; // Old motds to use in the abscence of a new one
	time_t lastUpdate; // Time since motd last updated
	time(&lastUpdate);
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

	CROW_ROUTE(app, "/")([&dailyMsg, &motdBackup, &lastUpdate, &projects, &webBadges, &dbNpesta]
			     (const crow::request& req){
		// Project of the day
		time_t now;
		time(&now);
		long day = now/60/60/24; // How many days since Jan 1 1900
		crow::json::wvalue potd = projects.at(day % projects.size()); // TODO: Randomization
		//
		auto page = crow::mustache::load("index.html");
		crow::mustache::context ctx({
				{"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)},
				{"project-daily", potd},
				{"badges", webBadgeArray(webBadges, 8)},
				{"npesta", npestaQuip(dbNpesta)}
			});
		return page.render(ctx);
	});

	CROW_ROUTE(app, "/write")([&dailyMsg, &motdBackup, &lastUpdate]
			     (const crow::request& req){
		auto page = crow::mustache::load("write.html");
		crow::mustache::context ctx({
				{"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)}
			});
		return page.render(ctx);
	});

	CROW_ROUTE(app, "/projects")([&dailyMsg, &motdBackup, &lastUpdate, &projectStructure, &webBadges]
				     (const crow::request& req){
		// List of projects
		auto page = crow::mustache::load("projects.html");
		crow::mustache::context ctx({
				{"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)},
				{"projects", projectStructure},
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

	CROW_ROUTE(app, "/blog")([&dailyMsg, &motdBackup, &lastUpdate, &dbBlog]
				 (const crow::request& req){       
		// List of posts
		auto page = crow::mustache::load("blog.html");
		crow::mustache::context ctx;
		ctx["msg-daily"] = getMotd(dailyMsg, motdBackup, lastUpdate);
		// ctx["badges"] = webBadgeArray(webBadges, 8);
		// Get list of categories
		std::vector<std::pair<int, std::string>> categories;
		sqlite3_exec(dbBlog,
			     "SELECT * FROM categories;",
			     [](void* data, int argc, char** argv, char** azColName) {
				     auto cats = reinterpret_cast<std::vector<std::pair<int, std::string>>*>(data);
				     cats->push_back(<% std::stoi(argv[0]), argv<:1:> %>);
				     // First letter uppercase
				     cats->back().second[0] = std::toupper(cats->back().second<:0:>);
				     return 0;
			     }, &categories, NULL);
		// Get all posts
		for (auto cat : categories) {
			// Name
			if (cat.first != 1) // First category is canonical, which is ordered so done seperately
				ctx["categories"][cat.first-2]["name"] = cat.second;
			// idk bruh
			sqlite3_stmt* st;
			int rc = sqlite3_prepare_v2(dbBlog,
						    "SELECT * FROM posts "
						    "WHERE category_id=? "
						    "ORDER BY id DESC;",
						    -1, &st, NULL);
			CROW_LOG_DEBUG << "Prepare: " << rc;
			if (rc != SQLITE_OK) {
				CROW_LOG_ERROR << "SQL ERROR: " << rc;
				rc = sqlite3_finalize(st); // I think?
				break;
			}
			// Add values to statement
			sqlite3_bind_int(st, 1, cat.first);
			// Step forward to get results
			int index = 0;
			while (true) {
				rc = sqlite3_step(st);
				CROW_LOG_DEBUG << "Step: " << rc;
				if (rc == SQLITE_ROW) {
					// Apparently this only works if the text is ascii,
					// but I'm sure that this won't come back to bite me in the ass
					// :clueless:
					// From the future, it seems to work, but I'll leave this comment here in case it ever does not
					// for whatever reason
					const std::string title = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st, 1)));
					if (cat.first == 1) { // Canonical
						ctx["main"]["posts"][index]["name"] = title;
						ctx["main"]["posts"][index]["date"] = formatDate(sqlite3_column_int(st, 2));
						std::string link = std::format("{}. {}", sqlite3_column_int(st, 0), title);
						std::transform(link.begin(), link.end(), link.begin(),
							       [](char c) { if (c == ' ') return '_'; else return c; });
						ctx["main"]["posts"][index]["link"] = link;
					}
					else {
						ctx["categories"][cat.first-2]["posts"][index]["name"] = title;
						ctx["categories"][cat.first-2]["posts"][index]["date"] = formatDate(sqlite3_column_int(st, 2));;
						std::string link = std::format("{}. {}", sqlite3_column_int(st, 0), title);
						std::transform(link.begin(), link.end(), link.begin(),
							       [](char c) { if (c == ' ') return '_'; else return c; });
						ctx["categories"][cat.first-2]["posts"][index]["link"] = link;
					}
					index++;
				} else
					break;
			}
			CROW_LOG_DEBUG << "Last code (should be 101): " << rc;
			// Cleanup
			rc = sqlite3_finalize(st);
			CROW_LOG_DEBUG << "Finalize: " << rc;
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
			if (BCrypt::validatePassword(url_pass + salt, pass))
			{
				// Validation not really needed because we can assume I'm not stupid
				int nro = std::stoi(n);
				{
					sqlite3_stmt* st;
					int rc = sqlite3_prepare_v2(dbBlog,
								    "INSERT INTO posts "
								    "(name, posted, category_id, nro, desc)"
								    "VALUES(?, unixepoch('now'), ?, ?, ?);",
								    -1, &st, NULL);
					CROW_LOG_DEBUG << "Prepare: " << rc;
					if (rc != SQLITE_OK) {
						CROW_LOG_ERROR << "SQL ERROR: " << rc;
						rc = sqlite3_finalize(st);
						res.redirect("/");
						return;
					}
					// Add values to statement
					sqlite3_bind_text(st, 1, title, -1, SQLITE_STATIC);
					sqlite3_bind_int(st, 2, 0); // TODO: Map category
					sqlite3_bind_int(st, 3, nro);
					sqlite3_bind_text(st, 4, desc, -1, SQLITE_STATIC);
					// Execute statement
					rc = sqlite3_step(st);
					CROW_LOG_DEBUG << "Step: " << rc;
					rc = sqlite3_finalize(st);
					CROW_LOG_DEBUG << "Finalize: " << rc;
				}
				int post_id;
				{
					sqlite3_stmt* st;
					int rc = sqlite3_prepare_v2(dbBlog,
								    "SELECT id FROM posts "
								    "WHERE nro=?;",
								    -1, &st, NULL);
					CROW_LOG_DEBUG << "Prepare: " << rc;
					if (rc != SQLITE_OK) {
						CROW_LOG_ERROR << "SQL ERROR: " << rc;
						rc = sqlite3_finalize(st);
						res.redirect("/");
						return;
					}
					// Add values to statement
					sqlite3_bind_int(st, 1, nro);
					// Step forward to see if a result
					rc = sqlite3_step(st);
					CROW_LOG_DEBUG << "Step: " << rc;
					if (rc != SQLITE_ROW) {
						CROW_LOG_ERROR << "Post number: " << nro << " does not exist";
						rc = sqlite3_finalize(st);
						CROW_LOG_DEBUG << "Finalize: " << rc;
						rc = sqlite3_finalize(st);
						res.redirect("/");
					}
					// Get text
					post_id = sqlite3_column_int(st, 0);	
				}
				{
					sqlite3_stmt* st;
					int rc = sqlite3_prepare_v2(dbBlog,
								    "INSERT INTO post_texts "
								    "(lang, text, post_id)"
								    "VALUES('en', ?, ?);",
								    -1, &st, NULL);
					CROW_LOG_DEBUG << "Prepare: " << rc;
					if (rc != SQLITE_OK) {
						CROW_LOG_ERROR << "SQL ERROR: " << rc;
						rc = sqlite3_finalize(st);
						res.redirect("/");
						return;
					}
					// Add values to statement
					sqlite3_bind_text(st, 1, text, -1, SQLITE_STATIC);
					sqlite3_bind_int(st, 2, post_id);
					// Execute statement
					rc = sqlite3_step(st);
					CROW_LOG_DEBUG << "Step: " << rc;
					rc = sqlite3_finalize(st);
					CROW_LOG_DEBUG << "Finalize: " << rc;
				}
			}
		}
		res.redirect("/");
		res.end();
	});

	CROW_ROUTE(app, "/rss/<string>")([&dbBlog]
					 (const crow::request& req, crow::response& res, std::string category) {
		// Rss feeds
		category<:0:> = std::tolower(category<:0:>);
		int category_id;
		std::string description = "Wisdurm blog posts, universal feed";
		if (category != "universal") {
			sqlite3_stmt* st_ct;
			int rc_ct = sqlite3_prepare_v2(dbBlog,
						       "SELECT * FROM categories "
						       "WHERE name=?;",
						       -1, &st_ct, NULL);
			CROW_LOG_DEBUG << "Prepare: " << rc_ct;
			if (rc_ct != SQLITE_OK) {
				CROW_LOG_ERROR << "SQL ERROR: " << rc_ct;
				rc_ct = sqlite3_finalize(st_ct);
				res.code = 500;
				res.end();
				return;
			}
			// Add values to statement
			sqlite3_bind_text(st_ct, 1, category.c_str(), -1, SQLITE_STATIC);
			// Step forward to see if a result (1 at max)
			rc_ct = sqlite3_step(st_ct);
			CROW_LOG_DEBUG << "Step: " << rc_ct;
			if (rc_ct != SQLITE_ROW) {
				CROW_LOG_ERROR << "Channel: " << category << " does not exist";
				rc_ct = sqlite3_finalize(st_ct);
				CROW_LOG_DEBUG << "Finalize: " << rc_ct;
				res.code = 400;
				res.end();
				return;
			}
			// Get info
			category_id = sqlite3_column_int(st_ct, 0);
			description = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st_ct, 2)));
			// Cleanup
			rc_ct = sqlite3_finalize(st_ct);
			CROW_LOG_DEBUG << "Finalize: " << rc_ct;
		}
		//
		// Get posts
		//
		sqlite3_stmt* st_p;
		int rc_p;
		if (category == "universal") {
			rc_p = sqlite3_prepare_v2(dbBlog,
						  "SELECT * FROM posts "
						  "ORDER BY id DESC;",
						  -1, &st_p, NULL);
		} else {
			rc_p = sqlite3_prepare_v2(dbBlog,
						  "SELECT * FROM posts "
						  "WHERE category_id=? "
						  "ORDER BY id DESC;",
						  -1, &st_p, NULL);
		}
		CROW_LOG_DEBUG << "Prepare: " << rc_p;
		if (rc_p != SQLITE_OK) {
			CROW_LOG_ERROR << "SQL ERROR: " << rc_p;
			rc_p = sqlite3_finalize(st_p);
			res.code = 500;
			res.end();
			return;
		}
		// Add values to statement
		if (category != "universal")
			sqlite3_bind_int(st_p, 1, category_id);
		// Step forward to get results
		struct post {
			const std::string title;
			const std::string desc;
			const std::string link;
			const int posted;
		};
		std::vector<post> posts;
		while (true) {
			rc_p = sqlite3_step(st_p);
			CROW_LOG_DEBUG << "Step: " << rc_p;
			if (rc_p == SQLITE_ROW) {
				const std::string title = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st_p, 1)));
				const std::string desc = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st_p, 5)));
				const int posted = sqlite3_column_int(st_p, 2);
				std::string link = std::format("{}. {}", sqlite3_column_int(st_p, 0), title);
				std::transform(link.begin(), link.end(), link.begin(),
					       [](char c) { if (c == ' ') return '_'; else return c; });
				posts.push_back(<%
						.title = title,
						.desc = desc,
						.link = "https://wisdurm.fi/blog/" + link,
						.posted = posted
						%>);
			} else
				break;
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

	CROW_ROUTE(app, "/blog/<int>.<string>")([&dailyMsg, &motdBackup, &lastUpdate, &dbBlog]
						(const crow::request& req, crow::response& res, int postId, std::string _) {
		// Get blog post text
		sqlite3_stmt* st_p;
		int rc_p = sqlite3_prepare_v2(dbBlog,
					      "SELECT * FROM post_texts "
					      "WHERE post_id=?;",
					      -1, &st_p, NULL);
		CROW_LOG_DEBUG << "Prepare: " << rc_p;
		if (rc_p != SQLITE_OK) {
			CROW_LOG_ERROR << "SQL ERROR: " << rc_p;
			rc_p = sqlite3_finalize(st_p);
			res.code = 500;
			res.end();
			return;
		}
		// Add values to statement
		sqlite3_bind_int(st_p, 1, postId);
		// Step forward to see if a result (1 at max)
		rc_p = sqlite3_step(st_p);
		CROW_LOG_DEBUG << "Step: " << rc_p;
		if (rc_p != SQLITE_ROW) {
			CROW_LOG_ERROR << "Post: " << postId << " does not exist";
			rc_p = sqlite3_finalize(st_p);
			CROW_LOG_DEBUG << "Finalize: " << rc_p;
			res.redirect("/404");
			res.end();
			return;
		}
		// Get text
		std::string postText = std::string(reinterpret_cast<const char*>(sqlite3_column_text(st_p, 1)));
		// Cleanup
		rc_p = sqlite3_finalize(st_p);
		CROW_LOG_DEBUG << "Finalize: " << rc_p;
		//
		auto page = crow::mustache::load("blogPost.html");
		crow::mustache::context ctx({ {"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)}, {"blogText", parse(postText)} });
		res.body = page.render(ctx).body_;
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

	CROW_ROUTE(app, "/guestbook")
		.methods("GET"_method, "POST"_method)([&dailyMsg, &motdBackup, &lastUpdate, &dbComments, &errMsgs]
						      (const crow::request& req) {
			// Page for comments and form
			auto page = crow::mustache::load("guestbook.html");
			crow::mustache::context ctx({
					{"msg-daily", getMotd(dailyMsg, motdBackup, lastUpdate)}
				});
			// If error message
			if (const char* e = req.url_params.get("err")) {
				const std::string err = e;
				const std::string errorBox = std::format(
					"<div class='error'><strong>Error</strong><br>"
					"The server could not process your request "
					"for the following reason: <br> {0}"
					"</div><br>",
					errMsgs.contains(err) ? errMsgs.at(err) : "Loser");
				ctx["error"] = errorBox;
			}
			// Get comments
			struct comment {		    
				const std::string msg;
				const std::string name;
				const int posted;
			};
			std::vector<comment> comments;
			// TODO: I think some tricks allow skipping the use of *data, they're just a
			// tad too difficult for me to figure out at the moment
			sqlite3_exec(dbComments,
				     "SELECT * FROM comments "
				     "ORDER BY posted DESC;",
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
				ctx["comments"][i]["posted"] = formatDate(c.posted);
				i++;
			}
			return page.render(ctx);
		});

	CROW_ROUTE(app, "/comment").methods("GET"_method, "POST"_method)
		([&dailyMsg, &pass, &salt, &motdBackup, &lastUpdate, &dbComments]
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
					goto EXIT;
				}
				if (msg.size() < 5 or
				    msg.size() > 250) {
					// msg len wrong
					res.redirect("/guestbook?err=msg");
					goto EXIT;
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
						goto EXIT;
					}
				}
				// Check ip not already used
				sqlite3_stmt* st_ip;
				int rc_ip = sqlite3_prepare_v2(dbComments,
							       "SELECT * FROM comments "
							       "WHERE ip=?;",
							       -1, &st_ip, NULL);
				CROW_LOG_DEBUG << "Prepare: " << rc_ip;
				if (rc_ip != SQLITE_OK) {
					CROW_LOG_ERROR << "SQL ERROR: " << rc_ip;
					rc_ip = sqlite3_finalize(st_ip); // I think?
					res.redirect("/guestbook?err=internal");
					goto EXIT;
				}
				// Add values to statement
				sqlite3_bind_text(st_ip, 1, addr.c_str(), -1, SQLITE_STATIC);
				// Step forward to see if a result (1 at max)
				rc_ip = sqlite3_step(st_ip);
				CROW_LOG_DEBUG << "Step: " << rc_ip;
				if (rc_ip == SQLITE_ROW) {
					CROW_LOG_INFO << "Failed, ip: " << addr << " has already posted";
					rc_ip = sqlite3_finalize(st_ip);
					CROW_LOG_DEBUG << "Finalize: " << rc_ip;
					res.redirect("/guestbook?err=ip");
					goto EXIT;
				}
				// Cleanup
				rc_ip = sqlite3_finalize(st_ip);
				CROW_LOG_DEBUG << "Finalize: " << rc_ip;
				//
				// Add to database
				sqlite3_stmt* st;
				int rc = sqlite3_prepare_v2(dbComments,
							    "INSERT INTO comments "
							    "VALUES(?, ?, ?, ?);",
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
				sqlite3_bind_int(st, 3, static_cast<int>(now));
				sqlite3_bind_text(st, 4, addr.c_str(), -1, SQLITE_STATIC);
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
	// Initialize database connection
        CROW_LOG_INFO << "Opening database connections";
	int opened = sqlite3_open("db.db", &dbComments);
	if (opened) {
		CROW_LOG_CRITICAL << "Unable to establish comment database connection: " << sqlite3_errmsg(dbComments);
		sqlite3_close(dbComments);
		return EXIT_FAILURE;
	}
	opened = sqlite3_open("blog.db", &dbBlog);
	if (opened) {
		CROW_LOG_CRITICAL << "Unable to establish blog database connection: " << sqlite3_errmsg(dbBlog);
		sqlite3_close(dbComments);
                sqlite3_close(dbBlog);
		return EXIT_FAILURE;
	}
	opened = sqlite3_open("npesta.db", &dbNpesta);
	if (opened) {
		CROW_LOG_CRITICAL << "Unable to establish blog database connection: " << sqlite3_errmsg(dbBlog);
		sqlite3_close(dbNpesta);
		sqlite3_close(dbComments);
		sqlite3_close(dbBlog);
		return EXIT_FAILURE;
        }
	// Other stuff finished, start server
	app.port(18080).multithreaded()
		.use_compression(crow::compression::algorithm::DEFLATE)
		.loglevel(crow::LogLevel::INFO)
		.run();
	// Post run cleanup
	std::cout << "[CLEANUP] Closing database connections\n";
	sqlite3_close(dbComments);
	sqlite3_close(dbBlog);
	sqlite3_close(dbNpesta);
	std::cout << "[CLEANUP] Closed\n";
}
