//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2013 Glenn De Jonghe
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

#include "states_screens/dialogs/login_dialog.hpp"

#include <IGUIEnvironment.h>

#include "audio/sfx_manager.hpp"
#include "challenges/unlock_manager.hpp"
#include "config/player.hpp"
#include "guiengine/engine.hpp"
#include "guiengine/widgets/button_widget.hpp"
#include "guiengine/widgets/label_widget.hpp"
#include "guiengine/widgets/text_box_widget.hpp"
#include "states_screens/state_manager.hpp"
#include "utils/translation.hpp"

using namespace GUIEngine;
using namespace irr;
using namespace irr::gui;

// -----------------------------------------------------------------------------

LoginDialog::LoginDialog(const float w, const float h) :
        ModalDialog(w,h)
{
    m_self_destroy = false;
    loadFromFile("login_dialog.stkgui");

    TextBoxWidget* textCtrl = getWidget<TextBoxWidget>("username");
    assert(textCtrl != NULL);
    textCtrl->setFocusForPlayer(PLAYER_ID_GAME_MASTER);

    //if (translations->isRTLLanguage()) textCtrl->addListener(this);
}

// -----------------------------------------------------------------------------

LoginDialog::~LoginDialog()
{
}

// -----------------------------------------------------------------------------

GUIEngine::EventPropagation LoginDialog::processEvent(const std::string& eventSource)
{
    if (eventSource == "cancel")
    {
        dismiss();
        return GUIEngine::EVENT_BLOCK;
    }
    return GUIEngine::EVENT_LET;
}

// -----------------------------------------------------------------------------

void LoginDialog::onEnterPressedInternal()
{
    // ---- Cancel button pressed
    const int playerID = PLAYER_ID_GAME_MASTER;
    ButtonWidget* cancelButton = getWidget<ButtonWidget>("cancel");
    if (GUIEngine::isFocusedForPlayer(cancelButton, playerID))
    {
        std::string fakeEvent = "cancel";
        processEvent(fakeEvent);
        return;
    }

    // ---- Otherwise, see if we can accept the new name
    TextBoxWidget* textCtrl = getWidget<TextBoxWidget>("username");
    stringw username = textCtrl->getText().trim();
    const int size = username.size();

    // sanity check
    int nonEmptyChars = 0;
    for (int n=0; n<size; n++)
    {
        if (username[n] != L' ')
        {
            nonEmptyChars++;
        }
    }

    if (size > 0 && nonEmptyChars > 0)
    {
        Log::info("Login Dialog","Username : %ls", username.c_str());
        m_self_destroy = true;
    } // if valid name
    else
    {
        LabelWidget* label = getWidget<LabelWidget>("title");
        label->setText(_("Not a valid username"), false);
        sfx_manager->quickSound( "anvil" );
    }
}

// -----------------------------------------------------------------------------

void LoginDialog::onUpdate(float dt)
{
    // It's unsafe to delete from inside the event handler so we do it here
    if (m_self_destroy)
    {
        TextBoxWidget* textCtrl = getWidget<TextBoxWidget>("username");
        stringw playerName = textCtrl->getText().trim();

        // irrLicht is too stupid to remove focus from deleted widgets
        // so do it by hand
        GUIEngine::getGUIEnv()->removeFocus( textCtrl->getIrrlichtElement() );
        GUIEngine::getGUIEnv()->removeFocus( m_irrlicht_window );


        ModalDialog::dismiss();

    }
}