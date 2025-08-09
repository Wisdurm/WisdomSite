#include "crow.h"

int main()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([]
        (const crow::request& req){
	    auto page = crow::mustache::load("begin.html");
        crow::mustache::context ctx({{"ip", req.remote_ip_address}});
	    return page.render(ctx);
    });

    app.port(18080).multithreaded().run();
}
