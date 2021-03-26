//
// Observable.hh for pekwm
// Copyright (C) 2009-2021 Claes Nästen <pekdon@gmail.com>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#pragma once

#include <map>
#include <vector>

/**
 * Message sent to observer.
 */
class Observation {
public:
    virtual ~Observation(void);
};

/**
 * Observable object.
 */
class Observable {
public:
    virtual ~Observable(void);
};

/**
 * Object observing Observables.
 */
class Observer {
public:
    virtual ~Observer(void);
    virtual void notify(Observable*, Observation*) = 0;
};

class ObserverMapping {
public:
    ObserverMapping(void);
    ~ObserverMapping(void);

    size_t size(void) const { return _observable_map.size(); }

    void notifyObservers(Observable *observable, Observation *observation);
    void addObserver(Observable *observable, Observer *observer);
    void removeObserver(Observable *observable, Observer *observer);

    void removeObservable(Observable *observable);

private:
    /** Map from Observable to list of observers. */
    std::map<Observable*, std::vector<Observer*>> _observable_map;
};

namespace pekwm
{
    ObserverMapping* observerMapping(void);
}
