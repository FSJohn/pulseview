/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <extdef.h>

#include <assert.h>
#include <math.h>

#include <QApplication>

#include "signal.h"
#include "view.h"

namespace pv {
namespace view {

const QPen Signal::SignalAxisPen(QColor(128, 128, 128, 64));

const char *const ProbeNames[] = {
	"CLK",
	"DATA",
	"IN",
	"OUT",
	"RST",
	"Tx",
	"Rx",
	"EN",
	"SCLK",
	"MOSI",
	"MISO",
	"/SS",
	"SDA",
	"SCL"
};

Signal::Signal(pv::SigSession &session, const sr_probe *const probe) :
	Trace(session, probe->name),
	_probe(probe),
	_name_action(NULL),
	_name_widget(NULL),
	_updating_name_widget(false)
{
	assert(_probe);
}

void Signal::init_context_bar_actions(QWidget *parent)
{
	_name_widget = new QComboBox(parent);
	_name_widget->setEditable(true);

	_name_action = new QWidgetAction(parent);
	_name_action->setDefaultWidget(_name_widget);

	for(unsigned int i = 0; i < countof(ProbeNames); i++)
		_name_widget->insertItem(i, ProbeNames[i]);
	_name_widget->setEditText(_probe->name);

	connect(_name_widget, SIGNAL(editTextChanged(const QString&)),
		this, SLOT(on_text_changed(const QString&)));
}

void Signal::set_name(QString name)
{
	Trace::set_name(name);
	_updating_name_widget = true;
	_name_widget->setEditText(name);
	_updating_name_widget = false;
}

bool Signal::enabled() const
{
	return _probe->enabled;
}

void Signal::paint_axis(QPainter &p, int y, int left, int right)
{
	p.setPen(SignalAxisPen);
	p.drawLine(QPointF(left, y + 0.5f), QPointF(right, y + 0.5f));
}

void Signal::on_text_changed(const QString &text)
{
	Trace::set_name(text);
	text_changed();
}

} // namespace view
} // namespace pv
