/*
 * exchangemanager.cpp
 *
 * Copyright (C) 2001 Atomic Blue (info@planeshift.it, http://www.atomicblue.org)
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

#include <psconfig.h>

#include "bulkobjects/pscharacterloader.h"
#include "bulkobjects/dictionary.h"
#include "bulkobjects/psitem.h"
#include "bulkobjects/psglyph.h"
#include "bulkobjects/psnpcdialog.h"
#include "util/log.h"
#include "util/psconst.h"
#include "util/psstring.h"
#include "util/eventmanager.h"
#include "globals.h"
#include "psserver.h"
#include "psserverchar.h"
#include "clients.h"
#include "gem.h"
#include "chatmanager.h"
#include "cachemanager.h"
#include "playergroup.h"

#include "exchangemanager.h"
#include "invitemanager.h"

/** Question client. Handles YesNo response of client for trade request. */
class PendingTradeInvite : public PendingInvite
{
public:
    PendingTradeInvite( Client *inviter, Client *invitee, 
        const char *question ) : PendingInvite( inviter, invitee, true, 
        question, "Accept", "Reject", 
        "You ask %s to trade with you.",
        "%s asks to trade with you.",
        "%s agrees to trade with you.",
        "You agree to trade.",
        "%s does not want to trade.",
        "You refuse to trade with %s.",
        psQuestionMessage::generalConfirm )
    {
        
    }

    void HandleAnswer(const csString & answer)
    {
        Client * invitedClient = psserver->GetConnections()->Find(clientnum);
        if ( !invitedClient )
            return;

        PendingInvite::HandleAnswer(answer);

        Client * inviterClient = psserver->GetConnections()->Find(inviterClientNum);
        if ( !inviterClient )
            return;
        
        psCharacter* invitedCharData = invitedClient->GetCharacterData();
        psCharacter* inviterCharData = inviterClient->GetCharacterData();
        const char* invitedCharFirstName = invitedCharData->GetCharName();
        const char* inviterCharFirstName = inviterCharData->GetCharName();

        if ( answer == "no" )
        {
            // Inform proposer that the trade has been rejected
            csString message;
            message.Format( "%s has rejected your trade offer.", invitedCharFirstName );
            psserver->SendSystemError( inviterClient->GetClientNum(),
                message.GetData() );
                        
            return;
        }

        // Ignore the answer if the invited client is busy with something else
        if (invitedClient->GetActor()->GetMode() != PSCHARACTER_MODE_PEACE && invitedClient->GetActor()->GetMode() != PSCHARACTER_MODE_SIT && invitedClient->GetActor()->GetMode() != PSCHARACTER_MODE_OVERWEIGHT)
        {
            csString err;
            err.Format("You can't trade while %s.",invitedClient->GetCharacterData()->GetModeStr());
            psserver->SendSystemError(invitedClient->GetClientNum(), err);

            // Not telling the inviter what the invited client is doing...
            psserver->SendSystemError(inviterClient->GetClientNum(), "%s is already busy with something else.", invitedCharFirstName);
            return;
        }

        // Ignore the answer if the invited client is already in trade
        if (invitedClient->GetExchangeID())
        {
            psserver->SendSystemError(invitedClient->GetClientNum(), "You are already busy with another trade");
            psserver->SendSystemError(inviterClient->GetClientNum(), "%s is already busy with another trade", invitedCharFirstName);
            return;
        }
        // Ignore the answer if the inviter client is already in trade
        if (inviterClient->GetExchangeID())
        {
            psserver->SendSystemError(inviterClient->GetClientNum(), "You are already busy with another trade");
            psserver->SendSystemError(invitedClient->GetClientNum(), "%s is already busy with another trade", inviterCharFirstName);
            return;
        }

        Exchange* exchange = new PlayerToPlayerExchange( inviterClient, invitedClient,
                                                         psserver->GetExchangeManager() );

        // Check range
        if ( !exchange->CheckRange(inviterClient->GetClientNum(), 
            inviterClient->GetActor(), invitedClient->GetActor()) )
        {
            psserver->SendSystemError(inviterClient->GetClientNum(), 
                "%s is too far away to trade.", invitedCharFirstName );
            delete exchange;
            return;
        }

        // Overwrite client trading state
        bool oldTradingStopped = inviterCharData->SetTradingStopped(false);
        if ( !inviterCharData->ReadyToExchange() )
        {
            psserver->SendSystemInfo( inviterClient->GetClientNum(), 
                "You are not ready to trade." );
            inviterCharData->SetTradingStopped( oldTradingStopped );
            delete exchange;
            return;
        }

        inviterCharData->SetTradingStopped( oldTradingStopped );

        if ( !invitedCharData->ReadyToExchange() )
        {
            psserver->SendSystemInfo( inviterClient->GetClientNum(), 
                "Target is not ready to trade." );
            delete exchange;
            return;
        }

        inviterClient->SetExchangeID( exchange->GetID() );
        invitedClient->SetExchangeID( exchange->GetID() );

        psserver->GetExchangeManager()->AddExchange( exchange );
    }
};


/** Sends text message describing change of offers to user 'clientNum'.
  * otherName - name of the other character (the person that the user exchanges with)
  * object - item that was moved (e.g. "2x sword")
  * actionOfClient - did the user that we send the message to do the operation ? (or the other character?)
  * movedToOffer - was the object moved to offered stuff ? (or removed from offered stuff and returned to inventory?)
  */
