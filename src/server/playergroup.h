/*
 * playergroup.h by Anders Reggestad <andersr@pvv.org>
 *
 * Copyright (C) 2004 Atomic Blue (info@planeshift.it, http://www.atomicblue.org) 
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation (version 2 of the License)
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
#ifndef __PLAYERGROUP_H__
#define __PLAYERGROUP_H__

#include <csutil/ref.h>
#include <csutil/refarr.h>
#include <csutil/refcount.h>
#include <csutil/array.h>


class gemActor;
class GroupManager;
class Client;
class MsgEntry;

/** An existing group.
 * This defines a 'group' of players on the server.
 */
class PlayerGroup : public csRefCount
{
public:
    /// The main server group manager.
    GroupManager      *manager;

    /// The online client that is the current leader of the group.
    gemActor        *leader;
    int                id;
    static int         next_id;

    // TODO: Client should use csRefArray but then the client have to
    // be ref counted.
    csArray<gemActor*> members;

    PlayerGroup();
    PlayerGroup(GroupManager * mgr, gemActor * leader);
    ~PlayerGroup();

    /// Add a new client to the existing group.
    void Add(gemActor * new_member);

    /// Remove a client from this group.
    void Remove(gemActor * member);

    /// Send a message to all members in this group.
    void Broadcast(MsgEntry *me);
    void ListMembers(gemActor * client);
    bool IsLeader(gemActor * client);
    void Disband();
    bool IsEmpty();
    void BroadcastMemberList();
    size_t  GetMemberCount() { return members.GetSize(); }
    gemActor *GetMember(size_t which) { return members[which]; }
    bool HasMember(gemActor *member);

    int operator==(PlayerGroup& other)
    {
        return id == other.id;
    }
    
    int operator<(PlayerGroup& other)
    {
        return id < other.id;
    }
    
};

#endif
