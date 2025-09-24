#pragma once

#include <string>
#include "markdown.hpp"
#include <md4c-html.h>

static void captureHtmlFragment(const MD_CHAR* data, MD_SIZE data_size, void* userData);

std::string parse(std::string& markdown)
{
	// This is very bizarre
	std::string result = "";
	md_html(markdown.c_str(),
		MD_SIZE(markdown.size()),
		captureHtmlFragment,
		&result,
		0,
		0
	);
	return result;
}

static void captureHtmlFragment(const MD_CHAR* data, MD_SIZE data_size, void* userData) {
	std::string* s = static_cast<std::string*>(userData);

	if (data_size > 0) {
		s->append(data, int(data_size));
	}
}