/*************
void SendTextDescription(int clientNum, const csString & otherName, const csString & object, bool actionOfClient, bool movedToOffer)
{
    psString text;

    if (actionOfClient)
    {
        if (movedToOffer)
            text.AppendFmt("You have just offered %s to %s.", object.GetData(), otherName.GetData());
        else
            text.AppendFmt("You have just removed %s from offer.", object.GetData());
    }
    else
    {
        if (movedToOffer)
            text.AppendFmt("%s has just offered %s to you.", otherName.GetData(), object.GetData());
        else
            text.AppendFmt("%s has just removed %s from offer.", otherName.GetData(), object.GetData());
    }
    psserver->SendSystemInfo(clientNum, text);
}

csString MakeMoneyObject(int coin, int count)
{
    csString object;

    object = count;
    object += "x ";

    switch (coin)
    {
        case MONEY_TRIAS: object += "tria"; break;
        case MONEY_OCTAS: object += "octa"; break;
        case MONEY_HEXAS: object += "hexa"; break;
        case MONEY_CIRCLES: object += "circle"; break;
    }
    return object;
}

void SendTextDescriptionOfMoney(int clientNum, const csString & otherName, psMoney oldMoney, psMoney newMoney, bool actionOfClient)
{
    csString object;

    if (oldMoney.GetTrias() != newMoney.GetTrias())
        object = MakeMoneyObject(MONEY_TRIAS, abs(oldMoney.GetTrias() - newMoney.GetTrias()));
    else if (oldMoney.GetOctas() != newMoney.GetOctas())
        object = MakeMoneyObject(MONEY_OCTAS, abs(oldMoney.GetOctas() - newMoney.GetOctas()));
    else if (oldMoney.GetHexas() != newMoney.GetHexas())
        object = MakeMoneyObject(MONEY_HEXAS, abs(oldMoney.GetHexas() - newMoney.GetHexas()));
    else if (oldMoney.GetCircles() != newMoney.GetCircles())
        object = MakeMoneyObject(MONEY_CIRCLES, abs(oldMoney.GetCircles() - newMoney.GetCircles()));

    SendTextDescription(clientNum, otherName, object, actionOfClient, newMoney > oldMoney);
}
***************/




bool Exchange::CheckRange(int clientNum, gemObject * ourActor, gemObject * otherActor)
{
    if (ourActor->RangeTo(otherActor) > RANGE_TO_SELECT)
    {
        psserver->SendSystemInfo(clientNum, "You are not in range to trade with %s.", otherActor->GetName());
        return false;
    }
    return true;
}

/***********************************************************************************
*
*                              class ExchangingCharacter
*
***********************************************************************************/


ExchangingCharacter::ExchangingCharacter(int clientNum,psCharacterInventory& inv)
{
    client = clientNum;
    chrinv = &inv;
}

bool ExchangingCharacter::OfferItem(int exchSlotNum, INVENTORY_SLOT_NUMBER invSlot, int stackcount, bool test)
{
    if (exchSlotNum>=EXCHANGE_SLOT_COUNT )
        return false;

    psCharacterInventory::psCharacterInventoryItem *itemInSlot = chrinv->FindExchangeSlotOffered(exchSlotNum);
    if (itemInSlot == NULL) // slot is open
    {
        if (!test)
        {
            chrinv->SetExchangeOfferSlot(NULL,invSlot,exchSlotNum,stackcount);
        }
        return true;
    }
    else
    {
        return false;
    }
}


bool ExchangingCharacter::RemoveItemFromOffer(int exchSlotNum, int count, int& remain)
{
    if (exchSlotNum >= EXCHANGE_SLOT_COUNT )
       return false;

    psCharacterInventory::psCharacterInventoryItem *item = chrinv->FindExchangeSlotOffered(exchSlotNum);
    if (item == NULL)
        return false;

    if (count == -1)
        count = item->exchangeStackCount;  // stack count of offer, not of item

    if (item->exchangeStackCount <= count)
    {
        item->exchangeStackCount = 0; // take out of offering array
        item->exchangeOfferSlot = -1;
    }
    else
    {
        item->exchangeStackCount -= count;
        remain = item->exchangeStackCount;
    }
    return true;
}

bool ExchangingCharacter::MoveOfferedItem(int fromSlot,int stackcount,int toSlot)
{
    psCharacterInventory::psCharacterInventoryItem *item = chrinv->FindExchangeSlotOffered(fromSlot);
    if (item == NULL)
        return false;

    psCharacterInventory::psCharacterInventoryItem *item2 = chrinv->FindExchangeSlotOffered(toSlot);

    item->exchangeOfferSlot = toSlot;
    if (item2)
    {
        item2->exchangeOfferSlot = fromSlot;  // swap if dest slot is occupied
        return true; // true if swapped
    }
    return false;
}


psCharacter *ExchangingCharacter::GetClientCharacter(int clientNum)
{
    Client * client = psserver->GetConnections()->Find(clientNum);
    if (client == NULL)
        return NULL;

    return client->GetCharacterData();
}



void ExchangingCharacter::TransferMoney( int targetClientNum )
{
    psCharacter *target; //,*cclient;

    if ( targetClientNum == 0 )
        return;

    target =  GetClientCharacter(targetClientNum);

    if (target == NULL)
        return;

    TransferMoney(target);
}

void ExchangingCharacter::TransferMoney( psCharacter *target )
{
    psMoney money;
    money.Set( offeringMoney.Get(MONEY_CIRCLES),offeringMoney.Get(MONEY_OCTAS),offeringMoney.Get(MONEY_HEXAS), offeringMoney.Get(MONEY_TRIAS) );

    target->AdjustMoney( money, false );
    
    // Now negate and take away from offerer
    money.Set( -offeringMoney.Get(MONEY_CIRCLES),-offeringMoney.Get(MONEY_OCTAS),-offeringMoney.Get(MONEY_HEXAS), -offeringMoney.Get(MONEY_TRIAS) );
    chrinv->owner->AdjustMoney( money, false );

    offeringMoney.Set(0, 0, 0, 0);
}

