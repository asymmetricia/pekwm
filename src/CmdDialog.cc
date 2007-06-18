//
// CmdDialog.cc for pekwm
// Copyright © 2004-2007 Claes Nästén <me{@}pekdon{.}net>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//
// $Id$
//

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif // HAVE_CONFIG_H

#include <iostream>
#include <list>
#include <cwctype>

extern "C" {
#include <X11/Xutil.h> // XLookupString
#include <X11/keysym.h>
}

#include "PWinObj.hh"
#include "PDecor.hh"
#include "CmdDialog.hh"
#include "Config.hh"
#include "PScreen.hh"
#include "PixmapHandler.hh"
#include "KeyGrabber.hh"
#include "ScreenResources.hh"
#include "Workspaces.hh"

using std::cerr;
using std::endl;
using std::list;
using std::string;
using std::wstring;

//! @brief CmdDialog constructor
//! @todo Initial size, configurable?
CmdDialog::CmdDialog(Display *dpy, Theme *theme, const std::wstring &title)
    : PDecor(dpy, theme, "CMDDIALOG"),
      _cmd_data(theme->getCmdDialogData()),
      _cmd_wo(NULL), _bg(None),
      _wo_ref(NULL),
      _pos(0), _buf_off(0), _buf_chars(0)
{
    // PWinObj attributes
    _type = PWinObj::WO_CMD_DIALOG;
    _layer = LAYER_NONE; // hack, goes over LAYER_MENU
    _hidden = true; // don't care about it when changing worskpace etc

    // Add action to list, going to be used from close and exec
    ::Action action;
    _ae.action_list.push_back(action);

    titleAdd(&_title);
    titleSetActive(0);
    setTitle(title);

    _cmd_wo = new PWinObj(_dpy);
    XSetWindowAttributes attr;
    attr.override_redirect = false;
    attr.event_mask = ButtonPressMask|ButtonReleaseMask|ButtonMotionMask|
                      FocusChangeMask|KeyPressMask|KeyReleaseMask;
    _cmd_wo->setWindow(XCreateWindow(_dpy, _window,
                                     0, 0, 1, 1, 0,
                                     CopyFromParent, InputOutput, CopyFromParent,
                                     CWOverrideRedirect|CWEventMask, &attr));

    addChild(_cmd_wo);
    addChildWindow(_cmd_wo->getWindow());
    activateChild(_cmd_wo);
    _cmd_wo->mapWindow();

    // setup texture, size etc
    loadTheme();

    Workspaces::instance()->insert(this);
    woListAdd(this);
    _wo_map[_window] = this;
}

//! @brief CmdDialog destructor
CmdDialog::~CmdDialog(void)
{
    Workspaces::instance()->remove(this);
    _wo_map.erase(_window);
    woListRemove(this);

    // Free resources
    if (_cmd_wo != NULL) {
        _child_list.remove(_cmd_wo);
        removeChildWindow(_cmd_wo->getWindow());
        XDestroyWindow(_dpy, _cmd_wo->getWindow());
        delete _cmd_wo;
    }

    unloadTheme();
}

//! @brief Handles ButtonPress, moving the text cursor
ActionEvent*
CmdDialog::handleButtonPress(XButtonEvent *ev)
{
    if (*_cmd_wo == ev->window) {
        // FIXME: move cursor
        return NULL;
    } else {
        return PDecor::handleButtonPress(ev);
    }
}

//! @brief Handles KeyPress, editing the buffer
ActionEvent*
CmdDialog::handleKeyPress(XKeyEvent *ev)
{
    ActionEvent *c_ae, *ae = NULL;

    if ((c_ae = KeyGrabber::instance()->findAction(ev, _type)) != NULL) {
        list<Action>::iterator it(c_ae->action_list.begin());
        for (; it != c_ae->action_list.end(); ++it) {
            switch (it->getAction()) {
            case CMD_D_INSERT:
                bufAdd(ev);
                break;
            case CMD_D_REMOVE:
                bufRemove();
                break;
            case CMD_D_CLEAR:
                bufClear();
                break;
            case CMD_D_CLEARFROMCURSOR:
                bufKill();
                break;
            case CMD_D_EXEC:
                ae = exec();
                break;
            case CMD_D_CLOSE:
                ae = close();
                break;
            case CMD_D_COMPLETE:
                break;
            case CMD_D_CURS_NEXT:
                bufChangePos(1);
                break;
            case CMD_D_CURS_PREV:
                bufChangePos(-1);
                break;
            case CMD_D_CURS_BEGIN:
                _pos = 0;
                break;
            case CMD_D_CURS_END:
                _pos = _buf.size();
                break;
            case CMD_D_HIST_NEXT:
                histNext();
                break;
            case CMD_D_HIST_PREV:
                histPrev();
                break;
            case CMD_D_NO_ACTION:
            default:
                // do nothing, shouldn't happen
                break;
            };
        }

        // something ( most likely ) changed, redraw the window
        if (ae == NULL) {
            bufChanged();
            render();
        }
    }

    return ae;
}

