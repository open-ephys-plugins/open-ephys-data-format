/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2021 Open Ephys

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

#include "OpenEphysFileSource.h"

OpenEphysFileSource::OpenEphysFileSource() : m_samplePos(0)
{}


bool OpenEphysFileSource::open(File file)
{

    XmlDocument doc(file);
    std::unique_ptr<XmlElement> xml = doc.getDocumentElement();

    if (xml == 0 || !xml->hasTagName("EXPERIMENT"))
    {
        LOGD("File not found!");
        return false;
    }

	m_rootPath = file.getParentDirectory();

    for (auto* recordTag: xml->getChildIterator())
    {
        if (recordTag->hasTagName("RECORDING"))
        {

			Recording recording;

            recording.id = recordTag->getIntAttribute("number");

            for (auto* streamTag: recordTag->getChildIterator())
            {

				String streamName = streamTag->getStringAttribute("name");
				int sourceNodeId = streamTag->getIntAttribute("source_node_id");
				String sourceNodeName = streamTag->getStringAttribute("source_node_name");
				float sampleRate = streamTag->getIntAttribute("sample_rate") * 1.0f;

                for (auto* channel: streamTag->getChildIterator())
                {

                    ChannelInfo info;

                    info.filename = channel->getStringAttribute("filename");

					if (channel->getTagName() == "CHANNEL")
					{

						info.name = channel->getStringAttribute("name");
						info.bitVolts = channel->getStringAttribute("bitVolts").getDoubleValue();
						info.startPos = channel->getIntAttribute("position");


						if (!recording.streams.count(streamName))
						{
							StreamInfo streamInfo;

							streamInfo.sampleRate = sampleRate;
							streamInfo.startPos = info.startPos;

							std::unique_ptr<MemoryMappedFile> timestampFileMap(new MemoryMappedFile(m_rootPath.getChildFile(info.filename), MemoryMappedFile::readOnly));

							int64* startTimestamp = static_cast<int64*>(timestampFileMap->getData()) + 1024/8;

							streamInfo.startTimestamp = *startTimestamp;

							recording.streams[streamName] = streamInfo;

						}

						recording.streams[streamName].channels.push_back(info);

					}
					else if (channel->getTagName() == "EVENTS")
					{

						File eventsFile = m_rootPath.getChildFile(info.filename);
						int nEvents = (eventsFile.getSize() - EVENT_HEADER_SIZE_IN_BYTES) / BYTES_PER_EVENT;

						if (nEvents > 0)
						{
							std::unique_ptr<MemoryMappedFile> eventFileMap(new MemoryMappedFile(eventsFile, MemoryMappedFile::readOnly));

							EventInfo eventInfo;

							for (int i = 0; i < nEvents; i++)
							{

								int64* timestamp = static_cast<int64*>(eventFileMap->getData()) + EVENT_HEADER_SIZE_IN_BYTES / 8 + i * sizeof(int64) / 4;
								uint8* eventType = static_cast<uint8*>(eventFileMap->getData()) + EVENT_HEADER_SIZE_IN_BYTES + i * BYTES_PER_EVENT + 10;
								uint8* sourceID = static_cast<uint8*>(eventFileMap->getData()) + EVENT_HEADER_SIZE_IN_BYTES + i * BYTES_PER_EVENT + 11;
								uint8* channelState = static_cast<uint8*>(eventFileMap->getData()) + EVENT_HEADER_SIZE_IN_BYTES + i * BYTES_PER_EVENT + 12;
								uint8* channel = static_cast<uint8*>(eventFileMap->getData()) + EVENT_HEADER_SIZE_IN_BYTES + i * BYTES_PER_EVENT + 13;
								uint8* recordingNum = static_cast<uint8*>(eventFileMap->getData()) + EVENT_HEADER_SIZE_IN_BYTES + i * BYTES_PER_EVENT + 14;

								eventInfo.channels.push_back(*channel);
								eventInfo.channelStates.push_back(*reinterpret_cast<juce::int16*>(channelState));

								int64 offset = recording.streams[streamName].startTimestamp;
								eventInfo.timestamps.push_back(*timestamp - offset);
							}
							eventInfoMap[streamName] = eventInfo;

						}

					}

                }

            }
			recordings[recording.id] = recording;

			//Update the total number of samples for each stream in the previous recording
			if (recording.id > 1)
			{

				Recording curr = recordings[recording.id];
				Recording prev = recordings[recording.id - 1];

				std::vector<String> streams;
				for (auto const& imap : recordings[recording.id].streams)
				{
					String streamName = imap.first;

					long int prevStartPos = prev.streams[streamName].startPos;
					long int currStartPos = curr.streams[streamName].startPos;

					prev.streams[streamName].numSamples = currStartPos - prevStartPos;

				}

				recordings[recording.id - 1] = prev;
			}

        }
    }

	///Update the total number of samples for the last recording
	int numRecordings = recordings.size();
	if (numRecordings > 0)
	{
		Recording last = recordings[numRecordings];
		std::vector<String> streams;
		for (auto const& imap : last.streams)
		{
			String streamName = imap.first;

			StreamInfo info = recordings[numRecordings].streams[streamName];
			juce::File dataFile = m_rootPath.getChildFile(info.channels[0].filename);
			int fileSize = dataFile.getSize();
			last.streams[streamName].numSamples = (dataFile.getSize() - info.startPos) / 2070 * 1024;
		}
		recordings[recordings.size()] = last;
	}

	return true;
}

