
#pragma once

#include <string>

namespace network::http
{

struct Header
{
	std::string name;
	std::string value;
};

} // namespace network::http