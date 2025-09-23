#include "markdown.hpp"

std::string parse(std::string& markdown)
{
	// For testing purposes this just needs to return something
	int index = 0;
	return parseInternal(markdown, index);
}

std::string parseInternal(std::string& markdown, int& index)
{
	switch (markdown[index])
	{
	case '#':
	{
		int headingLevel = 0;
		do 
		{
			headingLevel++;
			index++;
		}
		while (markdown[index] == '#');

		std::string result = "<h" + std::to_string(headingLevel) + ">" + parseInternal(markdown, index) + "</h" + std::to_string(headingLevel) + ">" ; // format in c++ 20 :cry:
		return result;
	}
	default:
		break;
	}
}