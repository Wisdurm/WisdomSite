#include "markdown.hpp"

std::string parse(std::string& markdown)
{
	// For testing purposes this just needs to return something
	std::string r = "<h1>" + markdown + "</h1>";
	return r;
}