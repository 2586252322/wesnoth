/* $Id$ */
/*
   Copyright (C) 2007 - 2008 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

//! @file canvas.cpp
//! Implementation of canvas.hpp.

#include "gui/widgets/canvas.hpp"

#include "config.hpp"
#include "font.hpp"
#include "formula.hpp"
#include "image.hpp"
#include "log.hpp"
#include "serialization/parser.hpp"
#include "variable.hpp"

#include <algorithm>
#include <cassert>

#define DBG_G LOG_STREAM(debug, gui)
#define LOG_G LOG_STREAM(info, gui)
#define WRN_G LOG_STREAM(warn, gui)
#define ERR_G LOG_STREAM(err, gui)

#define DBG_G_D LOG_STREAM(debug, gui_draw)
#define LOG_G_D LOG_STREAM(info, gui_draw)
#define WRN_G_D LOG_STREAM(warn, gui_draw)
#define ERR_G_D LOG_STREAM(err, gui_draw)

#define DBG_G_E LOG_STREAM(debug, gui_event)
#define LOG_G_E LOG_STREAM(info, gui_event)
#define WRN_G_E LOG_STREAM(warn, gui_event)
#define ERR_G_E LOG_STREAM(err, gui_event)

#define DBG_G_P LOG_STREAM(debug, gui_parse)
#define LOG_G_P LOG_STREAM(info, gui_parse)
#define WRN_G_P LOG_STREAM(warn, gui_parse)
#define ERR_G_P LOG_STREAM(err, gui_parse)

static Uint32 decode_colour(const std::string& colour);

static Uint32 decode_colour(const std::string& colour)
{
	std::vector<std::string> fields = utils::split(colour);

	// make sure we have four fields
	while(fields.size() < 4) fields.push_back("0");

	Uint32 result = 0;
	for(int i = 0; i < 4; ++i) {
		// shift the previous value before adding, since it's a nop on the
		// first run there's no need for an if.
		result = result << 8;
		result |= lexical_cast_default<int>(fields[i]);
	}

	return result;
}

//! A value can be either a number of a formula, if between brackets it is a formula
//! else it will be read as an unsigned. 
//! If empty neither is modified otherwise only the type it is is modified.
static void read_possible_formula(const std::string str, unsigned& value, std::string& formula)
{
	if(str.empty()) {
		return;
	}

	if(str[0] == '(') {
		formula = str;
	} else {
		value = lexical_cast_default<unsigned>(str);
	}
}

namespace gui2{



tcanvas::tcanvas() :
	shapes_(),
	dirty_(true),
	w_(0),
	h_(0),
	canvas_(),
	variables_()
{
}

tcanvas::tcanvas(const config& cfg) :
	shapes_(),
	dirty_(true),
	w_(0),
	h_(0),
	canvas_(),
	variables_()
{
	parse_cfg(cfg);
}

void tcanvas::draw(const config& cfg)
{
	parse_cfg(cfg);
	draw(true);
}

void tcanvas::draw(const bool force)
{
	log_scope2(gui_draw, "Canvas: drawing.");
	if(!dirty_ && !force) {
		DBG_G_D << "Canvas: nothing to draw.\n";
		return;
	}

	if(dirty_) {
		variables_.add("width",variant(w_));
		variables_.add("height",variant(h_));
	}

	// set surface sizes (do nothing now)
#if 0	
	if(fixme test whether -> can be used otherwise we crash w_ != canvas_->w || h_ != canvas_->h) {
		// create new

	} else {
		// fill current
	}
#endif
	// instead we overwrite the entire thing for now
	DBG_G_D << "Canvas: create new empty canvas.\n";
	canvas_.assign(SDL_CreateRGBSurface(SDL_SWSURFACE, w_, h_, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000));

	// draw items 
	for(std::vector<tshape_ptr>::iterator itor = 
			shapes_.begin(); itor != shapes_.end(); ++itor) {
		log_scope2(gui_draw, "Canvas: draw shape.");
		
		(*itor)->draw(canvas_, variables_);
	}

	dirty_ = false;
}

void tcanvas::parse_cfg(const config& cfg)
{
	log_scope2(gui_parse, "Canvas: parsing config.");
	shapes_.clear();

	for(config::all_children_iterator itor = 
			cfg.ordered_begin(); itor != cfg.ordered_end(); ++itor) {

		const std::string& type = *((*itor).first);;
		const vconfig data(&(*((*itor).second)));

		DBG_G_P << "Canvas: found shape of the type " << type << ".\n";

		if(type == "line") {
			shapes_.push_back(new tline(data));
		} else if(type == "rectangle") {
			shapes_.push_back(new trectangle(data));
		} else if(type == "image") {
			shapes_.push_back(new timage(data));
		} else if(type == "text") {
			shapes_.push_back(new ttext(data));
		} else {
			ERR_G_P << "Canvas: found a shape of an invalid type " << type << ".\n";
			assert(false); // FIXME remove in production code.
		}
	}
}

void tcanvas::tshape::put_pixel(unsigned start, Uint32 colour, unsigned w, unsigned x, unsigned y)
{
	// fixme the 4 is true due to Uint32..
	*reinterpret_cast<Uint32*>(start + (y * w * 4) + x * 4) = colour;
}

// the surface should be locked
// the colour should be a const and the value send should already
// be good for the wanted surface
void tcanvas::tshape::draw_line(surface& canvas, Uint32 colour, 
		const int x1, int y1, const int x2, int y2)
{
	colour = SDL_MapRGBA(canvas->format, 
		((colour & 0xFF000000) >> 24),
		((colour & 0x00FF0000) >> 16),
		((colour & 0x0000FF00) >> 8),
		((colour & 0x000000FF)));

	unsigned start = reinterpret_cast<unsigned>(canvas->pixels);
	unsigned w = canvas->w;

	DBG_G_D << "Shape: draw line from :" 
		<< x1 << ',' << y1 << " to : " << x2 << ',' << y2
		<< " canvas width " << w << " canvas height "
		<< canvas->h << ".\n";

	// use a special case for vertical lines
	if(x1 == x2) {
		if(y2 < y1) {
			std::swap(y1, y2);	
		}

		for(int y = y1; y <= y2; ++y) {
			put_pixel(start, colour, w, x1, y);
		}
		return;
	} 

	// use a special case for horizontal lines
	if(y1 == y2) {
		for(int x  = x1; x <= x2; ++x) {
			put_pixel(start, colour, w, x, y1);
		}
		return;
	} 

	// draw based on Bresenham on wikipedia
	int dx = x2 - x1;
	int dy = y2 - y1;
	int slope = 1;
	if (dy < 0) {
		slope = -1;
		dy = -dy;
	}

	// Bresenham constants
	int incE = 2 * dy;
	int incNE = 2 * dy - 2 * dx;
	int d = 2 * dy - dx;
	int y = y1;

	// Blit
	for (int x = x1; x <= x2; ++x) {
		put_pixel(start, colour, w, x, y);
		if (d <= 0) {
			d += incE;
		} else {
			d += incNE;
			y += slope;
		}
	}
}

tcanvas::tline::tline(const int x1, const int y1, const int x2,
		const int y2, const Uint32 colour, const unsigned thickness) :
	x1_(x1),
	y1_(y1),
	x2_(x2),
	y2_(y2),
	colour_(colour),
	thickness_(thickness) 
{
}

tcanvas::tline::tline(const vconfig& cfg) :
	x1_(lexical_cast_default<int>(cfg["x1"])),
	y1_(lexical_cast_default<int>(cfg["y1"])),
	x2_(lexical_cast_default<int>(cfg["x2"])),
	y2_(lexical_cast_default<int>(cfg["y2"])),
	colour_(decode_colour(cfg["colour"])),
	thickness_(lexical_cast_default<unsigned>(cfg["thickness"]))
{
/*WIKI
 * [line]
 *     x1, y1 = (int = 0), (int = 0) The startpoint of the line.
 *     x2, y2 = (int = 0), (int = 0) The endpoint of the line.
 *     colour = (widget.colour = "") The colour of the line.
 *     thickness = (uint = 0)        The thickness of the line.
 *     debug = (string = "")         Debug message to show upon creation
 *                                   this message is not stored.
 * [/line]
 */

