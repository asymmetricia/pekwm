//
// Observer.hh for pekwm
// Copyright © 2009 Claes Nästen <me@pekdon.net>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#ifndef _OBSERVER_HH_
#define _OBSERVER_HH_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

class Observable;

class Observer {
public:
    Observer(void) { }
    virtual ~Observer(void) { }

    virtual void notify(Observable *observable) { }
};

#endif // _OBSERVER_HH_