//! @brief Handles ExposeEvent, redraw when ev->count == 0
ActionEvent*
CmdDialog::handleExposeEvent(XExposeEvent *ev)
{
    if (ev->count > 0) {
        return NULL;
    }
    render();
    return NULL;
}

//! @brief Maps the CmdDialog center on the PWinObj it executes actions on.
//! @param buf Buffer content.
//! @param focus Give input focus if true.
//! @param wo_ref PWinObj reference, defaults to NULL which does not update.
void
CmdDialog::mapCentered(const std::string &buf, bool focus, PWinObj *wo_ref)
{
    // Setup data
    _wo_ref = wo_ref ? wo_ref : _wo_ref;
    _hist_it = _hist_list.end();

    _buf = Util::to_wide_str(buf);
    _pos = _buf.size();
    bufChanged();

    // Update position
    moveCentered(_wo_ref);

    // Map and render
    PDecor::mapWindowRaised();
    render();

    // Give input focus if requested
    if (focus) {
        giveInputFocus();
    }
}

//! @brief Moves to center of wo.
//! @param wo PWinObj to center on.
void
CmdDialog::moveCentered(PWinObj *wo)
{
    // Fallback wo on root.
    if (!wo) {
        wo = PWinObj::getRootPWinObj();
    }

    // Make sure position is inside head.
    Geometry head;
    uint head_nr = PScreen::instance()->getNearestHead(wo->getX()
                                                       + (wo->getWidth() / 2),
                                                       wo->getY()
                                                       + (wo->getHeight() / 2));
    PScreen::instance()->getHeadInfo(head_nr, head);

    // Make sure X is inside head.
    int new_x = wo->getX() + (static_cast<int>(wo->getWidth())
                              - static_cast<int>(_gm.width)) / 2;
    if (new_x < head.x) {
        new_x = head.x;
    } else if ((new_x + _gm.width) > (head.x + head.width)) {
        new_x = head.x + head.width - _gm.width;
    }

    // Make sure Y is inside head.
    int new_y = wo->getY() + (static_cast<int>(wo->getHeight())
                              - static_cast<int>(_gm.height)) / 2;
    if (new_y < head.y) {
        new_y = head.y;
    } else if ((new_y + _gm.height) > (head.y + head.height)) {
        new_y = head.y + head.height - _gm.height;
    }

    // Update position.
    move(new_x, new_y);    
}

//! @brief Sets title of decor
void
CmdDialog::setTitle(const std::wstring &title)
{
    _title.setReal(title);
}

//! @brief Maps window, overloaded to refresh content of window after mapping.
void
CmdDialog::mapWindow(void)
{
    if (! _mapped) {
        PDecor::mapWindow();
        render();
    }
}

//! @brief Unmaps window, overloaded to clear buffer.
void
CmdDialog::unmapWindow(void)
{
    if (_mapped) {
        PDecor::unmapWindow();

        _wo_ref = NULL;
        bufClear();
    }
}

//! @brief Sets background and size
void
CmdDialog::loadTheme(void)
{
    // setup variables
    _cmd_data = _theme->getCmdDialogData();

    // setup size
    resizeChild(PScreen::instance()->getWidth() / 4,
                _cmd_data->getFont()->getHeight() +
                _cmd_data->getPad(PAD_UP) +
                _cmd_data->getPad(PAD_DOWN));

    // get pixmap
    PixmapHandler *pm = ScreenResources::instance()->getPixmapHandler();
    pm->returnPixmap(_bg);
    _bg = pm->getPixmap(_cmd_wo->getWidth(), _cmd_wo->getHeight(),
                        PScreen::instance()->getDepth());

    // render texture
    _cmd_data->getTexture()->render(_bg, 0, 0,
                                    _cmd_wo->getWidth(), _cmd_wo->getHeight());
    _cmd_wo->setBackgroundPixmap(_bg);
    _cmd_wo->clear();
}

//! @brief Frees resources
void
CmdDialog::unloadTheme(void)
{
    ScreenResources::instance()->getPixmapHandler()->returnPixmap(_bg);
}

