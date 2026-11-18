/*
 *
 * Copyright (C) 2009  Daniel Aquino
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
 * with this program; if not, see <http://www.gnu.org/licenses/>, or write
 * to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#pragma once

/* net_tracker.h -- announcements to the game tracker (master server)
 *
 * net_tracker.cpp (PC) sends one-line UDP text messages and reads nothing
 * back: "hosting <port> <version> <host name>[,<player>...] <level>\n" while
 * hosting and "finished <port>\n" when the game ends. They go out from the
 * enet host's own socket, so the tracker sees the public address and port
 * that joining players will connect to; with no enet host nothing is sent.
 * networking.cpp's update_tracker resends the update every 20 seconds while
 * this machine hosts with the tracker enabled. stubs_3ds.cpp makes both calls
 * no-ops on 3DS.
 */

void send_tracker_update( char* host, int port );
void send_tracker_finished( char* host, int port );
