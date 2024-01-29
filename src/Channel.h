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
#ifndef _Channel_h
#define _Channel_h

namespace kc1fsz {

class Channel {
public:

    Channel() : _id(0) { }
    Channel(const Channel& that) : _id(that._id), _isGood(that._isGood) { }
    Channel(int id, bool isGood = true) : _id(id), _isGood(isGood) { }
    int getId() const { return _id; }
    bool isGood() const { return _isGood; }
    bool operator== (const Channel& that) { return _id == that._id && _isGood == that._isGood; }

private:

    int _id;
    bool _isGood;
};

}

#endif