//! @brief Renders _buf onto _cmd_wo
void
CmdDialog::render(void)
{
    _cmd_wo->clear();

    // draw buf content
    _cmd_data->getFont()->setColor(_cmd_data->getColor());

    _cmd_data->getFont()->draw(_cmd_wo->getWindow(),
                               _cmd_data->getPad(PAD_LEFT),
                               _cmd_data->getPad(PAD_UP),
                               _buf.c_str() + _buf_off, _buf_chars);

    // draw cursor
    uint pos = _cmd_data->getPad(PAD_LEFT);
    if (_pos > 0) {
        pos = _cmd_data->getFont()->getWidth(_buf.c_str() + _buf_off,
                                             _pos - _buf_off) + 1;
    }

    _cmd_data->getFont()->draw(_cmd_wo->getWindow(),
                               pos, _cmd_data->getPad(PAD_UP),
                               L"|");
}

//! @brief Generates ACTION_CLOSE.
//! @return Pointer to ActionEvent.
ActionEvent*
CmdDialog::close(void)
{
    _ae.action_list.back().setAction(ACTION_NO);

    return &_ae;
}

//! @brief Parses _buf and tries to generate an ActionEvent
//! @return Pointer to ActionEvent.
ActionEvent*
CmdDialog::exec(void)
{
    _hist_list.push_back(_buf);
    if (_hist_list.size() > 10) // FIXME: make configurable
        _hist_list.pop_front();

    // Check if it's a valid Action, if not we assume it's a command and try
    // to execute it.
    string buf_mb(Util::to_mb_str(_buf));
    if (!Config::instance()->parseAction(buf_mb, _ae.action_list.back(),
                                         KEYGRABBER_OK)) {
        _ae.action_list.back().setAction(ACTION_EXEC);
        _ae.action_list.back().setParamS(buf_mb);
    }

    return &_ae;
}

//! @brief Tab completion, complete word at cursor position.
void
CmdDialog::complete(void)
{
}

//! @brief Adds char to buffer
void
CmdDialog::bufAdd(XKeyEvent *ev)
{
    char c_return[64];
    memset(c_return, '\0', 64);

    XLookupString(ev, c_return, 64, NULL, NULL);

    // Add wide string to buffer counting position
    wstring buf_ret(Util::to_wide_str(c_return));
    for (unsigned int i = 0; i < buf_ret.size(); ++i) {
      if (iswprint(buf_ret[i])) {
        _buf.insert(_buf.begin() + _pos++, buf_ret[i]);
      }
    }
}

//! @brief Removes char from buffer
void
CmdDialog::bufRemove(void)
{
    if ((_pos > _buf.size()) || (_pos == 0) || (_buf.size() == 0)) {
        return;
    }

    _buf.erase(_buf.begin() + --_pos);
}

//! @brief Clears the buffer, resets status
void
CmdDialog::bufClear(void)
{
    _buf = L""; // old gcc doesn't know about .clear()
    _pos = _buf_off = _buf_chars = 0;
}

//! @brief Removes buffer content after cursor position
void
CmdDialog::bufKill(void)
{
    _buf.resize(_pos);
}

//! @brief Moves the marker
void
CmdDialog::bufChangePos(int off)
{
    if ((signed(_pos) + off) < 0) {
        _pos = 0;
    } else if (unsigned(_pos + off) > _buf.size()) {
        _pos = _buf.size();
    } else {
        _pos += off;
    }
}

//! @brief Recalculates, _buf_off and _buf_chars
void
CmdDialog::bufChanged(void)
{
    PFont *font =  _cmd_data->getFont(); // convenience

    // complete string doesn't fit in the window OR
    // we don't fit in the first set
    if ((_pos > 0)
        && (font->getWidth(_buf.c_str()) > _cmd_wo->getWidth())
        && (font->getWidth(_buf.c_str(), _pos) > _cmd_wo->getWidth())) {

        // increase position until it all fits
        for (_buf_off = 0; _buf_off < _pos; ++_buf_off) {
            if (font->getWidth(_buf.c_str() + _buf_off, _buf.size() - _buf_off)
                    < _cmd_wo->getWidth())
                break;
        }

        _buf_chars = _buf.size() - _buf_off;
    } else {
        _buf_off = 0;
        _buf_chars = _buf.size();
    }
}

//! @brief Sets the buffer to the next item in the history.
void
CmdDialog::histNext(void)
{
    if (_hist_it == _hist_list.end()) {
        return; // nothing to do
    }

    // get next item, if at the end, restore the edit buffer
    ++_hist_it;
    if (_hist_it == _hist_list.end()) {
        _buf = _hist_new;
    } else {
        _buf = *_hist_it;
    }

    // move cursor to the end of line
    _pos = _buf.size();
}

//! @brief Sets the buffer to the previous item in the history.
void
CmdDialog::histPrev(void)
{
    if (_hist_it == _hist_list.begin()) {
        return; // nothing to do
    }

    // save item so we can restore the edit buffer later
    if (_hist_it == _hist_list.end()) {
        _hist_new = _buf;
    }

    // get prev item
    _buf = *(--_hist_it);

    // move cursor to the end of line
    _pos = _buf.size();
}
