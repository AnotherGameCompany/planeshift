/*
 * psproxlist.cpp
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

#include <psconfig.h>

#include "psstdint.h"

//CS includes
#include <iutil/objreg.h>
#include <iutil/event.h>
#include <iutil/eventq.h>
#include <iutil/evdefs.h>
#include <iutil/virtclk.h>
#include "util/serverconsole.h"

#include <imesh/sprite3d.h>

#include <csutil/databuf.h>
#include <csutil/plugmgr.h>
#include <iengine/movable.h>
#include <iengine/engine.h>
#include <iengine/region.h>
#include <cstool/collider.h>
#include <ivaria/collider.h>

#include <physicallayer/entity.h>
#include <propclass/mesh.h>

// PS Includes
#include <propclass/linmove.h>
#include "util/log.h"
#include "gem.h"
#include "psproxlist.h"
#include "playergroup.h"
#include "globals.h"
#include "npcmanager.h"
#include "net/npcmessages.h"
#include "entitymanager.h"
#include "engine/psworld.h"

// define if you want loads of debug printfs
//#define PSPROXDEBUG


//----------------------------------------------------------------------------

ProximityList::ProximityList( iObjectRegistry* object_reg, gemObject *parent )
{
    pslog::Initialize (object_reg);
    
    #ifdef PSPROXDEBUG
        CPrintf(CON_DEBUG, "Construction of cel Proximity List (%p)!\n", this);
    #endif

    self = parent;

    clientnum = 0;
    float rot;
    iSector *sector;
    firstFrame = true;
    self->GetPosition (oldPos, rot, sector);
}

ProximityList::~ProximityList()
{
    #ifdef PSPROXDEBUG
        CPrintf(CON_DEBUG, "Destruction of cel Proximity List (%p)!\n", this );
    #endif

    while (objectsThatIWatch.GetSize())
    {
#ifdef PSPROXDEBUG
        CPrintf(CON_DEBUG, "Unsubscribing from %s (%p).\n",obj->GetName(), this );
#endif

        objectsThatIWatch[0]->GetProxList()->RemoveWatcher(self);
        objectsThatIWatch.DeleteIndex(0);
        objectsThatIWatch_touched.DeleteIndex(0);
    }

    while ( objectsThatWatchMe.GetSize() )
    {
        gemObject *obj = (gemObject *)objectsThatWatchMe.Get(0).object;
        #ifdef PSPROXDEBUG
            CPrintf(CON_DEBUG, "Unsubscribing from %s (%p).\n",obj->GetName(), this );
        #endif
        obj->GetProxList()->EndWatching(self);
    }

    self = NULL;
}


//----------------------------------------------------------------------------

bool ProximityList::Initialize(int cnum,gemObject *parent)
{
    clientnum = cnum;
    return true;
}

#define PSABS(x)    ((x) < 0 ? -(x) : (x))

bool ProximityList::IsNear(iSector *sector,csVector3& pos,gemObject *object2,float radius)
{
    // Find the current position of the specified entity
    csVector3 pos2;
    float yrot;
    iSector *sector2;

    object2->GetPosition(pos2,yrot,sector2);

    if (sector != sector2)  // ptr comparison here is dangerous but valid.  Might convert to strcmp later.
        return false;       // if not same sector, then by definition not Near.

    /**
     * Note below defines a cube, not a sphere, but for updating
     * purposes should be fine and is faster.
     */
    if ( (PSABS(pos.x - pos2.x) < radius) &&
         (PSABS(pos.y - pos2.y) < radius) &&
         (PSABS(pos.z - pos2.z) < radius) )
        return true;
    else
        return false;
}

bool ProximityList::StartMutualWatching(int othercnum, gemObject *otherobject, float range)
{
    bool ret1,ret2;

    ret1 = StartWatching(otherobject, range);
    ret2 = otherobject->GetProxList()->StartWatching(self, range);
    return (ret1 || ret2);
}

void ProximityList::AddWatcher(gemObject *interestedObject, float range)
{
    PublishDestination *pd;

    pd = FindObjectThatWatchesMe(interestedObject);
    if (pd != NULL)
    {
        UpdatePublishDestRange(pd, self, interestedObject, range);
        return;
    }


    size_t i = objectsThatWatchMe.Push(PublishDestination(interestedObject->GetClientID(), interestedObject, 0, 100));
    UpdatePublishDestRange(&objectsThatWatchMe[i], self, interestedObject, range);
    objectsThatWatchMe_touched.Push(true);
}

bool ProximityList::EndMutualWatching(gemObject *fromobject)
{
    EndWatching(fromobject);
    fromobject->GetProxList()->EndWatching(self);
    return true;
}

/*void ProximityList::UnsubscribeAllWatchers()
{
    int x = 0;
    // Remove the target's entity/client from our list
    x = 0;
    while (x < objectsThatWatchMe.GetSize())
    {
        gemObject* object = (gemObject*)objectsThatWatchMe.Get(x)->object;
        if (object != self)
            // In this case objectsThatWatchMe[x] will be deleted and the next watcher will be placed to index 'x',
            // so we don't increment x
            object->GetProxList()->EndWatching(self);
        else
            x++;
    }
}*/