// This code can be used by a parser to generate the wiki page
// structure
// [tag name]
// param = type_info description
//
// param = key (, key)
// 
// type_info = ( type = default_value) the info about a optional parameter
// type_info = type                        info about a mandatory parameter
// type_info = [ type_infp ]               info about a conditional parameter description should explain the reason
//
// description                             description of the parameter
//
	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_G_P << "Line: found debug message '" << debug << "'.\n";
	}

}

void tcanvas::tline::draw(surface& canvas,
	const game_logic::map_formula_callable& variables)
{
	DBG_G_D << "Line: draw from :" 
		<< x1_ << ',' << y1_ << " to: " << x2_ << ',' << y2_ << '\n';

	// we wrap around the coordinates, this might be moved to be more
	// generic place, but leave it here for now. Note the numbers are
	// negative so adding them is subtracting them.
	
	if(x1_ < 0) x1_ += canvas->w;
	if(x2_ < 0) x2_ += canvas->w;
	if(y1_ < 0) y1_ += canvas->h;
	if(y2_ < 0) y2_ += canvas->h;

	// FIXME validate the line is on the surface !!!

	// now draw the line we use Bresenham's algorithm, which doesn't
	// support antialiasing. The advantage is that it's easy for testing.
	
	// lock the surface
	surface_lock locker(canvas);
	if(x1_ > x2_) {
		// invert points
		draw_line(canvas, colour_, x2_, y2_, x1_, y1_);
	} else {
		draw_line(canvas, colour_, x1_, y1_, x2_, y2_);
	}
	
}

