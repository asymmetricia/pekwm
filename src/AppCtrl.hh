//
// AppCtrl.hh for pekwm
// Copyright (C) 2021 Claes Nästén <pekdon@gmail.com>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#ifndef _APP_CTRL_HH_
#define _APP_CTRL_HH_

#include <string>

/**
 * Application control interface, allows for reloading/stopping the
 * application.
 */
class AppCtrl {
public:
    virtual void reload(void) = 0;
    virtual void restart(void) = 0;
    virtual void restart(std::string cmd) = 0;
    virtual void shutdown(void) = 0;
};

#endif // _APP_CTRL_HH_
