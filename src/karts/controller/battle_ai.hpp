
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004-2005 Steve Baker <sjbaker1@airmail.net>
//  Copyright (C) 2006-2007 Eduardo Hernandez Munoz
//  Copyright (C) 2010      Joerg Henrichs
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef HEADER_BATTLE_AI_HPP
#define HEADER_BATTLE_AI__HPP

#include "karts/controller/ai_base_controller.hpp"
#include "race/race_manager.hpp"
#include "tracks/battle_graph.hpp"
#include "utils/random_generator.hpp"

class AIProperties;
class ThreeStrikesBattle;
class BattleGraph;
class Track;
class Vec3;
class Item;

namespace irr
{
    namespace scene { class ISceneNode; }
    namespace video { class ITexture; }
}

class BattleAI : public AIBaseController
{

private: 

    /** Holds the current position of the AI on the battle graph. Sets to  
     *  BattleGraph::UNKNOWN_POLY if the location is unknown. This variable is
     *  updated in ThreeStrikesBattle::updateKartNodes() */
    int m_current_node;
    
    /** Holds the next node the kart is expected to drive to. Currently unused. */
    int m_next_node;

    /** The node(poly) at which the target point lies in. */
    int m_target_node;

    /** The target point. */
    Vec3 m_target_point;

    /** The point kart is aimint at right now */
    Vec3 m_aim_point;

    /** The steering angle required to reach the target point. */
    float m_target_angle;

    /** The kart at which this AI is targetting */
    AbstractKart* m_target_kart;

    /** Holds the set of portals that the kart will cross when moving through 
     *  polygon channel. See findPortals() */
    std::vector<std::pair<Vec3,Vec3> > m_portals;

    /** Holds the corner points computed using the funnel algorithm that the AI
     * will eventaully move through. See stringPull() */
    std::vector<Vec3> m_path_corners;

    /** This is a timer that counts down when the kart is reversing to get unstuck */
    float m_time_since_stuck;

    /** Indicates that the kart is currently reversing, and m_time_since_stuck is 
     *  counting down. */
    bool m_currently_reversing;

	const Item *m_item_to_collect;

    float determineTurnRadius(std::vector<Vec3>& points);
    void findPortals(int start, int end);
    void stringPull(const Vec3&, const Vec3&);
    void handleAcceleration(const float dt) ;
    void handleSteering(const float dt);
    void handleBraking();
    void handleGetUnstuck(const float dt);   
	void handleItems(const float dt);
	void handleItemCollection(Vec3*, int*);
    AbstractKart* determineTargetKart(AbstractKart* const current_target);

protected:

    /** Keep a pointer to world. */
    ThreeStrikesBattle *m_world;

#ifdef AI_DEBUG
    /** For debugging purpose: a sphere indicating where the AI 
     *  is targeting at. */
    irr::scene::ISceneNode *m_debug_sphere;
#endif

public:
                BattleAI(AbstractKart *kart, 
                    StateManager::ActivePlayer *player=NULL);
                ~BattleAI();
    unsigned int getCurrentNode() const { return m_current_node; }
    void         setCurrentNode(int i)  { m_current_node = i;   } 
    virtual void update      (float delta);
    virtual void reset       ();
    
    virtual void crashed(const AbstractKart *k) {};
    virtual void handleZipper(bool play_sound) {};
    virtual void finishedRace(float time) {};
    virtual void collectedItem(const Item &item, int add_info=-1,
		                       float previous_energy=0) {};
    virtual void setPosition(int p) {};
    virtual bool isNetworkController() const { return false; }
    virtual bool isPlayerController() const { return false; }
    virtual void action(PlayerAction action, int value) {};
    virtual void  skidBonusTriggered() {};
    virtual bool  disableSlipstreamBonus() const {return 0;}
    virtual void  newLap(int lap) {};
};

#endif