tcanvas::trectangle::trectangle(const vconfig& cfg) :
	x_(0),
	y_(0),
	w_(0),
	h_(0),
	x_formula_(),
	y_formula_(),
	w_formula_(),
	h_formula_(),
	border_thickness_(lexical_cast_default<unsigned>(cfg["border_thickness"])),
	border_colour_(decode_colour(cfg["border_colour"])),
	fill_colour_(decode_colour(cfg["fill_colour"]))
{
/*WIKI
 * [rectangle]
 *     x, y = (int = 0), (int = 0)    The top left corner of the rectangle.
 *     w = (int = 0)                  The width of the rectangle.
 *     h = (int = 0)                  The height of the rectangle.
 *     border_thickness = (uint = 0)  The thickness of the border if the 
 *                                    thickness is zero it's not drawn.
 *     border_colour = (widget.colour = "") 
 *                                    The colour of the border if empty it's
 *                                    not drawn.
 *     fill_colour = (widget.colour = "")
 *                                    The colour of the interior if ommitted
 *                                    it's not draw (transparent is draw but
 *                                    does nothing).
 *     debug = (string = "")          Debug message to show upon creation
 *                                    this message is not stored.
 * [/rectangle]
 */

	read_possible_formula(cfg["x"], x_, x_formula_);
	read_possible_formula(cfg["y"], y_, y_formula_);
	read_possible_formula(cfg["w"], w_, w_formula_);
	read_possible_formula(cfg["h"], h_, h_formula_);

	if(border_colour_ == 0) {
		border_thickness_ = 0;
	}

	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_G_P << "Rectangle: found debug message '" << debug << "'.\n";
	}
}

void tcanvas::trectangle::draw(surface& canvas,
	const game_logic::map_formula_callable& variables)
{

	//@todo formulas are now recalculated every draw cycle which is a 
	// bit silly unless there has been a resize. So to optimize we should
	// use an extra flag or do the calculation in a separate routine.
	if(!y_formula_.empty()) {
		DBG_G_D << "Rectangle execute y formula '" << y_formula_ << "'.\n";
		y_ = game_logic::formula(y_formula_).execute(variables).as_int();
	}

	if(!x_formula_.empty()) {
		DBG_G_D << "Rectangle execute x formula '" << x_formula_ << "'.\n";
		x_ = game_logic::formula(x_formula_).execute(variables).as_int();
	}

	if(!w_formula_.empty()) {
		DBG_G_D << "Rectangle execute width formula '" << w_formula_ << "'.\n";
		w_ = game_logic::formula(w_formula_).execute(variables).as_int();
	}

	if(!h_formula_.empty()) {
		DBG_G_D << "Rectangle execute height formula '" << h_formula_ << "'.\n";
		h_ = game_logic::formula(h_formula_).execute(variables).as_int();
	}


	DBG_G_D << "Rectangle: draw from :" << x_ << ',' << y_
		<< " width: " << w_ << " height: " << h_ << '\n';

	surface_lock locker(canvas);

	//FIXME validate the input.

	// draw the border
	for(unsigned i = 0; i < border_thickness_; ++i) {

		const unsigned left = x_ + i;
		const unsigned right = left + w_ - (i * 2) - 1;
		const unsigned top = y_ + i;
		const unsigned bottom = top + h_ - (i * 2) - 1;

		// top horizontal (left -> right)
		draw_line(canvas, border_colour_, left, top, right, top);

		// right vertical (top -> bottom)
		draw_line(canvas, border_colour_, right, top, right, bottom);

		// bottom horizontal (left -> right)
		draw_line(canvas, border_colour_, left, bottom, right, bottom);

		// left vertical (top -> bottom)
		draw_line(canvas, border_colour_, left, top, left, bottom);

	}

	// The fill_rect_alpha code below fails, can't remember the exact cause
	// so use the slow line drawing method to fill the rect.
	if(fill_colour_) {
		
		const unsigned left = x_ + border_thickness_;
		const unsigned right = left + w_ - (2 * border_thickness_) - 1;
		const unsigned top = y_ + border_thickness_;
		const unsigned bottom = top + h_ - (2 * border_thickness_);

		for(unsigned i = top; i < bottom; ++i) {
			
			draw_line(canvas, fill_colour_, left, i, right, i);
		}
	}
/*
	const unsigned left = x_ + border_thickness_ + 1;
	const unsigned top = y_ + border_thickness_ + 1;
	const unsigned width = w_ - (2 * border_thickness_) - 2;
	const unsigned height = h_ - (2 * border_thickness_) - 2;
	SDL_Rect rect = create_rect(left, top, width, height);

	const Uint32 colour = fill_colour_ & 0xFFFFFF00;
	const Uint8 alpha = fill_colour_ & 0xFF;

	// fill
	fill_rect_alpha(rect, colour, alpha, canvas);
	canvas = blend_surface(canvas, 255, 0xAAAA00);	
*/	
}

