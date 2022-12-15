//
// Color.hh for pekwm
// Copyright (C) 2022 Claes Nästén <pekdon@gmail.com>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#ifndef _PEKWM_COLOR_HH_
#define _PEKWM_COLOR_HH_

#include "config.h"

#include "Compat.hh"
#include "Types.hh"

#include <map>
#include <string>

#include "X11.hh"

namespace pekwm
{
	void clearColorResources(void);
	const std::map<std::string, std::string>& getColorResources(void);

	std::string getColorResource(std::string desc);
	XColor* getColor(const std::string& color);
}

#endif // _PEKWM_COLOR_HH_