void ExchangingCharacter::TransferOffer(int targetClientNum)
{
    // printf("In ExchangingCharacter::TransferOffer(%d)...\n",targetClientNum);

    psCharacter * me = GetClientCharacter(client);
    psCharacter * target;

    if (targetClientNum != 0)
    {
        target = GetClientCharacter(targetClientNum);
        if (target == NULL)
        {
            // printf("  targetClient is not found.\n");
            return;
        }
        else
        {
            // printf("  targetClient is %s.\n",target->GetCharName() );
        }
    }
    else
    {
        // printf("  targetClient is cleared.\n",target->GetCharName() );
        target = NULL;
    }

    if (target != NULL)
    {
        for (int i=0; i < EXCHANGE_SLOT_COUNT; i++)
        {
            psCharacterInventory::psCharacterInventoryItem *itemInSlot = chrinv->FindExchangeSlotOffered(i);
            psItem *item = (itemInSlot) ? itemInSlot->GetItem() : NULL;

            if (item != NULL)
            {
                printf("  Handling item %s now.\n",item->GetName() );
                // printf("    Moving item to inventory for %s.\n", target->GetCharName() );
                psItem *newItem = item->CreateNew();
                item->Copy(newItem);
                newItem->SetStackCount(itemInSlot->exchangeStackCount);

                if (!target->Inventory().Add(newItem))
                {
                    // TODO: Probably rollback here if anything failed
                    printf("    Could not add item to inv, so dropping on ground.\n");

                    // Drop item on ground!!
                    target->DropItem( newItem );

                    csString buf;
                    buf.Format("%s, %s, %s, \"%s\", %u, %d",
                                me->GetCharFullName(),
                                target->GetCharFullName(),
                                "P2P - Item drop",
                                newItem->GetName(),
                                (unsigned int)newItem->GetStackCount(),
                                0);

                    psserver->GetLogCSV()->Write(CSV_EXCHANGES, buf);
                }
            }
        }
        // Money is handled in TransferMoney
    }
    chrinv->PurgeOffered(); // Now delete anything we just put in the other guy's inv
}


void ExchangingCharacter::TransferOffer(psCharacter *npc)
{
    chrinv->PurgeOffered();  // npc's just destroy stuff
}

void ExchangingCharacter::AdjustMoney(int type, int delta)
{
    offeringMoney.Adjust( type, delta );
}

void ExchangingCharacter::AdjustMoney(const psMoney& amount)
{
    offeringMoney += amount;
}

void ExchangingCharacter::UpdateReceivingMoney( psMoney& money )
{
    psExchangeMoneyMsg message( client, CONTAINER_RECEIVING_MONEY,
                             money.Get(MONEY_TRIAS),
                             money.Get(MONEY_HEXAS),
                             money.Get(MONEY_CIRCLES),
                             money.Get(MONEY_OCTAS) );
    message.SendMessage();
}

void ExchangingCharacter::UpdateOfferingMoney()
{
    psExchangeMoneyMsg message( client, CONTAINER_OFFERING_MONEY,
                             offeringMoney.Get(MONEY_TRIAS),
                             offeringMoney.Get(MONEY_HEXAS),
                             offeringMoney.Get(MONEY_CIRCLES),
                             offeringMoney.Get(MONEY_OCTAS) );
    message.SendMessage();
}

psItem *ExchangingCharacter::GetOfferedItem(int slot)
{
    psCharacterInventory::psCharacterInventoryItem *itemInSlot = chrinv->FindExchangeSlotOffered(slot);
    psItem *item = (itemInSlot) ? itemInSlot->GetItem() : NULL;
    return item;
}


void ExchangingCharacter::GetOffering(csString& buff)
{
    csString moneystr = offeringMoney.ToString();
    buff.Format("<L MONEY=\"%s\">", moneystr.GetData() );
    unsigned int z;

    for ( z = 0; z < EXCHANGE_SLOT_COUNT; z++ )
    {
        psCharacterInventory::psCharacterInventoryItem *itemInSlot = chrinv->FindExchangeSlotOffered(z);
        psItem *item = (itemInSlot) ? itemInSlot->GetItem() : NULL;
        if (item)
        {
            csString itemStr;
            itemStr.Format(
                        "<ITEM N=\"%s\" "
                        "SLT_POS=\"%d\" "
                        "C=\"%d\" "
                        "WT=\"%.2f\" "
                        "IMG=\"%s\"/>",
                        item->GetName(),
                        z,
                        itemInSlot->exchangeStackCount,
                        0.0, //////////item->GetSumWeight(),
                        item->GetImageName());

                buff.Append(itemStr);
        }
    }

    buff.Append("</L>");
}

void ExchangingCharacter::GetSimpleOffering(csString& buff, psCharacter *chr, bool exact)
{
    csString moneystr = offeringMoney.ToString();
    if (exact)
    {
        buff.Format("<L MONEY=\"%s\">", moneystr.GetData() );
    }
    else
    {
        buff.Format("<L MONEY=\"0,0,0,%d\">", offeringMoney.GetTotal());        
    }
    
    
    unsigned int z;

    for ( z = 0; z < EXCHANGE_SLOT_COUNT; z++ )
    {
        psCharacterInventory::psCharacterInventoryItem *itemInSlot = chrinv->FindExchangeSlotOffered(z);
        psItem *item = (itemInSlot) ? itemInSlot->GetItem() : NULL;
        if (item)
        {
            csString itemStr;
//            if ((item->GetCrafterID() == 0) || (item->GetCrafterID() != chr->GetActor()->GetPlayerID()))
//            {
                itemStr.Format("<ITEM N=\"%s\" "
                           "C=\"%d\" />",
                           item->GetName(),
                           itemInSlot->exchangeStackCount);
//            }
//            else
//            {
//                itemStr.Format("<ITEM N=\"%s\" "
//                           "C=\"%d\" Q=\"%s\" />",
//                           item->GetName(),
//                           itemInSlot->exchangeStackCount,
//                           item->GetQualityString());
//            }
            buff.Append(itemStr);
        }
    }

    buff.Append("</L>");
}

