#pragma once

#include "URL.h"

#include <string>

class HTTP
{
public:

    static std::string Get(
        const URL& url
    );
};