void OpenEphysFileSource::fillRecordInfo()
{

	for (auto rec : extract_keys(recordings))
	{

		for (auto streamName : extract_keys(recordings[rec].streams))
		{

			RecordInfo info;

			info.name = String(streamName);
			info.sampleRate = recordings[rec].streams[streamName].sampleRate;
			juce::File dataFile = m_rootPath.getChildFile(recordings[rec].streams[streamName].channels[0].filename);
			info.numSamples = recordings[rec].streams[streamName].numSamples;

			for (int i = 0; i < recordings[rec].streams[streamName].channels.size(); i++)
			{
				RecordedChannelInfo cInfo;

				cInfo.name = recordings[rec].streams[streamName].channels[i].name;
				cInfo.bitVolts = recordings[rec].streams[streamName].channels[i].bitVolts;
				
				info.channels.add(cInfo);
			}
			infoArray.add(info);
			numRecords++;
			
		}
	}

}

void OpenEphysFileSource::updateActiveRecord(int index)
{

	activeRecord.set(index);
	dataFiles.clear();

	int selectedRecording = 1; //TODO: Currently only supports single recording
	currentStream = extract_keys(recordings[selectedRecording].streams)[index];

	for (int i = 0; i < infoArray[index].channels.size(); i++)
	{
		juce::File dataFile = m_rootPath.getChildFile(recordings[selectedRecording].streams[currentStream].channels[i].filename);
		dataFiles.add(new MemoryMappedFile(dataFile, juce::MemoryMappedFile::readOnly));
	}

	m_samplePos = 0;

	blockIdx = 0;
	samplesLeftInBlock = 0;

	numActiveChannels = getActiveNumChannels();

	bitVolts.clear();

	for (int i = 0; i < numActiveChannels; i++)
		bitVolts.add(getChannelInfo(index, i).bitVolts);
}

void OpenEphysFileSource::seekTo(int64 sample)
{
	m_samplePos = sample % getActiveNumSamples();
}

int OpenEphysFileSource::readData(int16* buffer, int nSamples)
{

	int64 samplesToRead;

	if (m_samplePos + nSamples > getActiveNumSamples())
	{
		// TODO: The last block of data is likely to contain trailing zeros
		// Compute the number of trailing zeros and subtract from samplesToRead here
		samplesToRead = getActiveNumSamples() - m_samplePos;
	}
	else
	{
		samplesToRead = nSamples;
	}

	readSamples(buffer, samplesToRead);

	m_samplePos += samplesToRead;

	return samplesToRead;

}

void OpenEphysFileSource::processChannelData(int16* inBuffer, float* outBuffer, int channel, int64 numSamples)
{

	for (int i = 0; i < numSamples; i++)
	{
		int16 hibyte = (*(inBuffer + (numActiveChannels * i) + channel) & 0x00ff) << 8;
		int16 lobyte = (*(inBuffer + (numActiveChannels * i) + channel) & 0xff00) >> 8;
		*(outBuffer + i) = ( lobyte | hibyte ) * bitVolts[channel];
	}
}

void OpenEphysFileSource::processEventData(EventInfo &eventInfo, int64 start, int64 stop) 
{ 

	int local_start = start % getActiveNumSamples();;
	int local_stop = stop % getActiveNumSamples();
	int loop_count = start / getActiveNumSamples();

	EventInfo info = eventInfoMap[currentStream];

	int i = 0;

	while (i < info.timestamps.size())
	{
		if (info.timestamps[i] >= local_start && info.timestamps[i] <= local_stop)
		{
			eventInfo.channels.push_back(info.channels[i]);
			eventInfo.channelStates.push_back((info.channelStates[i]));
			eventInfo.timestamps.push_back(info.timestamps[i] + loop_count * getActiveNumSamples());
		}
		i++;
	}

};


void OpenEphysFileSource::readSamples(int16* buffer, int64 samplesToRead)
{

	int nChans = getActiveNumChannels();

	/* Organize samples into a vector that mimics BinaryFormat */
	std::vector<int16> samples;

	/* Read rest of previous block */
	if (samplesLeftInBlock > 0) 
	{
		for (int i = 0; i < samplesLeftInBlock; i++) 
		{
			for (int j = 0; j < nChans; j++)
			{
				int16* data = static_cast<int16*>(dataFiles[j]->getData()) + 518 + blockIdx*1035 + (1024 - samplesLeftInBlock) + i;
				samples.push_back(data[0]);
			}
		}

		samplesToRead -= samplesLeftInBlock;
		samplesLeftInBlock = 0;
		blockIdx = (blockIdx + 1) % (infoArray[getActiveRecord()].numSamples / 1024);

	} 

	/* Read full blocks */
	while (samplesToRead >= 1024)
	{
		for (int i = 0; i < 1024; i++)
		{
			for (int j = 0; j < nChans; j++)
			{
				int16* data = static_cast<int16*>(dataFiles[j]->getData()) + 518 + blockIdx*1035 + i;
				samples.push_back(data[0]);
			}
		}

		blockIdx = (blockIdx + 1) % (infoArray[getActiveRecord()].numSamples / 1024);
		samplesToRead -= 1024;

	}

	/* Read only some of next block */
	if (samplesToRead > 0)
	{
		for (int i = 0; i < samplesToRead; i++) 
		{
			for (int j = 0; j < nChans; j++)
			{
				int16* data = static_cast<int16*>(dataFiles[j]->getData()) + 518 + blockIdx*1035 + i;
				samples.push_back(data[0]);
			}
		}

		samplesLeftInBlock = 1024 - samplesToRead;

	}

	memcpy(buffer, &samples[0], samples.size()*sizeof(int16));

}