/*
float ExchangingCharacter::GetSumSize()
{
    float size = 0.0;
    
    psItem *item = NULL;
    
    for (int i=0; i < EXCHANGE_SLOT_COUNT; i++)
    {
        psCharacterInventory::psCharacterInventoryItem *itemInSlot = chrinv->FindExchangeSlotOffered(i);
        psItem *item = (itemInSlot) ? itemInSlot->GetItem() : NULL;
        if (item != NULL)    
        {
            size += item->GetTotalStackSize();        
        }
    }        
    return size;                        
}


float ExchangingCharacter::GetSumWeight()
{
    float weight = 0.0;    
    psItem *item = NULL;
    
    for (int i=0; i < EXCHANGE_SLOT_COUNT; i++)
    {
        psCharacterInventory::psCharacterInventoryItem *itemInSlot = chrinv->FindExchangeSlotOffered(i);
        psItem *item = (itemInSlot) ? itemInSlot->GetItem() : NULL;
        if (item != NULL)    
        {
            weight += item->GetWeight();        
        }
    }        
    return weight;                        
}
*/

void ExchangingCharacter::GetOfferingCSV(csString& buff)
{
    buff.Empty();
    for (int i=0; i<EXCHANGE_SLOT_COUNT; i++)
    {
        psCharacterInventory::psCharacterInventoryItem *itemInSlot = chrinv->FindExchangeSlotOffered(i);
        psItem *item = (itemInSlot) ? itemInSlot->GetItem() : NULL;
        if (item)
            buff.AppendFmt("%s, ",item->GetName() );
    }
}

bool ExchangingCharacter::GetExchangedItems(csString& text)
{
    text.Clear();

    // RMS: Loop through all items offered
    for (int i=0; i < EXCHANGE_SLOT_COUNT; i++)
    {
        psCharacterInventory::psCharacterInventoryItem *itemInSlot = chrinv->FindExchangeSlotOffered(i);
        psItem *item = (itemInSlot) ? itemInSlot->GetItem() : NULL;
        if (item != NULL)
        {
            text += item->GetQuantityName(item->GetName(),itemInSlot->exchangeStackCount);
            text += ", ";
        }
    }

    // RMS: Add the money count
    if (offeringMoney.GetTotal() > 0)
    {
        text.AppendFmt("%i Trias, ", offeringMoney.GetTotal());
    }
    if (!text.IsEmpty())
    {
        text.Truncate(text.Length() - 2);
        return true;
    }

    return false;
}








/***********************************************************************************
*
*                              class Exchange
*
***********************************************************************************/

/**
 * Common base for different kinds of exchanges
 */

Exchange::Exchange(Client* starter, csRef<ExchangeManager> manager)
        : starterChar( starter->GetClientNum(), starter->GetCharacterData()->Inventory() )
{
    id = next_id++;
    exchangeEnded = false;
    exchangeSuccess = false;
    exchangeMgr = manager;
    starterClient = starter;
    this->player = starter->GetClientNum();

    starter->GetActor()->RegisterCallback( this );
    starter->GetCharacterData()->Inventory().BeginExchange();
}

Exchange::~Exchange()
{
    // printf("In ~Exchange()\n");

    // Must first get rid of exchange references because vtable is now deleted
    starterClient->SetExchangeID(0);
    if ( !exchangeSuccess )
    {
        starterClient->GetCharacterData()->Inventory().RollbackExchange();
        psserver->GetCharManager()->SendInventory(player);
    }

    SendEnd( player );
}

bool Exchange::AddItem(Client* fromClient, INVENTORY_SLOT_NUMBER fromSlot, int stackCount, int toSlot)
{
    psCharacterInventory::psCharacterInventoryItem *invItem = fromClient->GetCharacterData()->Inventory().GetCharInventoryItem(fromSlot);
    if (!invItem)
        return false;

    SendAddItemMessage(fromClient, toSlot, invItem);

    return true;
}

void Exchange::SendAddItemMessage(Client* fromClient, int slot, psCharacterInventory::psCharacterInventoryItem* invItem)
{
    psItem* item = invItem->GetItem();
    psExchangeAddItemMsg msg(fromClient->GetClientNum(), item->GetName(),
                             CONTAINER_EXCHANGE_OFFERING, slot,
                             invItem->exchangeStackCount, item->GetImageName());
    psserver->GetEventManager()->SendMessage(msg.msg);
}

void Exchange::SendRemoveItemMessage(Client* fromClient, int slot)
{
    psExchangeRemoveItemMsg msg(fromClient->GetClientNum(), CONTAINER_EXCHANGE_OFFERING, slot, 0);
    psserver->GetEventManager()->SendMessage(msg.msg);
}

void Exchange::MoveItem(Client* fromClient, int fromSlot, int stackCount, int toSlot)
{
    psCharacterInventory &inv = fromClient->GetCharacterData()->Inventory();
    psCharacterInventory::psCharacterInventoryItem *srcItem  = inv.FindExchangeSlotOffered(fromSlot);
    psCharacterInventory::psCharacterInventoryItem *destItem = inv.FindExchangeSlotOffered(toSlot);

    srcItem->exchangeOfferSlot = toSlot;

    if (destItem)
        destItem->exchangeOfferSlot = fromSlot; // swap if destination slot is occupied

    SendRemoveItemMessage(fromClient, fromSlot);
    if (destItem)
    {
        SendRemoveItemMessage(fromClient, toSlot);
        SendAddItemMessage(fromClient, fromSlot, destItem);
    }
    SendAddItemMessage(fromClient, toSlot, srcItem);
}

psMoney Exchange::GetOfferedMoney(Client *client)
{
    CS_ASSERT(client == starterClient);
    return starterChar.GetOfferedMoney();
}


bool Exchange::AdjustMoney( Client* client, int moneyType, int newMoney )
{
    if ( client == starterClient )
    {
        starterChar.AdjustMoney( moneyType, newMoney);
        starterChar.UpdateOfferingMoney();
        return true;
    }
    return false;
}

bool Exchange::AdjustMoney( Client* client, const psMoney& amount )
{
    if ( client == starterClient )
    {
        starterChar.AdjustMoney( amount);
        starterChar.UpdateOfferingMoney();
        return true;
    }
    return false;
}

