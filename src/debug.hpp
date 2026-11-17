#pragma once
inline bool& debug()
{
	static bool d;
	return d;
}
