/*
	------------------------------------------------------------------

	This file is part of the Open Ephys GUI
	Copyright (C) 2022 Open Ephys

	------------------------------------------------------------------

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef FILEHEADERS_H_DEFINED
#define FILEHEADERS_H_DEFINED

#include <RecordingLib.h>

#include <stdio.h>
#include <map>

#include "Definitions.h"

String getFormatDescription(const ChannelInfoObject* ch)
{
	if (ch->getType() == InfoObject::Type::EVENT_CHANNEL)
	{
		return "header.description = 'each record contains one 64-bit timestamp, "
			"one 16-bit sample position, one uint8 event type, one uint8 processor ID, "
			"one uint8 event ID, one uint8 event channel, and one uint16 recordingNumber'; \n";
	}
	else if (ch->getType() == InfoObject::Type::CONTINUOUS_CHANNEL)
	{
		return "header.description = 'each record contains one 64-bit timestamp, "
			"one 16-bit sample count (N), 1 uint16 recordingNumber, N 16-bit samples, "
			"and one 10-byte record marker (0 1 2 3 4 5 6 7 8 255)'; \n";
	}
	else if (ch->getType() == InfoObject::Type::SPIKE_CHANNEL)
	{
		return "header.description = 'Each record contains 1 uint8 eventType, 1 int64 timestamp, 1 int64 software timestamp, "
			"1 uint16 sourceID, 1 uint16 numChannels (n), 1 uint16 numSamples (m), 1 uint16 sortedID, 1 uint16 electrodeID, "
			"1 uint16 channel, 3 uint8 color codes, 2 float32 component projections, n*m uint16 samples, n float32 channelGains, n uint16 thresholds, and 1 uint16 recordingNumber'; \n";
	}
}

String getEventChannelHeaderText(const ChannelInfoObject* ch)
{

	String header = "";

	header += "header.channel = 'Events';\n";
	header += "header.channelType = 'Event';\n";
	header += ";\n";
	header += "header.blockLength = ";
	header += BLOCK_LENGTH;
	header += ";\n";

	return header;
}

String getContinuousChannelHeaderText(const ChannelInfoObject* ch)
{
	String header = "";

	header += "header.channel = '";
	header += ch->getName();
	header += "';\n";
	header += "header.channelType = 'Continuous';\n";
	header += "header.sampleRate = ";
	header += String(ch->getSampleRate());
	header += ";\n";
	header += "header.blockLength = ";
	header += BLOCK_LENGTH;
	header += ";\n";

	header += "header.bitVolts = ";
	header += String(dynamic_cast<const ContinuousChannel*>(ch)->getBitVolts());
	header += ";\n";

	return header;

}

String getSpikeChannelHeaderText(const SpikeChannel* ch)
{
	String header = "";

	header += "header.electrode = '";
	header += ch->getName();
	header += "';\n";

	header += "header.num_channels = ";
	header += String(ch->getNumChannels());
	header += ";\n";

	header += "header.sampleRate = ";
	header += String(ch->getSampleRate());
	header += ";\n";

	header += "header.samplesPerSpike = ";
	header += String(ch->getTotalSamples());
	header += ";\n";

	return header;

}

String generateHeader(const ChannelInfoObject* ch, String dateString)
{
	String header = "header.format = 'Open Ephys Data Format'; \n";

	header += "header.version = " + String(VERSION_STRING) + "; \n";
	header += "header.header_bytes = ";
	header += String(HEADER_SIZE);
	header += ";\n";

	header += getFormatDescription(ch);

	header += "header.date_created = '";
	header += dateString;
	header += "';\n";

	switch (ch->getType())
	{
	case InfoObject::Type::EVENT_CHANNEL:
		header += getEventChannelHeaderText(ch);
		break;
	case InfoObject::Type::CONTINUOUS_CHANNEL:
		header += getContinuousChannelHeaderText(ch);
		break;
	case InfoObject::Type::SPIKE_CHANNEL:
		header += getSpikeChannelHeaderText((const SpikeChannel*) ch);
		break;
	}

	header = header.paddedRight(' ', HEADER_SIZE);

	return header;

}

#endif