bool Exchange::RemoveItem(Client* fromClient, int fromSlot, int count)
{
    psCharacterInventory::psCharacterInventoryItem *invItem = fromClient->GetCharacterData()->Inventory().FindExchangeSlotOffered(fromSlot);
    if (!invItem)
        return false;

    if (count == -1)
        count = invItem->exchangeStackCount;  // stack count of offer, not of item

    if (invItem->exchangeStackCount <= count)
    {
        invItem->exchangeStackCount = 0; // take out of offering array
        invItem->exchangeOfferSlot = -1;
        SendRemoveItemMessage(fromClient, fromSlot);
    }
    else
    {
        invItem->exchangeStackCount -= count;
        SendRemoveItemMessage(fromClient, fromSlot);
        SendAddItemMessage(fromClient, fromSlot, invItem);
    }
    return true;
}


void Exchange::SendEnd(int clientNum)
{
    // printf("In Exchange::SendEnd(%d)\n",clientNum);

    psExchangeEndMsg msg(clientNum);
    psserver->GetEventManager()->SendMessage(msg.msg);
}

psItem* Exchange::GetStarterOffer(int slot)
{
    if (slot < 0 || slot >= EXCHANGE_SLOT_COUNT)
        return NULL;

    return starterChar.GetOfferedItem(slot);
}

int Exchange::next_id = 1;

//------------------------------------------------------------------------------

PlayerToPlayerExchange::PlayerToPlayerExchange(Client* player, Client* target, csRef<ExchangeManager> manager)
                       :Exchange(player, manager), targetChar(target->GetClientNum(),target->GetCharacterData()->Inventory() )
{
    targetClient = target;
    this->target = target->GetClientNum();

    starterAccepted = targetAccepted = false;

    SendRequestToBoth();

    targetClient->GetActor()->RegisterCallback( this );

    target->GetCharacterData()->Inventory().BeginExchange();
}


PlayerToPlayerExchange::~PlayerToPlayerExchange()
{
    // printf("In ~PlayerToPlayerExchange()\n");
    targetClient->SetExchangeID(0);

    // In case this is set to true JUST before he disappears...
    // Hopefully this could fix the exploit reported by Edicho
    targetAccepted = false;
    starterAccepted = false;

    if  ( !exchangeSuccess )
    {
        targetClient->GetCharacterData()->Inventory().RollbackExchange();
        psserver->GetCharManager()->SendInventory(target);
    }

    SendEnd( target );

    starterClient->GetActor()->UnregisterCallback( this );
    targetClient->GetActor()->UnregisterCallback( this );
}

void PlayerToPlayerExchange::DeleteObjectCallback(iDeleteNotificationObject * object)
{
    // printf("PlayerToPlayerExchange::DeleteObjectCallback(). Actor in exchange is being deleted, so exchange is ending.\n");

    // Make sure both actors are disconnected.
    starterClient->GetActor()->UnregisterCallback( this );
    targetClient->GetActor()->UnregisterCallback( this );

    HandleEnd( starterClient );
    exchangeMgr->DeleteExchange( this );
}



void PlayerToPlayerExchange::SendRequestToBoth()
{
    csString targetName( targetClient->GetName() );
    psExchangeRequestMsg one( starterClient->GetClientNum(), targetName, true );
    psserver->GetEventManager()->SendMessage( one.msg );

    csString clientName( starterClient->GetName() );
    psExchangeRequestMsg two( targetClient->GetClientNum(), clientName, true );
    psserver->GetEventManager()->SendMessage( two.msg );
}

Client* PlayerToPlayerExchange::GetOtherClient(Client* client) const
{
    return (client == starterClient) ? targetClient : starterClient;
}

bool PlayerToPlayerExchange::AddItem(Client* fromClient, INVENTORY_SLOT_NUMBER fromSlot, int stackCount, int toSlot)
{
    if (!Exchange::AddItem(fromClient, fromSlot, stackCount, toSlot))
        return false;

    starterAccepted = false;
    targetAccepted = false;
    SendExchangeStatusToBoth();
    return true;
}

void PlayerToPlayerExchange::SendAddItemMessage(Client* fromClient, int slot, psCharacterInventory::psCharacterInventoryItem* invItem)
{
    Exchange::SendAddItemMessage(fromClient, slot, invItem);
    psItem* item = invItem->GetItem();

    Client *toClient = GetOtherClient(fromClient);

    psExchangeAddItemMsg msg(toClient->GetClientNum(), item->GetName(),
                             CONTAINER_EXCHANGE_RECEIVING, slot,
                             invItem->exchangeStackCount, item->GetImageName());
    psserver->GetEventManager()->SendMessage(msg.msg);
}

void PlayerToPlayerExchange::SendRemoveItemMessage(Client* fromClient, int slot)
{
    Exchange::SendRemoveItemMessage(fromClient, slot);

    Client *toClient = GetOtherClient(fromClient);

    psExchangeRemoveItemMsg msg(toClient->GetClientNum(), CONTAINER_EXCHANGE_RECEIVING, slot, 0);
    psserver->GetEventManager()->SendMessage(msg.msg);
}


psMoney PlayerToPlayerExchange::GetOfferedMoney(Client *client)
{
    CS_ASSERT(client == starterClient || client == targetClient);
    if (client == starterClient)
        return starterChar.GetOfferedMoney();
    else
        return targetChar.GetOfferedMoney();
}

bool PlayerToPlayerExchange::AdjustMoney( Client* client, int moneyType, int newMoney )
{
    if ( Exchange::AdjustMoney( client, moneyType, newMoney ) )
    {
        targetChar.UpdateReceivingMoney( starterChar.offeringMoney );
    }
    else if (client == targetClient )
    {
        targetChar.AdjustMoney( moneyType, newMoney);
        targetChar.UpdateOfferingMoney();
        starterChar.UpdateReceivingMoney( targetChar.offeringMoney );
    }

    //Remove accepted state
    starterAccepted = false;
    targetAccepted = false;
    SendExchangeStatusToBoth();

    return true;
}

