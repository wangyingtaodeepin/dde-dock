/**
 * Copyright (C) 2015 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#ifndef XCB_MISC_H
#define XCB_MISC_H

#include <QtCore>

#include <xcb/xcb_ewmh.h>

class XcbMisc
{

public:
    enum Orientation {
        OrientationLeft,
        OrientationRight,
        OrientationTop,
        OrientationBottom
    };

    enum WindowType {
        Dock,
        Desktop
    };

    virtual ~XcbMisc();

    static XcbMisc * instance();

    void set_window_type(xcb_window_t winId, WindowType winType);
    void clear_strut_partial(xcb_window_t winId);
    void set_strut_partial(xcb_window_t winId, Orientation orientation, uint strut, uint start, uint end);
    void set_window_icon_geometry(xcb_window_t winId, QRect geo);

private:
    XcbMisc();

    xcb_ewmh_connection_t m_ewmh_connection;
};

#endif // XCB_MISC_H
