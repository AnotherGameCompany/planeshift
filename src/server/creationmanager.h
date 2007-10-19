/*
 * creationmanager.h - author: Andrew Craig
 *
 * Copyright (C) 2003 Atomic Blue (info@planeshift.it, http://www.atomicblue.org) 
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
#ifndef PS_CHAR_CREATION_MANAGER_H
#define PS_CHAR_CREATION_MANAGER_H

#include "msgmanager.h"
#include "net/charmessages.h"

enum ReservedNames {NAME_RESERVED_FOR_YOU, NAME_RESERVED, NAME_AVAILABLE};

/* Server manager for character creation.
 * This guy loads all the data that may be needed to be sent to the client
 * for the character creation.  It loads data from these tables:
 *   race_info
 *   character_creation
 *
 */
class psCharCreationManager : MessageManager
{

private:
    // Structure to hold the initial CP race values.
    struct RaceCP
    {
        int id;
        int value;
    };
    
    ////////////////////////////////////////////////////
    // Structure for cached character creation choices
    ////////////////////////////////////////////////////
public:    
    struct CreationChoice
    {
        int id;                     
        csString name;              
        csString description;       
        int choiceArea;
        int cpCost;
        csString eventScript;
    };
    ////////////////////////////////////////////////////
private:    
    //////////////////////////////////////////////////////
    // Structure for cached character creation life event
    //////////////////////////////////////////////////////    
    class LifeEventChoiceServer : public LifeEventChoice
    {
    public:
        csString eventScript;       
    };        
    //////////////////////////////////////////////////////    
    
    
public:
    
    psCharCreationManager();
    virtual ~psCharCreationManager();
    
    /** Caches the data from the database needed for character creation.
     */
    bool Initialize();
          
    /** Handles incomming net messages.
     * Have to add what messages it is subscribed to.
     */    
    virtual void HandleMessage(MsgEntry *pMsg,Client *client);
    
    LifeEventChoiceServer* FindLifeEvent( int id );
    CreationChoice* FindChoice( int id );
    
    bool Validate( psCharUploadMessage& mesg, csString& errorMsg );

    /** Check to see a name is unique in the characters table.
      * @param playerName The name to check to see if it is unique.
      * @return true if the name is unique.
      */
    bool IsUnique( const char* playerName );

    /** Check to see a lastname is unique in the characters table.
      * @param lastname The lastname to check to see if it is unique.
      * @return true if the name is unique.
      */
    bool IsLastNameUnique( const char* lastname );

    // Returns true if the name is ok
    static bool FilterName( const char* name );

protected:
    
    /// Cache in the data about the race CP starting values.
    bool LoadCPValues();
    
    /// Caches in creation choices from the database.
    bool LoadCreationChoices();
    
    bool LoadLifeEvents();
    
    RaceCP* raceCPValues;
    int raceCPValuesLength;    // length of the raceCPValues array
    
    /// A list of all the for the parent screen.
    csPDelArray<CreationChoice> parentData;
    csPDelArray<CreationChoice>  childhoodData;
    csPDelArray<LifeEventChoiceServer> lifeEvents;
    
    /** Takes a string name of a choice and converts it to it's enum value.
     * This is useful to have string names in the database ( ie human readable ) but
     * then use them as ints in the application ( ie easier to use ).
     *
     * @param area The name of the area to convert.
     * @return The int value of that area. Will be one of the defined enums of areas.
     */      
    int ConvertAreaToInt( const char* area );
    
    /** Handle and incomming message from a client about parents.
     * This handles a request from the client to send it all the data about choice 
     * areas in the parents screen. 
     */
    void HandleParents( MsgEntry* me,Client *client );
    
    void HandleChildhood( MsgEntry* me,Client *client );
    
    void HandleLifeEvents( MsgEntry* me,Client *client );    

    void HandleTraits( MsgEntry* me,Client *client );    

    void HandleUploadMessage( MsgEntry * me, Client *client );

    /** Handles the creation of a character from the char creation screen
     */
    bool HandleCharCreateCP(MsgEntry*me, Client* client);
    
    /** Handles a check on a name. 
      * This will check the name against the migration and the current 
      * characters table.  It will send back a message to the client to
      * notify them of the name status. If rejected it will include a 
      * message why.
      */
    void HandleName( MsgEntry* me ,Client *client);
  
    /** Handles the deletion of a character from the char pick screen.
      */
    bool HandleCharDelete( MsgEntry* me, Client* client );
    
    int CalculateCPChoices( csArray<uint32_t>& choices, int fatherMod, int motherMod );
    int CalculateCPLife( csArray<uint32_t>& events );  
           
    /** Assign a script to a character. 
      * Takes whatever script is stored in the migration table and adds it to the 
      * character to be run on character login. 
      * @param chardata The character data class to append the script to. 
      */ 
    void AssignScript( psCharacter* chardata );

    /** Check to see if a name is on the reserve list for database migration.
      *  @param playerName The name to check to see if is on a the reserve list.
      *  @param acctID The acct that is trying to create this character.
      *
      *  @return <UL>
      *          <LI>NAME_RESERVED_FOR_YOU - if you can take this name.
      *          <LI>NAME_RESERVED - you are not allowed this name.
      *          <LI>NAME_AVAILABLE - name is free for use.
      *          </UL>
      */
    int IsReserved( const char* playerName, int acctID );
};



#endif
