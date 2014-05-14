//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2012-2013 SuperTuxKart-Team
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

#include "config/player_profile.hpp"

#include "achievements/achievements_manager.hpp"
#include "challenges/unlock_manager.hpp"
#include "config/player_manager.hpp"
#include "online/online_player_profile.hpp"
#include "io/xml_node.hpp"
#include "io/utf_writer.hpp"
#include "utils/string_utils.hpp"

#include <sstream>
#include <stdlib.h>

//------------------------------------------------------------------------------
/** Constructor to create a new player that didn't exist before.
 *  \param name Name of the player.
 *  \param is_guest True if this is a guest account.
*/
PlayerProfile::PlayerProfile(const core::stringw& name, bool is_guest)
{
#ifdef DEBUG
    m_magic_number = 0xABCD1234;
#endif
    m_local_name          = name;
    m_is_guest_account    = is_guest;
    m_use_frequency       = is_guest ? -1 : 0;
    m_unique_id           = PlayerManager::get()->getUniqueId();
    m_is_default          = false;
    m_is_default          = false;
    m_saved_session       = false;
    m_saved_token         = "";
    m_saved_user_id       = 0;
    m_achievements_status = NULL;
    m_story_mode_status   = NULL;
}   // PlayerProfile

//------------------------------------------------------------------------------
/** Constructor to deserialize player data that was saved to a XML file. The
 *  constructor will only load the main player data (like name, id, saved
 *  online data), but not the achievements and story mode data. Reason is
 *  that the achievement and story mode data depends on other data to be
 *  read first (challenges and achievement files), which in turn can only be
 *  created later in the startup process (they depend on e.g. all tracks to
 *  be known). On the other hand, automatic login needs to happen asap
 *  (i.e. as soon as the network thread is started), which needs the main
 *  player data (i.e. the default player, and saved session data). So the
 *  constructor only reads this data, the rest of the player data is handled
 *  in loadRemainingData later in the initialisation process.
 *  \param node The XML node representing this player.
*/
PlayerProfile::PlayerProfile(const XMLNode* node)
{
    m_saved_session       = false;
    m_saved_token         = "";
    m_saved_user_id       = 0;
    m_story_mode_status   = NULL;
    m_achievements_status = NULL;

    node->get("name",          &m_local_name      );
    node->get("guest",         &m_is_guest_account);
    node->get("use-frequency", &m_use_frequency   );
    node->get("unique-id",     &m_unique_id       );
    node->get("is-default",    &m_is_default      );
    node->get("saved-session", &m_saved_session   );
    node->get("saved-user",    &m_saved_user_id   );
    node->get("saved-token",   &m_saved_token     );

    #ifdef DEBUG
    m_magic_number = 0xABCD1234;
    #endif
}   // PlayerProfile

//------------------------------------------------------------------------------
PlayerProfile::~PlayerProfile()
{
#ifdef DEBUG
    m_magic_number = 0xDEADBEEF;
#endif
}   // ~PlayerProfile


//------------------------------------------------------------------------------
/** This function loads the achievement and story mode data. These can only
 *  be loaded after the UnlockManager is created, which needs the karts
 *  and tracks to be loaded first.
 */
void PlayerProfile::loadRemainingData(const XMLNode *node)
{
    const XMLNode *xml_story_mode = node->getNode("story-mode");
    m_story_mode_status =
                  unlock_manager->createStoryModeStatus(xml_story_mode);
    const XMLNode *xml_achievements = node->getNode("achievements");
    m_achievements_status = AchievementsManager::get()
                          ->createAchievementsStatus(xml_achievements);
}   // initRemainingData

//------------------------------------------------------------------------------
/** Initialises the story- and achievement data structure in case of the first
 *  start of STK.
 */
void PlayerProfile::initRemainingData()
{
    m_story_mode_status = unlock_manager->createStoryModeStatus();
    m_achievements_status =
        AchievementsManager::get()->createAchievementsStatus();

}   // initRemainingData
//------------------------------------------------------------------------------
/** Writes the data for this player to the specified UTFWriter.
 *  \param out The utf writer to write the data to.
 */
void PlayerProfile::save(UTFWriter &out)
{
    out << L"    <player name=\"" << m_local_name
        << L"\" guest=\""         << m_is_guest_account
        << L"\" use-frequency=\"" << m_use_frequency << L"\"\n";

    out << L"            is-default=\"" << m_is_default
        << L"\" unique-id=\""           << m_unique_id
        << L"\" saved-session=\""       << m_saved_session << L"\"\n";

    out << L"            saved-user=\"" << m_saved_user_id 
        << L"\" saved-token=\""         << m_saved_token << L"\">\n";

    {
        if(m_story_mode_status)
            m_story_mode_status->save(out);

        if(m_achievements_status)
            m_achievements_status->save(out);
    }
    out << L"    </player>\n";
}   // save

//------------------------------------------------------------------------------
// ------------------------------------------------------------------------
/** Saves the online data, so that it will automatically re-connect
*  next time this profile is loaded.
*  \param user_id Id of the online profile.
*  \param token Token used for authentication.
*/
void PlayerProfile::saveSession(int user_id, const std::string &token)
{
    m_saved_session = true;
    m_saved_user_id = user_id;
    m_saved_token   = token;
    PlayerManager::get()->save();
}   // saveSession

// ------------------------------------------------------------------------
/** Unsets any saved session data. */
void PlayerProfile::clearSession()
{
    m_saved_session = false;
    m_saved_user_id = 0;
    m_saved_token   = "";
    PlayerManager::get()->save();
}   // clearSession

//------------------------------------------------------------------------------
/** Increments how often that account was used. Guest accounts are not counted.
 */
void PlayerProfile::incrementUseFrequency()
{
    if (m_is_guest_account) m_use_frequency = -1;
    else m_use_frequency++;
}   // incrementUseFrequency

// ------------------------------------------------------------------------
/** Notification of a finished race, which can trigger fulfilling
*  challenges. */
void PlayerProfile::raceFinished()
{
    m_story_mode_status->raceFinished();
    m_achievements_status->onRaceEnd();
}   // raceFinished

//------------------------------------------------------------------------------
/** Comparison used to sort players.
 */
bool PlayerProfile::operator<(const PlayerProfile &other)
{
    return m_use_frequency < other.m_use_frequency;
}   // operator<

// -----------------------------------------------------------------------------
/** \brief Needed for toggling sort order **/
bool PlayerProfile::operator>(const PlayerProfile &other)
{
    return m_use_frequency > other.m_use_frequency;
}   // operator>

// -----------------------------------------------------------------------------