tcanvas::timage::timage(const vconfig& cfg) :
	src_clip_(),
	dst_clip_(),
	image_()
{
/*WIKI
 * [image]
 *     name = (string)                The name of the image.
 *     debug = (string = "")          Debug message to show upon creation
 * [/image]
 */

	image_.assign(image::get_image(image::locator(cfg["name"])));
	src_clip_ = create_rect(0, 0, image_->w, image_->h);

	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_G_P << "Image: found debug message '" << debug << "'.\n";
	}
}

void tcanvas::timage::draw(surface& canvas,
	const game_logic::map_formula_callable& variables)
{
	DBG_G_D << "Image: draw.\n";

	SDL_Rect src_clip = src_clip_;
	SDL_Rect dst_clip = dst_clip_;
	SDL_BlitSurface(image_, &src_clip, canvas, &dst_clip);
}

tcanvas::ttext::ttext(const vconfig& cfg) :
	x_(lexical_cast_default<unsigned>(cfg["x"])),
	y_(lexical_cast_default<unsigned>(cfg["y"])),
	w_(lexical_cast_default<unsigned>(cfg["w"])),
	h_(lexical_cast_default<unsigned>(cfg["h"])),
	font_size_(lexical_cast_default<unsigned>(cfg["font_size"])),
	colour_(decode_colour(cfg["colour"])),
	text_(cfg["text"])
{	
/*WIKI
 * [text]
 *     x, y = (unsigned = 0), (unsigned = 0)    
 *                                    The top left corner of the bounding
 *                                    rectangle.
 *     w = (unsigned = 0)             The width of the bounding rectangle.
 *     h = (unsigned = 0)             The height of the bounding rectangle.
 *     font_size = (unsigned = 0)     The size of the font.
 *     colour = (widget.colour = "")  The colour of the text.
 *     text = (t_string = "")         The text to print, for now always printed
 *                                    centered in the area.
 *     debug = (string = "")          Debug message to show upon creation
 *                                    this message is not stored.
 * [/rectangle]
 */

	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_G_P << "Text: found debug message '" << debug << "'.\n";
	}
}

void tcanvas::ttext::draw(surface& canvas,
	const game_logic::map_formula_callable& variables)
{
	DBG_G_D << "Text: draw at " << x_ << ',' << y_ << " text '"
		<< text_ << "'.\n";

	SDL_Color col = { (colour_ >> 24), (colour_ >> 16), (colour_ >> 8), colour_ };
	surface surf(font::get_rendered_text(text_, font_size_, col, TTF_STYLE_NORMAL));

	if(surf->w > w_) {
		WRN_G_D << "Text: text is too wide for the canvas and will be clipped.\n";
	}
	
	if(surf->h > h_) {
		WRN_G_D << "Text: text is too high for the canvas and will be clipped.\n";
	}
	
	unsigned x_off = (surf->w >= w_) ? 0 : ((w_ - surf->w) / 2);
	unsigned y_off = (surf->h >= h_) ? 0 : ((h_ - surf->h) / 2);
	unsigned w_max = w_ - x_ - x_off;
	unsigned h_max = h_ - y_ - y_off;

	SDL_Rect dst = { x_ + x_off, y_ + y_off, w_max, h_max };
	SDL_BlitSurface(surf, 0, canvas, &dst);
}

} // namespace gui2