bool PlayerToPlayerExchange::AdjustMoney( Client* client, const psMoney& money )
{
    if ( Exchange::AdjustMoney( client, money ) )
    {
        targetChar.UpdateReceivingMoney( starterChar.offeringMoney );
    }
    else if (client == targetClient )
    {
        targetChar.AdjustMoney( money);
        targetChar.UpdateOfferingMoney();
        starterChar.UpdateReceivingMoney( targetChar.offeringMoney );
    }

    //Remove accepted state
    starterAccepted = false;
    targetAccepted = false;
    SendExchangeStatusToBoth();

    return true;
}


bool PlayerToPlayerExchange::RemoveItem(Client* fromClient, int fromSlot, int count)
{
    if (!Exchange::RemoveItem(fromClient, fromSlot, count))
        return false;

    starterAccepted = false;
    targetAccepted = false;
    SendExchangeStatusToBoth();
    return true;
}


bool PlayerToPlayerExchange::CheckExchange(uint32_t clientNum, bool checkRange)
{
    ClientConnectionSet * clients = psserver->GetConnections();

    Client * playerClient = clients->Find(player);
    if (playerClient == NULL)
    {
        exchangeEnded = true;
        return false;
    }

    Client * targetClient = clients->Find(target);
    if (targetClient == NULL)
    {
        exchangeEnded = true;
        return false;
    }

    if (checkRange)
    {
        if (clientNum == player)
        {
            if ( ! CheckRange(player, playerClient->GetActor(), targetClient->GetActor()) )
            {
                psserver->GetCharManager()->SendInventory(clientNum);
                return false;
            }
        }
        else
        {
            if ( ! CheckRange(target, targetClient->GetActor(), playerClient->GetActor()) )
            {
                psserver->GetCharManager()->SendInventory(clientNum);
                return false;
            }
        }
    }

    return true;
}


bool PlayerToPlayerExchange::HandleAccept(Client * client)
{
    if ( ! CheckExchange(client->GetClientNum(), true))
        return exchangeEnded;
        
    if (client->GetClientNum() == player)
        starterAccepted = true;
    else
        targetAccepted = true;

    if (starterAccepted && targetAccepted)
    {
        Debug1(LOG_EXCHANGES,client->GetClientNum(),"Both Exchanged\n");
        Debug2(LOG_EXCHANGES,client->GetClientNum(),"Exchange %d have been accepted by both clients\n",id);

        ClientConnectionSet * clients = psserver->GetConnections();

        const char* playerName;
        const char* targetName;
        if (player !=0)
            playerName = clients->Find(player)->GetName();
        else
            playerName = "None";
        if (target !=0)
            targetName = clients->Find(target)->GetName();
        else
            targetName = "None";

        // We must set the exchange ID to 0 so that the items will now save
        targetClient->SetExchangeID(0);
        starterClient->SetExchangeID(0);

        // Exchange logging
        csString items;
        csString buf;
        starterChar.GetOfferingCSV(items);
        if (!items.IsEmpty())
        {
            items.Truncate(items.Length() - 2);
        }
        
        buf.Format("%s, %s, %s, \"%s\", %d, \"%s\"", playerName, targetName, "P2P Exchange", items.GetDataSafe(), 0, starterChar.GetOfferedMoney().ToString().GetData());
        psserver->GetLogCSV()->Write(CSV_EXCHANGES, buf);
        
        targetChar.GetOfferingCSV(items);
        if (!items.IsEmpty())
        {
            items.Truncate(items.Length() - 2);
        }

        buf.Format("%s, %s, %s, \"%s\", %d, \"%s\"", targetName, playerName, "P2P Exchange", items.GetDataSafe(), 0, targetChar.GetOfferedMoney().ToString().GetData());
        psserver->GetLogCSV()->Write(CSV_EXCHANGES, buf);

        csString itemsOffered;

        // RMS: Output items start client gave target client
        if (starterChar.GetExchangedItems(itemsOffered))
        {
            psserver->SendSystemBaseInfo( targetClient->GetClientNum(), "%s gave %s %s.", starterClient->GetName(), targetClient->GetName(), itemsOffered.GetData());                             // RMS: Trade cancelled without items being offered
            psserver->SendSystemBaseInfo( starterClient->GetClientNum(), "%s gave %s %s.", starterClient->GetName(), targetClient->GetName(), itemsOffered.GetData());                            // RMS: Trade cancelled without items being offered
        }

        // RMS: Output items target client gave start client
        if (targetChar.GetExchangedItems(itemsOffered))
        {
            psserver->SendSystemBaseInfo( targetClient->GetClientNum(), "%s gave %s %s.", targetClient->GetName(), starterClient->GetName(), itemsOffered.GetData());                             // RMS: Trade cancelled without items being offered
            psserver->SendSystemBaseInfo( starterClient->GetClientNum(), "%s gave %s %s.", targetClient->GetName(), starterClient->GetName(), itemsOffered.GetData());                            // RMS: Trade cancelled without items being offered
        }

        starterChar.TransferOffer(target);
        starterChar.TransferMoney(target);

        targetChar.TransferOffer(player);
        targetChar.TransferMoney(player);

        psserver->SendSystemOK(targetClient->GetClientNum(),"Trade complete");
        psserver->SendSystemOK(starterClient->GetClientNum(),"Trade complete");

        psserver->GetCharManager()->UpdateItemViews(player);
        psserver->GetCharManager()->UpdateItemViews(target);

        SendEnd(player);
        SendEnd(target);

        exchangeSuccess = true;
        return true;
    }
    else
        SendExchangeStatusToBoth();
    return false;
}



