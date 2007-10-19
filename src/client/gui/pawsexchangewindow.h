/*
 * pawsexchangewindow.h"
 *
 * Copyright (C) 2003 Atomic Blue (info@planeshift.it, http://www.atomicblue.org)
 *exi
 * Credits :    Andrew Craig <acraig@paqrat.com> 
 *          
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation (version 2
 * of the License).
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 */
#ifndef PAWS_EXCHANGE_WINDOW
#define PAWS_EXCHANGE_WINDOW

#include <csutil/csstring.h>

#include "net/subscriber.h"
#include "paws/pawswidget.h"
#include "util/psconst.h"
#include "gui/inventorywindow.h"

class MsgHandler;
class pawsMoney;

#define EXCHANGE_VISIBLE            4
#define EXCHANGE_VISIBLE_INVENTORY 16

enum psExchangeType
{
    exchangePC2PC,        /* exchange between our player and another player */
    exchangePC2NPC        /* exchange between our player and a non-player character
                                - in this case items are transfered only from our character to the NPC
                                - used for quests */
};

/** The trade window in PlaneShift.
 * This is the window that people will use to trade with each other.
 * There are 4 slots for items and 4 for money for both receiving and 
 * offering.  Whenever a person accepts their panel will change so each 
 * client knows.  If the trade is canceled no items are transfered.
 */
class pawsExchangeWindow : public pawsWidget, public psClientNetSubscriber
{
public:
    pawsExchangeWindow();
    virtual ~pawsExchangeWindow();

    //from pawsWidget:
    bool PostSetup();
    bool OnButtonPressed( int mouseButton, int keyModifier, pawsWidget* widget );
    void Close();
    
    //from iNetSubscriber:
    void HandleMessage( MsgEntry* me );
    
    /** Sends psExchangeAcceptMsg message to the server. */
    void SendAccept();

    /** Sends a psExchangeRejectMsg message to the server. */
    void SendEnd();

protected:

    /// Handles incomming messages that deal with exchange money.
    void HandleMoney( MsgEntry* me );
    
    /// Clear out the exchange window and reset the backgrounds.
    void Clear();

    /** Start a new exchange with a player.
      * @player The name of the player to trade with.
      * @withPlayer Is the user exchanging with other player, or with NPC ?
      */
    void StartExchange( csString& player, bool withPlayer );
        
    psExchangeType type;
    
    /// The background for the offering panel
    pawsWidget* offeringBG;
    
    /// The background for the receiving panel.
    pawsWidget* receivingBG;
    
    pawsMoney * offeringMoneyWidget, * receivingMoneyWidget;

    //Text that shows the total amount of trias
    pawsTextBox * totalTriasOffered, *totalTriasReceived;

    csRef<MsgHandler> msgHandler;
    
    
    /// List of slots for items that have been offered.
    pawsSlot* offeringSlots[EXCHANGE_SLOT_COUNT];
    
    /// List of slots for items that will be receiving. 
    pawsSlot* receivingSlots[EXCHANGE_SLOT_COUNT];
}; 


CREATE_PAWS_FACTORY( pawsExchangeWindow );



#endif
