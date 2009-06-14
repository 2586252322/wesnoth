/* $Id$ */
/*
   Copyright (C) 2008 - 2009 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/title_screen.hpp"

#include "game_config.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "gui/dialogs/addon_connect.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/window.hpp"
#include "titlescreen.hpp"

static lg::log_domain log_config("config");
#define ERR_CF LOG_STREAM(err, log_config)

namespace gui2 {

namespace {

template<class D>
void show_dialog(twidget* caller)
{
	ttitle_screen *dialog = dynamic_cast<ttitle_screen*>(caller->dialog());
	assert(dialog);

	D dlg;
	dlg.show(*(dialog->video()));
}

} // namespace

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_title_screen
 *
 * == Title screen ==
 *
 * This shows the title screen.
 */

ttitle_screen::ttitle_screen()
	: video_(NULL)
	, tips_()
{
	read_tips_of_day(tips_);
}

twindow* ttitle_screen::build_window(CVideo& video)
{
	return build(video, get_id(TITLE_SCREEN));
}

void ttitle_screen::pre_show(CVideo& video, twindow& window)
{
	assert(!video_);
	video_ = &video;

	set_restore(false);

	window.canvas()[0].set_variable("revision_number",
		variant(_("Version") + std::string(" ") + game_config::revision));

	/**** Set the buttons ****/
	window.get_widget<tbutton>("addons", false).
			set_callback_mouse_left_click(show_dialog<gui2::taddon_connect>);

	// Note changing the language doesn't upate the title screen...
	window.get_widget<tbutton>("language", false).
			set_callback_mouse_left_click(
				show_dialog<gui2::tlanguage_selection>);

	/**** Set the tip of the day ****/
	update_tip(window, true);

	window.get_widget<tbutton>("next_tip", false).
			set_callback_mouse_left_click(next_tip);

	window.get_widget<tbutton>("previous_tip", false).
			set_callback_mouse_left_click(previous_tip);

	/***** Select a random game_title *****/
	std::vector<std::string> game_title_list =
		utils::split(game_config::game_title
				, ','
				, utils::STRIP_SPACES | utils::REMOVE_EMPTY);

	if(game_title_list.empty()) {
		ERR_CF << "No title image defined\n";
	} else {
		window.canvas()[0].set_variable("background_image",
			variant(game_title_list[rand()%game_title_list.size()]));
	}
}

void ttitle_screen::post_show(twindow& /*window*/)
{
	video_ = NULL;
}

void ttitle_screen::update_tip(twindow& window, const bool previous)
{
	next_tip_of_day(tips_, previous);
	const config *tip = get_tip_of_day(tips_);
	assert(tip);

	window.get_widget<tlabel>("tip", false).set_label((*tip)["text"]);
	window.get_widget<tlabel>("source", false).set_label((*tip)["source"]);

	/**
	 * @todo Convert the code to use a multi_page so the invalidate is not
	 * needed.
	 */
	window.invalidate_layout();
}

void ttitle_screen::next_tip(twidget* caller)
{
	ttitle_screen *dialog = dynamic_cast<ttitle_screen*>(caller->dialog());
	assert(dialog);

	twindow *window = caller->get_window();
	assert(window);

	dialog->update_tip(*window, true);
}

void ttitle_screen::previous_tip(twidget* caller)
{
	ttitle_screen *dialog = dynamic_cast<ttitle_screen*>(caller->dialog());
	assert(dialog);

	twindow *window = caller->get_window();
	assert(window);

	dialog->update_tip(*window, false);
}

} // namespace gui2
