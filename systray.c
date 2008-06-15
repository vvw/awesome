/*
 * systray.c - systray handling
 *
 * Copyright © 2008 Julien Danjou <julien@danjou.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

#include "structs.h"
#include "systray.h"
#include "window.h"
#include "widget.h"
#include "common/xembed.h"
#include "common/swindow.h"

#define SYSTEM_TRAY_REQUEST_DOCK 0 /* Begin icon docking */

extern awesome_t globalconf;

void
systray_init(int phys_screen)
{
    xutil_intern_atom_request_t atom_systray_q, atom_manager_q;
    xcb_atom_t atom_systray;
    xcb_client_message_event_t ev;
    char atom_name[22];

    /* Send requests */
    atom_manager_q = xutil_intern_atom(globalconf.connection, &globalconf.atoms, atom_name);
    snprintf(atom_name, sizeof(atom_name), "_NET_SYSTEM_TRAY_S%d", phys_screen);
    atom_systray_q = xutil_intern_atom(globalconf.connection, &globalconf.atoms, atom_name);

    globalconf.screens[phys_screen].systray = simplewindow_new(globalconf.connection, phys_screen,
                                                               -1, -1, 1, 1, 0);

    /* Fill event */
    ev.format = 32;
    ev.data.data32[0] = XCB_CURRENT_TIME;
    ev.data.data32[2] = globalconf.screens[phys_screen].systray->window;
    ev.data.data32[3] = ev.data.data32[4] = 0;
    ev.response_type = xutil_intern_atom_reply(globalconf.connection,
                                               &globalconf.atoms, atom_manager_q);

    ev.data.data32[1] = atom_systray = xutil_intern_atom_reply(globalconf.connection,
                                                               &globalconf.atoms,
                                                               atom_systray_q);

    xcb_set_selection_owner(globalconf.connection,
                            globalconf.screens[phys_screen].systray->window,
                            atom_systray,
                            XCB_CURRENT_TIME);
}

/** Handle a systray request.
 * \param embed_win The window to embed.
 */
int
systray_request_handle(xcb_window_t embed_win, int phys_screen, xembed_info_t *info)
{
    xembed_window_t *em;
    int i;
    const uint32_t select_input_val[] =
    {
        XCB_EVENT_MASK_STRUCTURE_NOTIFY
            | XCB_EVENT_MASK_PROPERTY_CHANGE
            | XCB_EVENT_MASK_ENTER_WINDOW
    };


    xcb_change_window_attributes(globalconf.connection, embed_win, XCB_CW_EVENT_MASK,
                                 select_input_val);
    window_setstate(embed_win, XCB_WM_WITHDRAWN_STATE);

    em = p_new(xembed_window_t, 1);
    em->win = embed_win;
    em->phys_screen = phys_screen;

    if(info)
        em->info = *info;
    else
        xembed_info_get(globalconf.connection, em->win, &em->info);

    xembed_window_list_append(&globalconf.embedded, em);

    /** \todo we should create a dedicated window for that */
    if(globalconf.screens[phys_screen].systray)
        xembed_embedded_notify(globalconf.connection, em->win,
                                globalconf.screens[phys_screen].systray->window,
                                MIN(XEMBED_VERSION, em->info.version));

    if(em->info.flags & XEMBED_MAPPED)
       xcb_map_window(globalconf.connection, em->win);

    for(i = 0; i < globalconf.screens_info->nscreen; i++)
        widget_invalidate_cache(i, WIDGET_CACHE_EMBEDDED);

    return 0;
}

/** Handle systray message.
 * \param ev The event.
 * \return 0 on no error.
 */
int
systray_process_client_message(xcb_client_message_event_t *ev)
{
    int screen_nbr = 0;
    xcb_get_geometry_cookie_t geom_c;
    xcb_get_geometry_reply_t *geom_r;
    xcb_screen_iterator_t iter;

    switch(ev->data.data32[1])
    {
      case SYSTEM_TRAY_REQUEST_DOCK:
        geom_c = xcb_get_geometry(globalconf.connection, ev->window);

        if(!(geom_r = xcb_get_geometry_reply(globalconf.connection, geom_c, NULL)))
            return -1;

        for(iter = xcb_setup_roots_iterator(xcb_get_setup(globalconf.connection)), screen_nbr = 0;
            iter.rem && iter.data->root != geom_r->root; xcb_screen_next (&iter), ++screen_nbr);

        p_delete(&geom_r);

        systray_request_handle(ev->data.data32[2], screen_nbr, NULL);
        break;
    }
    return 0;
}

/** Handle xembed client message.
 * \param ev The event.
 * \return 0 on no error.
 */
int
xembed_process_client_message(xcb_client_message_event_t *ev)
{
    switch(ev->data.data32[1])
    {
      case XEMBED_REQUEST_FOCUS:
        xembed_focus_in(globalconf.connection, ev->window, XEMBED_FOCUS_CURRENT);
        break;
    }
    return 0;
}

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=80