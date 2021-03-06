/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 Soeren Apel <soeren@apelpie.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_DATA_SIGNALBASE_HPP
#define PULSEVIEW_PV_DATA_SIGNALBASE_HPP

#include <atomic>
#include <deque>
#include <condition_variable>
#include <thread>
#include <vector>

#include <QColor>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVariant>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "segment.hpp"

using std::atomic;
using std::condition_variable;
using std::deque;
using std::enable_shared_from_this;
using std::map;
using std::mutex;
using std::pair;
using std::shared_ptr;
using std::vector;

namespace sigrok {
class Channel;
}

namespace pv {
namespace data {

class Analog;
class AnalogSegment;
class DecoderStack;
class Logic;
class LogicSegment;
class Segment;
class SignalBase;
class SignalData;

class SignalGroup : public QObject
{
	Q_OBJECT

public:
	SignalGroup(const QString& name);

	void append_signal(shared_ptr<SignalBase> signal);
	void remove_signal(shared_ptr<SignalBase> signal);
	deque<shared_ptr<SignalBase>> signals() const;
	void clear();

	const QString name() const;

private:
	deque<shared_ptr<SignalBase>> signals_;
	QString name_;
};


class SignalBase : public QObject, public enable_shared_from_this<SignalBase>
{
	Q_OBJECT
	Q_PROPERTY(QString error_message READ get_error_message NOTIFY error_message_changed)

public:
	enum ChannelType {
		AnalogChannel = 1, ///< Analog data
		LogicChannel,  ///< Logic data
		DecodeChannel, ///< Protocol Decoder channel using libsigrokdecode
		MathChannel    ///< Virtual channel generated by math operations
	};

	enum ConversionType {
		NoConversion = 0,
		A2LConversionByThreshold = 1,
		A2LConversionBySchmittTrigger = 2
	};

	/**
	 * Conversion presets range from -1 to n, where 1..n are dependent on
	 * the conversion these presets apply to. -1 and 0 have fixed meanings,
	 * however.
	 */
	enum ConversionPreset {
		NoPreset = -1,     ///< Conversion uses custom values
		DynamicPreset = 0  ///< Conversion uses calculated values
	};

	static const QColor AnalogSignalColors[8];
	static const QColor LogicSignalColors[10];

private:
	static const int ColorBGAlpha;
	static const uint64_t ConversionBlockSize;
	static const uint32_t ConversionDelay;

public:
	SignalBase(shared_ptr<sigrok::Channel> channel, ChannelType channel_type);
	virtual ~SignalBase();

public:
	/**
	 * Returns the underlying SR channel.
	 * Generated channels don't have a SR channel.
	 */
	shared_ptr<sigrok::Channel> channel() const;

	/**
	 * Returns whether this channel is generated or a channel associated with the device.
	 */
	bool is_generated() const;

	/**
	 * Returns enabled status of this channel.
	 */
	bool enabled() const;

	/**
	 * Sets the enabled status of this channel.
	 * @param value Boolean value to set.
	 */
	void set_enabled(bool value);

	/**
	 * Gets the type of this channel.
	 */
	ChannelType type() const;

	/**
	 * Gets the index number of this channel, i.e. a unique ID assigned by
	 * the device driver.
	 */
	unsigned int index() const;

	/**
	 * Sets the index number of this channel, i.e. a unique ID assigned by
	 * the device driver or the logic bit index (see below).
	 * Only use immediately after creating the signal and leave it untouched after.
	 */
	void set_index(unsigned int index);

	/**
	 * Returns which bit of a given sample for this signal represents the
	 * signal itself. This is relevant for compound signals like logic,
	 * rather meaningless for everything else but provided in case there
	 * is a conversion active that provides a digital signal using bit #0.
	 */
	unsigned int logic_bit_index() const;

	/**
	 * Sets the signal group this signal belongs to
	 */
	void set_group(SignalGroup* group);

	/**
	 * Returns the signal group this signal belongs to or nullptr if none
	 */
	SignalGroup* group() const;

	/**
	 * Gets the name of this signal.
	 */
	QString name() const;

	/**
	 * Gets the internal name of this signal, i.e. how the device/generator calls it.
	 */
	QString internal_name() const;

	/**
	 * Sets the internal name of this signal, i.e. how the device/generator calls it.
	 * Only use immediately after creating the signal and leave it untouched after.
	 */
	void set_internal_name(QString internal_name);

	/**
	 * Produces a string for this signal that can be used for display,
	 * i.e. it contains one or both of the signal/internal names.
	 */
	QString display_name() const;

	/**
	 * Sets the name of the signal.
	 */
	virtual void set_name(QString name);

	/**
	 * Get the color of the signal.
	 */
	QColor color() const;

	/**
	 * Set the color of the signal.
	 */
	void set_color(QColor color);

	/**
	 * Get the background color of the signal.
	 */
	QColor bgcolor() const;

	/**
	 * Returns the current error message text.
	 */
	virtual QString get_error_message() const;

	/**
	 * Sets the internal data object.
	 */
	void set_data(shared_ptr<pv::data::SignalData> data);

	/**
	 * Clears all sample data and removes all associated segments.
	 */
	void clear_sample_data();

	/**
	 * Get the internal data as analog data object in case of analog type.
	 */
	shared_ptr<pv::data::Analog> analog_data() const;

	/**
	 * Get the internal data as logic data object in case of logic type.
	 */
	shared_ptr<pv::data::Logic> logic_data() const;

