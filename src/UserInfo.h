/**
 * MicroLink EchoLink Station
 * Copyright (C) 2024, Bruce MacKinnon KC1FSZ
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * FOR AMATEUR RADIO USE ONLY.
 * NOT FOR COMMERCIAL USE WITHOUT PERMISSION.
 */
#ifndef _UserInfo_h
#define _UserInfo_h

namespace kc1fsz {

/**
 * An attempt to completely abstract the management of an EchoLink 
 * session from the hardware environment it runs in.
 */
class UserInfo {
public:

    /**
     * Used to set a short status/progress message.  Like "Connection in 
     * process ..."
    */
    virtual void setStatus(const char* msg) { }

    /**
     * Used to set the "oNDATA message" that comes from the other station.
    */
    virtual void setOnData(const char* msg) { }

    /**
     * Used to indicate when the receiver is actively receiving 
     * audio packets. Will only be called on transitions.
    */
    virtual void setSquelchOpen(bool sq) { }
};

}

#endif