void PlayerToPlayerExchange::HandleEnd(Client * client)
{
    printf("In P2PExch::HandleEnd\n");

    csString itemsOffered;

    // RMS: Target client
    if (starterChar.GetExchangedItems(itemsOffered))
        psserver->SendSystemResult( targetClient->GetClientNum(), "Trade declined" );  // RMS: Trade cancelled without items being offered
    else
        psserver->SendSystemResult( targetClient->GetClientNum(), "Trade cancelled");  // RMS: Starter client

    if (targetChar.GetExchangedItems(itemsOffered))
        psserver->SendSystemResult( starterClient->GetClientNum(), "Trade declined" ); // RMS: Trade cancelled without items being offered
    else
        psserver->SendSystemResult( starterClient->GetClientNum(), "Trade cancelled");

    targetClient->GetCharacterData()->Inventory().RollbackExchange();
    starterClient->GetCharacterData()->Inventory().RollbackExchange();

    if (client->GetClientNum() == player)
        SendEnd(target);
    else
        SendEnd(player);
}

void PlayerToPlayerExchange::SendExchangeStatusToBoth()
{
    psExchangeStatusMsg starterMsg(starterClient->GetClientNum(), starterAccepted, targetAccepted);
    psserver->GetEventManager()->SendMessage(starterMsg.msg);

    psExchangeStatusMsg targetMsg(targetClient->GetClientNum(), targetAccepted, starterAccepted);
    psserver->GetEventManager()->SendMessage(targetMsg.msg);
}

psItem* PlayerToPlayerExchange::GetTargetOffer(int slot)
{
    if (slot < 0 || slot >= EXCHANGE_SLOT_COUNT)
        return NULL;

    return targetChar.GetOfferedItem(slot);
}

/***********************************************************************************
*
*                              class NPCExchange
*
***********************************************************************************/

PlayerToNPCExchange::PlayerToNPCExchange(Client* player, gemObject* target, csRef<ExchangeManager> manager)
                    : Exchange(player, manager)
{
    this->target = target;

    csString targetName( target->GetName() );
    psExchangeRequestMsg one( starterClient->GetClientNum(), targetName, false );
    psserver->GetEventManager()->SendMessage( one.msg );

    target->RegisterCallback( this );

//    player->GetCharacterData()->Inventory().BeginExchange();
}

PlayerToNPCExchange::~PlayerToNPCExchange()
{
    starterClient->GetCharacterData()->Inventory().RollbackExchange();

    starterClient->GetActor()->UnregisterCallback( this );
    target->UnregisterCallback( this );
}

gemObject * PlayerToNPCExchange::GetTargetGEM()
{
    return target;
}

bool PlayerToNPCExchange::CheckExchange(uint32_t clientNum, bool checkRange)
{
    ClientConnectionSet * clients = psserver->GetConnections();

    Client * playerClient = clients->Find(player);
    if (playerClient == NULL)
    {
        exchangeEnded = true;
        return false;
    }

    gemObject * targetGEM = GetTargetGEM();
    if (targetGEM == NULL)
    {
        exchangeEnded = true;
        return false;
    }

    if (checkRange)
    {
        if ( ! CheckRange(clientNum, playerClient->GetActor(), targetGEM) )
        {
            psserver->GetCharManager()->SendInventory(clientNum);
            return false;
        }
    }
    return true;
}



void PlayerToNPCExchange::HandleEnd(Client * client)
{
    client->GetCharacterData()->Inventory().RollbackExchange();
    return;
}

bool PlayerToNPCExchange::CheckXMLResponse(Client * client, psNPCDialog *dlg, csString trigger)
{
    NpcResponse *resp = dlg->FindXMLResponse(client, trigger);
    if (resp && resp->type != NpcResponse::ERROR_RESPONSE)
    {
        if (resp->ExecuteScript(client, (gemNPC *)target))
        {
            // Give offered items to npc
            starterChar.TransferOffer(target->GetCharacterData());
            starterChar.TransferMoney(target->GetCharacterData());
            exchangeSuccess = true;
            client->SetLastResponse(resp->id);
            return true;
        }
    }
    return false;
}
    

bool PlayerToNPCExchange::HandleAccept(Client * client)
{
    if ( ! CheckExchange(client->GetClientNum(), true))
        return exchangeEnded;


    // We must set the exchange ID to 0 so that the items will now save
    starterClient->SetExchangeID(0);

    // Send to the npc as an event trigger
    psNPCDialog *dlg = target->GetNPCDialogPtr();
    if (dlg)
    {
        csString trigger;

        // Check if NPC require the exact among of money
        starterChar.GetSimpleOffering(trigger,client->GetCharacterData(),true);
        if (!CheckXMLResponse(client,dlg,trigger))
        {
            // Check if NPC is OK with all the money total in trias
            starterChar.GetSimpleOffering(trigger,client->GetCharacterData(),false);
            if (!CheckXMLResponse(client,dlg,trigger))
            {
                psserver->SendSystemError(client->GetClientNum(), "%s does not need that.", target->GetName());
                psserver->SendSystemOK(client->GetClientNum(),"Trade Declined");
				client->GetCharacterData()->Inventory().RollbackExchange();
                psserver->GetCharManager()->SendInventory(player);
                SendEnd(player);
                return false;
            }
        }
    }
    else
    {
        psserver->SendSystemError(client->GetClientNum(), "%s does not respond to gifts.", target->GetName());
        client->GetCharacterData()->Inventory().RollbackExchange();
        psserver->GetCharManager()->SendInventory(player);
        SendEnd(player);
        return false;
    }

	// This is redundant but harmless.  CheckXMLResponse also does this but it's easy to miss so leaving here incase someone removes it.
    client->GetCharacterData()->Inventory().PurgeOffered();

    psserver->SendSystemOK(client->GetClientNum(),"Trade complete");

    psserver->GetCharManager()->SendInventory(player);
    SendEnd(player);

	// Not committing will cause rollback to do bad things instead of being harmless.
	starterClient->GetCharacterData()->Inventory().CommitExchange();

    return true;
}

void PlayerToNPCExchange::DeleteObjectCallback(iDeleteNotificationObject * object)
{
    starterClient->GetActor()->UnregisterCallback( this );
    target->UnregisterCallback( this );

    HandleEnd( starterClient );
    exchangeMgr->DeleteExchange( this );
}