	/**
	 * Get the primary internal data object, i.e. the data that was acquired from the device.
	 */
	shared_ptr<pv::data::SignalData> data() const;

	/**
	 * Determines whether a given segment is complete (i.e. end-of-frame has
	 * been seen). It only considers the original data, not the converted data.
	 */
	bool segment_is_complete(uint32_t segment_id) const;

	/**
	 * Determines whether this signal has any sample data at all.
	 */
	bool has_samples() const;

	/**
	 * Returns the sample rate for this signal.
	 */
	virtual double get_samplerate() const;

	/**
	 * Queries the kind of conversion performed on this channel.
	 */
	ConversionType get_conversion_type() const;

	/**
	 * Changes the kind of conversion performed on this channel.
	 *
	 * Restarts the conversion.
	 */
	void set_conversion_type(ConversionType t);

	/**
	 * Returns all currently known conversion options
	 */
	map<QString, QVariant> get_conversion_options() const;

	/**
	 * Sets the value of a particular conversion option
	 * Note: it is not checked whether the option is valid for the
	 * currently conversion. If it's not, it will be silently ignored.
	 *
	 * Does not restart the conversion.
	 *
	 * @return true if the value is different from before, false otherwise
	 */
	bool set_conversion_option(QString key, QVariant value);

	/**
	 * Returns the threshold(s) used for conversions, if applicable.
	 * The resulting thresholds are given for the chosen conversion, so you
	 * can query thresholds also for conversions which aren't currently active.
	 *
	 * If you want the thresholds for the currently active conversion,
	 * call it either with NoConversion or no parameter.
	 *
	 * @param t the type of conversion to obtain the thresholds for, leave
	 *          empty or use NoConversion if you want to query the currently
	 *          used conversion
	 *
	 * @param always_custom ignore the currently selected preset and always
	 *        return the custom values for this conversion, using 0 if those
	 *        aren't set
	 *
	 * @return a list of threshold(s) used by the chosen conversion
	 */
	vector<double> get_conversion_thresholds(
		const ConversionType t = NoConversion, const bool always_custom=false) const;

	/**
	 * Provides all conversion presets available for the currently active
	 * conversion.
	 *
	 * @return a list of description/ID pairs for each preset
	 */
	vector<pair<QString, int> > get_conversion_presets() const;

	/**
	 * Determines the ID of the currently used conversion preset, which is only
	 * valid for the currently available conversion presets. It is therefore
	 * suggested to call @ref get_conversion_presets right before calling this.
	 *
	 * @return the ID of the currently used conversion preset. -1 if no preset
	 *         is used. In that case, a user setting is used instead.
	 */
	ConversionPreset get_current_conversion_preset() const;

	/**
	 * Sets the conversion preset to be used.
	 *
	 * Does not restart the conversion.
	 *
	 * @param id the id of the preset to use
	 */
	void set_conversion_preset(ConversionPreset id);

#ifdef ENABLE_DECODE
	bool is_decode_signal() const;
#endif

	virtual void save_settings(QSettings &settings) const;
	virtual void restore_settings(QSettings &settings);

	void start_conversion(bool delayed_start=false);

protected:
	virtual void set_error_message(QString msg);

private:
	void stop_conversion();

	bool conversion_is_a2l() const;

	uint8_t convert_a2l_threshold(float threshold, float value);
	uint8_t convert_a2l_schmitt_trigger(float lo_thr, float hi_thr,
		float value, uint8_t &state);

	void convert_single_segment_range(shared_ptr<AnalogSegment> asegment,
		shared_ptr<LogicSegment> lsegment, uint64_t start_sample, uint64_t end_sample);
	void convert_single_segment(shared_ptr<AnalogSegment> asegment,
		shared_ptr<LogicSegment> lsegment);
	void conversion_thread_proc();

Q_SIGNALS:
	void enabled_changed(const bool &value);
	void name_changed(const QString &name);
	void color_changed(const QColor &color);
	void error_message_changed(QString msg);
	void conversion_type_changed(const ConversionType t);

	void samples_cleared();
	void samples_added(uint64_t segment_id, uint64_t start_sample,
		uint64_t end_sample);

	void min_max_changed(float min, float max);

private Q_SLOTS:
	void on_samples_cleared();

	void on_samples_added(SharedPtrToSegment segment, uint64_t start_sample,
		uint64_t end_sample);

	void on_input_segment_completed();

	void on_min_max_changed(float min, float max);

	void on_capture_state_changed(int state);

	void on_delayed_conversion_start();

protected:
	shared_ptr<sigrok::Channel> channel_;
	ChannelType channel_type_;
	SignalGroup* group_;
	shared_ptr<pv::data::SignalData> data_;
	shared_ptr<pv::data::SignalData> converted_data_;
	ConversionType conversion_type_;
	map<QString, QVariant> conversion_options_;

	float min_value_, max_value_;

	std::thread conversion_thread_;
	atomic<bool> conversion_interrupt_;
	mutex conversion_input_mutex_;
	condition_variable conversion_input_cond_;
	QTimer delayed_conversion_starter_;

	QString internal_name_, name_;
	QColor color_, bgcolor_;
	unsigned int index_;

	QString error_message_;
};

} // namespace data
} // namespace pv

Q_DECLARE_METATYPE(shared_ptr<pv::data::SignalBase>);

#endif // PULSEVIEW_PV_DATA_SIGNALBASE_HPP