bool ProximityList::StartWatching(gemObject * object, float range)
{
    if (self->GetClientID() == 0)
        return false;

    if (FindObjectThatIWatch(object))
    {
        object->GetProxList()->TouchObjectThatWatchesMe(self,range);
        return false;
    }

    objectsThatIWatch.Push(object);
    objectsThatIWatch_touched.Push(true);
    object->GetProxList()->AddWatcher(self, range);
    return true;
}

void ProximityList::EndWatching(gemObject * object)
{
    for (size_t x = 0; x < objectsThatIWatch.GetSize(); x++ )
    {
        if (objectsThatIWatch[x] == object)
        {
            objectsThatIWatch[x]->GetProxList()->RemoveWatcher(self);
            objectsThatIWatch.DeleteIndex(x);
            objectsThatIWatch_touched.DeleteIndex(x);
            return;
        }
    }
}

void ProximityList::RemoveWatcher(gemObject *object)
{
    // Remove the target's entity/client from our list
    for (size_t x = 0; x < objectsThatWatchMe.GetSize(); x++ )
    {
        if ( objectsThatWatchMe[x].object == object)
        {
            objectsThatWatchMe.DeleteIndex(x);
            objectsThatWatchMe_touched.DeleteIndex(x);
            return;
        }
    }
}

bool ProximityList::FindClient(int cnum)
{
    for (size_t x = 0; x < objectsThatWatchMe.GetSize(); x++ )
    {
        if ( objectsThatWatchMe[x].client == cnum)
        {
            return true;
        }
    }
    return false;
}

bool ProximityList::FindObject(gemObject *object)
{
    for (size_t x = 0; x < objectsThatWatchMe.GetSize(); x++ )
    {
        if ( objectsThatWatchMe[x].object == object)
        {
            return true;
        }
    }
    return false;
}

PublishDestination *ProximityList::FindObjectThatWatchesMe(gemObject *object)
{
    for (size_t x = 0; x < objectsThatWatchMe.GetSize(); x++ )
    {
        if ( objectsThatWatchMe[x].object == object)
        {
            objectsThatWatchMe_touched[x] = true;
            return &objectsThatWatchMe[x];
        }
    }
    return NULL;
}

bool ProximityList::FindObjectThatIWatch(gemObject *object)
{
    for (size_t x = 0; x < objectsThatIWatch.GetSize(); x++ )
    {
        if ( objectsThatIWatch[x] == object)
        {
            objectsThatIWatch_touched[x] = true;
            return true;
        }
    }
    return false;
}

gemObject *ProximityList::FindObjectName(const char *name)
{
    for (size_t x = 0; x < objectsThatIWatch.GetSize(); x++ )
    {
        if ( !strcasecmp(objectsThatIWatch[x]->GetName(),name) )
        {
            return objectsThatIWatch[x];
        }
    }
    return false;
}

void ProximityList::UpdatePublishDestRange(PublishDestination *pd, gemObject *myself, gemObject *object, float newrange)
{
    psNPCCommandsMessage::PerceptionType pcpt=psNPCCommandsMessage::CMD_TERMINATOR;

    pd->dist = newrange;
    if (newrange < pd->min_dist)
    {
        if (newrange < LONG_RANGE_PERCEPTION &&
            pd->min_dist > LONG_RANGE_PERCEPTION) // 30m threshold crossed
        {
            pcpt = psNPCCommandsMessage::PCPT_LONGRANGEPLAYER;
        }

        if (newrange < SHORT_RANGE_PERCEPTION &&
            pd->min_dist > SHORT_RANGE_PERCEPTION)  // 10m threshold crossed
        {
            pcpt = psNPCCommandsMessage::PCPT_SHORTRANGEPLAYER;
        }

        if (newrange < PERSONAL_RANGE_PERCEPTION &&
            pd->min_dist > PERSONAL_RANGE_PERCEPTION)  // 4m threshold crossed
        {
            pcpt = psNPCCommandsMessage::PCPT_VERYSHORTRANGEPLAYER;
        }

        if (pcpt)
        {
            gemActor *actorself   = dynamic_cast<gemActor *>(myself);
            gemActor *actorobject = dynamic_cast<gemActor *>(object);
            if (actorself && actorobject)
            {
                if (!myself->GetClientID() && object->GetClientID() && actorself->IsAlive()) // I'm an NPC and He is a player watching me
                {
                    float faction = actorself->GetRelativeFaction(actorobject);
                    psserver->GetNPCManager()->QueueEnemyPerception(pcpt,
                                                                    actorself,
                                                                    actorobject,
                                                                    faction);
                }
                else if (myself->GetClientID() && !object->GetClientID() && actorobject->IsAlive()) // I'm a player and he is an npc
                {
                    float faction = actorobject->GetRelativeFaction(actorself);
                    psserver->GetNPCManager()->QueueEnemyPerception(pcpt,
                                                                    actorobject,
                                                                    actorself,
                                                                    faction);
                }
            }
        }
        pd->min_dist = newrange;
    }
}