/***********************************************************************************
*
*                              class ExchangeManager
*
***********************************************************************************/

ExchangeManager::ExchangeManager(ClientConnectionSet *pClnts)
{
    clients      = pClnts;

    psserver->GetEventManager()->Subscribe(this, MSGTYPE_GUIEXCHANGE, REQUIRE_READY_CLIENT|REQUIRE_ALIVE);
    psserver->GetEventManager()->Subscribe(this, MSGTYPE_EXCHANGE_REQUEST, REQUIRE_READY_CLIENT|REQUIRE_ALIVE|REQUIRE_TARGETACTOR);
    psserver->GetEventManager()->Subscribe(this, MSGTYPE_EXCHANGE_ACCEPT, REQUIRE_READY_CLIENT|REQUIRE_ALIVE);
    psserver->GetEventManager()->Subscribe(this, MSGTYPE_EXCHANGE_END, REQUIRE_READY_CLIENT|REQUIRE_ALIVE);
}

ExchangeManager::~ExchangeManager()
{
    if (psserver->GetEventManager())
    {
        psserver->GetEventManager()->Unsubscribe(this, MSGTYPE_GUIEXCHANGE);
        psserver->GetEventManager()->Unsubscribe(this, MSGTYPE_EXCHANGE_REQUEST);
        psserver->GetEventManager()->Unsubscribe(this, MSGTYPE_EXCHANGE_ACCEPT);
        psserver->GetEventManager()->Unsubscribe(this, MSGTYPE_EXCHANGE_END);
    }
}
void ExchangeManager::StartExchange( Client* client, bool withPlayer )
{
    // Make sure we are in a peaceful mode before trading
    if ( client->GetActor()->GetMode() != PSCHARACTER_MODE_PEACE && client->GetActor()->GetMode() != PSCHARACTER_MODE_SIT && client->GetActor()->GetMode() != PSCHARACTER_MODE_OVERWEIGHT )
    {
        csString err;
        err.Format("You can't trade while %s.",client->GetCharacterData()->GetModeStr());
        psserver->SendSystemInfo(client->GetClientNum(), err);
        return;
    }

    if ( client->GetExchangeID() )
    {
        psserver->SendSystemError(client->GetClientNum(), "You are already busy with another trade" );
        return;
    }

    Client * targetClient = 0;
    gemObject * target;

    target = client->GetTargetObject();

    if ( target->IsAlive() == false )
    {
        psserver->SendSystemError(client->GetClientNum(), "Cannot give items to dead things!");
        return;
    }

    // if the command was "/give":
    if (!withPlayer)
    {
        if ( !target->GetNPCPtr() )
        {
            psserver->SendSystemError(client->GetClientNum(), "You can give items to NPC only");
            return;
        }

        Exchange* exchange = new PlayerToNPCExchange(client, target, this);
        client->SetExchangeID( exchange->GetID() );
        exchanges.Push (exchange);
    }

    // if the command was "/trade":
    else
    {
        if ( target->GetNPCPtr() )
        {
            psserver->SendSystemError(client->GetClientNum(), "You can trade with other players only");
            return;
        }

        targetClient = clients->FindPlayer(target->GetPlayerID());
        if (targetClient == NULL)
        {
            psserver->SendSystemError(client->GetClientNum(), "Cannot find your target!");
            return;
        }

        if ( client == targetClient )
        {
            psserver->SendSystemError(client->GetClientNum(),"You can not trade with yourself");
            return;
        }

        if ( targetClient->GetExchangeID() )
        {
            psserver->SendSystemError(client->GetClientNum(), "%s is busy with another trade", targetClient->GetName() );
            return;
        }

        // Check range
        if(client->GetActor()->RangeTo(targetClient->GetActor()) > RANGE_TO_SELECT)
        {
            psserver->SendSystemError(client->GetClientNum(), "%s is too far away to trade.",
                                      targetClient->GetActor()->GetName());
            return;
        }

        // Must ask the other player before starting the trade
        csString question;
        question.Format( "Do you want to trade with %s ?", client->GetCharacterData()->GetCharName() );
        PendingQuestion *invite = new PendingTradeInvite(
                    client, targetClient, question.GetData() );
        psserver->questionmanager->SendQuestion(invite);
    }
}

void ExchangeManager::HandleMessage(MsgEntry *me,Client *client)
{
    switch ( me->GetType() )
    {
        case MSGTYPE_EXCHANGE_END:
        {
            Notify2( LOG_TRADE, "Trade %d Rejected", client->GetExchangeID() );

            Exchange* exchange = GetExchange( client->GetExchangeID() );
            if ( exchange )
            {
                exchange->HandleEnd(client);
                DeleteExchange(exchange);
            }
            else
            {
                Warning2( LOG_TRADE, "Trade %d Not located", client->GetExchangeID() );
            }

            break;
        }

        case MSGTYPE_EXCHANGE_REQUEST:
        {
            Notify2( LOG_TRADE, "Trade Requested from %u", client->GetClientNum() );
            psExchangeRequestMsg msg(me);
            StartExchange( client, msg.withPlayer );
            break;
        }

        case MSGTYPE_EXCHANGE_ACCEPT:
        {
            Notify3( LOG_TRADE, "Exchange %d Accept from %d", client->GetExchangeID(), client->GetClientNum() );

            Exchange* exchange = GetExchange( client->GetExchangeID() );
            if ( exchange )
            {
                if (exchange->HandleAccept(client))
                {
                    DeleteExchange(exchange);
                }
            }
            break;
        }
    }
}



Exchange * ExchangeManager::GetExchange(int id)
{
    // TODO:  Make this a hashmap
    for (size_t n = 0; n < exchanges.GetSize(); n++){
        if (exchanges[n]->GetID() == id) return exchanges[n];
    }
    Error2("Can't find exchange: %d !!!",id);
    return NULL;
}

void ExchangeManager::DeleteExchange(Exchange * exchange)
{
    exchanges.Delete(exchange); // deletes the exchange object
}
