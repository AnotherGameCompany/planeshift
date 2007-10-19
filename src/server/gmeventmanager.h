/** gmeventmanager.h
 *
 * Copyright (C) 2006 Atomic Blue (info@planeshift.it, http://www.atomicblue.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation (version 2 of the License)
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Manages Game Master events for players.
 */

#ifndef __GMEVENTMANAGER_H__
#define __GMEVENTMANAGER_H__

#include <csutil/ref.h>
#include <csutil/csstring.h>
#include <csutil/array.h>
#include "msgmanager.h"
#include "net/msghandler.h"
#include "bulkobjects/psitem.h"

#define MAX_PLAYERS_PER_EVENT 100
#define MAX_EVENT_NAME_LENGTH 40
#define MAX_REGISTER_RANGE    100.0

#define NO_RANGE              -1.0

enum GMEventStatus
{
    EMPTY = 0,                 // no GM event
    RUNNING,                   // GM event is running
    COMPLETED                  // GM event is complete
};
 
/** GameMaster Events manager class.
 */
class GMEventManager : public MessageManager
{
public:
    GMEventManager();
    ~GMEventManager();

    bool Initialise(void);

    /** GM attempts to add new event.
     *
     * @param client: client pointer.
     * @param eventName: event name.
     * @param eventDescription: event description.
     * @return bool: true = success, false = failed.
     */
    bool AddNewGMEvent (Client* client, csString eventName, csString eventDescription);

    /** GM registers player into his/her event.
     *
     * @param clientnum: client number of GM.
     * @param gmID: GM player ID
     * @param target: registeree player client.
     * @return bool: true = success, false = failed.
     */
    bool RegisterPlayerInGMEvent (int clientnum, int gmID, Client* target);
   
    /** GM registers all players in range.
     *
     * @param client: client pointer.
     * @param range: required range.
     * @return bool: true = success, false = failed.
     */
    bool RegisterPlayersInRangeInGMEvent (Client* client, float range);

    /** A player completes an event.
     *
     * @param client: client pointer.
     * @param gmID: Game Master player ID.
     * @return bool: true = success, false = failed.
     */
    bool CompleteGMEvent (Client* client, int gmID);

    /** A player is removed from a running event.
     * A player can be excused from finishing an event.
     * @param clientnum: client number
     * @param gmID: game master player id.
     * @param target: registeree player client to be removed.
     * @return bool: true = success, false = failed.
     */
    bool RemovePlayerFromGMEvent (int clientnum, int gmID, Client* target);

    /** Reward players who complete an event (NB event must be live at the time
     *  of reward).
     *
     * @param client: client pointer.
     * @param range: required range of winners (NO_RANGE = all participants).
     * @param stackCount: number of items to reward.
     * @param itemName: name of the reward item.
     * @return bool: true = success, false = failed.
     */
    bool RewardPlayersInGMEvent (Client* client,
                                 float range,
                                 unsigned short stackCount,
                                 csString itemName);

    /** Returns all events for a player.
     * 
     * @param playerID: the player identity.
     * @param completedEvents: array of event ids of completed events player.
     * @param runningEventAsGM: running event id as GM.
     * @param completedEventsAsGM: array of ids of completed events as GM.
     * @return int: running event id.
     */
    int GetAllGMEventsForPlayer (int playerID,
                                 csArray<int>& completedEvents,
                                 int& runningEventAsGM,
                                 csArray<int>& completedEventsAsGM);

    /** Returns event details for a particular event.
     *
     * @param id: event id.
     * @param name: name of event.
     * @param description: description of event.
     * @returns GMEventStatus: status of event.
     */
     GMEventStatus GetGMEventDetailsByID (int id,
                                          csString& name,
                                          csString& description);

     virtual void HandleMessage(MsgEntry *me, Client *client); 

     /** Removes a player from any GM event they maybe involved with (eg player being deleted)
      *
      * @param playerID: id of player being removed
      * @returns bool: true = success
      */
     bool RemovePlayerFromGMEvents(int playerID);

private:

    int nextEventID;
   
    struct GMEvent
    {
        int id;
        GMEventStatus status;
        unsigned int gmID;
        csString eventName;
        csString eventDescription;
        csArray<unsigned int> playerID;
    };
    csArray<GMEvent*> gmEvents;    /// cache of GM events

    /** find a GM event by its id.
     *
     * @param id: id of event.
     * @return GMEvent*: ptr to GM event structure.
     */
    GMEvent* GetGMEventByID(int id);
    
    /** find a particular GM's event from them all.
     *
     * @param gmID: player id of the GM.
     * @param status: event of status looked for.
     * @param startIndex: start index into array.
     * @return GMEvent*: ptr to GM event structure.
     */
    GMEvent* GetGMEventByGM(unsigned int gmID, GMEventStatus status, int& startIndex);

    /** Find any event that a player may be/was registered to, returns index.
     *
     * @param playerID: the player index.
     * @param status: event of status looked for.
     * @param startIndex: start index into array.
     * @return GMEvent*: ptr to GM event structure.
     */
    GMEvent* GetGMEventByPlayer(unsigned int playerID, GMEventStatus status, int& startIndex);

    /** Reward player in event.
     * 
     * @param target: client pointer to recipient.
     * @return bool: true = success, false = failed.
     */
    bool RewardPlayer(Client* target, unsigned short stackCount, psItemStats* basestats);

    /** Get next free event id number.
     */
    int GetNextEventID(void);
};

#endif