void ProximityList::TouchObjectThatWatchesMe(gemObject *object,float newrange)
{
    for (size_t x = 0; x < objectsThatWatchMe.GetSize(); x++ )
    {
        if ( objectsThatWatchMe[x].object == object)
        {
            objectsThatWatchMe_touched[x] = true;
            UpdatePublishDestRange(&objectsThatWatchMe[x], self, object, newrange);
            return;
        }
    }
}

bool ProximityList::CheckUpdateFrequency()
{
    csVector3 pos;
    float rot;
    iSector *sector;

    self->GetPosition (pos, rot, sector);

    if (self->IsUpdateReq (pos, oldPos) || firstFrame)
    {
        firstFrame = false;
        oldPos = pos;
        return true;
    }
    return false;
}


void ProximityList::ClearTouched()
{
    size_t objNum;

    for (objNum = 0; objNum < objectsThatWatchMe_touched.GetSize(); objNum++ )
        objectsThatWatchMe_touched[objNum] = false;
    for (objNum = 0; objNum < objectsThatIWatch_touched.GetSize(); objNum++ )
        objectsThatIWatch_touched[objNum]  = false;
}

bool ProximityList::GetUntouched_ObjectThatWatchesMe(gemObject* &object)
{
    for (size_t x = 0; x < objectsThatWatchMe_touched.GetSize(); x++ )
    {
        if (!objectsThatWatchMe_touched[x])
        {
            object = (gemObject*)objectsThatWatchMe[x].object;
            objectsThatWatchMe_touched[x] = true;
            return true;
        }
    }
    return false;
}

bool ProximityList::GetUntouched_ObjectThatIWatch(gemObject* &object)
{
    for (size_t x = 0; x < objectsThatIWatch_touched.GetSize(); x++ )
    {
        if (!objectsThatIWatch_touched[x])
        {
            object = objectsThatIWatch[x];
            objectsThatIWatch_touched[x] = true;
            return true;
        }
    }
    return false;
}


float ProximityList::RangeTo( gemObject* object, bool ignoreY )
{
#ifdef PSPROXDEBUG
    CPrintf(CON_DEBUG, "[float ProximityList::RangeTo( gemObject* entity )]\n");
#endif
    // Find the current position of the specified entity
    csVector3 pos1;
    csVector3 pos2;
    iSector *sector1,*sector2;
    int offset = 0;

    object->GetPosition(pos1,sector1);

#ifdef PSPROXDEBUG
    CPrintf(CON_DEBUG, "Entity %s is at (%f,%f,%f)\n", entity->GetName(), pos1.x, pos1.y, pos1.z);
#endif

    self->GetPosition(pos2, sector2);

#ifdef PSPROXDEBUG
    CPrintf(CON_DEBUG, "Entity %s is at (%f,%f,%f)\n", GetEntity()->GetName(), pos2.x, pos2.y, pos2.z);
#endif

    EntityManager::GetSingleton().GetWorld()->WarpSpace(sector2, sector1, pos2);
    csRef<iRegion> region = scfQueryInterface<iRegion> (sector1->QueryObject ()->GetObjectParent ());
    csRef<iRegion> region2 = scfQueryInterface<iRegion> (sector2->QueryObject ()->GetObjectParent ());

    // Penalty for being in different regions
    if(region != region2)
    {
        offset = 50;
    }

    if ( ignoreY )
    {
        return ( sqrt(  (pos1.x - pos2.x)*(pos1.x - pos2.x)+
                    (pos1.z - pos2.z)*(pos1.z - pos2.z)) + offset);
    }
    else
    {
        return ( sqrt(  (pos1.x - pos2.x)*(pos1.x - pos2.x)+
                    (pos1.y - pos2.y)*(pos1.y - pos2.y)+
                    (pos1.z - pos2.z)*(pos1.z - pos2.z)) + offset);
    }
}


void ProximityList::DebugDumpContents(csString& out)
{
    size_t x;
    psString temp;

    temp.AppendFmt("I represent client %d\n",clientnum);
    temp.AppendFmt("I am publishing updates to:\n");
    
    for (x = 0; x < objectsThatWatchMe.GetSize(); x++ )
    {
        temp.AppendFmt( "\tClient %d (%s), range %1.2f\n",
                objectsThatWatchMe[x].client,
                ((gemObject*)(objectsThatWatchMe[x].object))->GetName(),
                objectsThatWatchMe[x].dist);
    }

    if (clientnum)
    {
        temp.Append( "I am listening to:\n");
        for (x = 0; x < objectsThatIWatch.GetSize(); x++ )
        {
            gemObject * obj = objectsThatIWatch[x];
            
            temp.AppendFmt("\t%-3d %s\n",obj->GetEntity()->GetID(),obj->GetName() );
        }
    }
    else
    {
        temp.Append("I am not a human player, so I don't subscribe to other entities.\n");
    }
    out.Append(temp);
}


