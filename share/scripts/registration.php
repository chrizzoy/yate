#!/usr/bin/php -q
<?php
/**
 * registration.php
 * This file is part of the YATE Project http://YATE.null.ro
 *
 * Yet Another Telephony Engine - a fully featured software PBX and IVR
 * Copyright (C) 2005-2012 Null Team
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* Test script for the Yate PHP interface
   To test add in extmodule.conf

   [scripts]
   test.php=
*/
require_once("libyate.php");

/* Always the first action to do */
Yate::Init();

/* Install a handler for the engine generated timer message */
//Yate::Install("engine.timer",10);
Yate::Install("user.auth",10);

/* Create and dispatch an initial test message */
/* The main loop. We pick events and handle them */
for (;;) {
    $ev=Yate::GetEvent();
    /* If Yate disconnected us then exit cleanly */
    if ($ev === false)
        break;
    /* Empty events are normal in non-blocking operation.
       This is an opportunity to do idle tasks and check timers */
    if ($ev === true) {
//        Yate::Output("PHP event: empty");
        continue;
    }
    /* If we reached here we should have a valid object */
    switch ($ev->type) {
	case "incoming":
	    $conn = pg_connect("host=127.0.0.1 dbname=yate user=postgres password=");
	    $query = pg_query ($conn, "SELECT username,password FROM yatet");
	    Yate::Output("Username : " . pg_fetch_result($query,0,0) . " password " . pg_fetch_result($query,0,1));
	    $ev->retval = pg_fetch_result($query,0,1);
	    $ev->handled = true;
	    /* This is extremely important.
	       We MUST let messages return, handled or not */
	    $ev->Acknowledge();
	    break;
	case "answer":
	    Yate::Output("PHP Answered: " . $ev->name . " id: " . $ev->id);
	    break;
	case "installed":
	    Yate::Output("PHP Installed: " . $ev->name);
	    break;
	case "uninstalled":
	    Yate::Output("PHP Uninstalled: " . $ev->name);
	    break;
	default:
	    Yate::Output("PHP Event: " . $ev->type);
    }
}

Yate::Output("PHP: bye!");

/* vi: set ts=8 sw=4 sts=4 noet: */
?>
