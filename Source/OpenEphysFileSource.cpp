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

OpenEphysFileSource::OpenEphysFileSource() : 
	m_samplePos(0), 
	totalSamplesRead(0),
	samplesLeftInBlock(0),
	blockIdx(0)
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

				int sourceNodeId = streamTag->getIntAttribute("source_node_id");
				String streamName = String(sourceNodeId) + "_" + streamTag->getStringAttribute("name");
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

							int16* recordingNum = static_cast<int16*>(timestampFileMap->getData()) + 512 + 5;

							int64* startTimestamp = reinterpret_cast<int64*>(recordingNum - 5);

							int count = 0;
							if (*recordingNum != recording.id - 1)
							{
								//Iterate over the file until we find the current recording number
								while (*recordingNum < recording.id - 1)
									recordingNum += 1035;

								//Get the start timestamp for the current recording 
								startTimestamp = reinterpret_cast<int64*>(recordingNum - 5);

							}

							streamInfo.startTimestamp = *startTimestamp;
							recording.streams[streamName] = streamInfo;

						}

						recording.streams[streamName].channels.push_back(info);

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

					prev.streams[streamName].numSamples = (currStartPos - prevStartPos) / 2070 * 1024;

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

	// Load in event data
	for (auto* recordTag: xml->getChildIterator())
    {
        if (recordTag->hasTagName("RECORDING"))
        {

            int id = recordTag->getIntAttribute("number");

			Recording recording = recordings[id];

            for (auto* streamTag: recordTag->getChildIterator())
            {

				int sourceNodeId = streamTag->getIntAttribute("source_node_id");
				String streamName = String(sourceNodeId) + "_" + streamTag->getStringAttribute("name");
				String sourceNodeName = streamTag->getStringAttribute("source_node_name");
				float sampleRate = streamTag->getIntAttribute("sample_rate") * 1.0f;

                for (auto* channel: streamTag->getChildIterator())
                {

                    ChannelInfo info;

                    info.filename = channel->getStringAttribute("filename");

					if (channel->getTagName() == "EVENTS")
					{

						File eventsFile = m_rootPath.getChildFile(info.filename);
						int nEvents = (eventsFile.getSize() - EVENT_HEADER_SIZE_IN_BYTES) / BYTES_PER_EVENT;

						if (nEvents > 0 && !eventInfoMap.count(streamName))
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
								uint16* recordingNum = reinterpret_cast<uint16*>(channel + 1);

								eventInfo.channels.push_back(*channel);
								eventInfo.channelStates.push_back(*channelState);

								// Correct event timestamp offsets based on the current recording number
								int64 offset = recordings[1].streams[streamName].startTimestamp;
								if (*recordingNum > 0)
								{

									int numRecordings = *recordingNum + 1;
									while (numRecordings > 1)
									{
										Recording curr = recordings[numRecordings];
										Recording prev = recordings[numRecordings - 1];
										offset += curr.streams[streamName].startTimestamp - (prev.streams[streamName].numSamples + prev.streams[streamName].startTimestamp); 
										numRecordings--;
									}

								}

								eventInfo.timestamps.push_back(*timestamp - offset);

							}

							eventInfoMap[streamName] = eventInfo;

						}

					}
				}
			}
		}
	}

	return true;
}

void OpenEphysFileSource::fillRecordInfo()
{

	// Note: Currently all recordings are concatenated into a single recording. 
	int recordingNum = 1;

	for (auto streamName : extract_keys(recordings[recordingNum].streams))
	{

		RecordInfo info;

		info.name = String(streamName);
		info.sampleRate = recordings[1].streams[streamName].sampleRate;
		juce::File dataFile = m_rootPath.getChildFile(recordings[recordingNum].streams[streamName].channels[0].filename);

		// Add the number of samples for each recording since they are all concatenated
		info.numSamples = 0;
		for (auto rec : extract_keys(recordings))
			info.numSamples += recordings[rec].streams[streamName].numSamples;

		for (int i = 0; i < recordings[recordingNum].streams[streamName].channels.size(); i++)
		{
			RecordedChannelInfo cInfo;

			cInfo.name = recordings[recordingNum].streams[streamName].channels[i].name;
			cInfo.bitVolts = recordings[recordingNum].streams[streamName].channels[i].bitVolts;
				
			info.channels.add(cInfo);
		}
		infoArray.add(info);
		numRecords++;
			
	}

}

void OpenEphysFileSource::updateActiveRecord(int index)
{

	activeRecord.set(index);
	dataFiles.clear();

	// Note: All recordings are concatenated together (in order) into a single recording
	// This may need to change if individual recordings get sufficiently large. 
	int selectedRecording = 1; 
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

	totalSamples = infoArray[activeRecord.get()].numSamples;
	totalBlocks = totalSamples / 1024; //1024 samples per block

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

	int64 samplesToRead = nSamples;

	// Check if we're at the end of the file
	if (m_samplePos + nSamples > getActiveNumSamples())
		samplesToRead = getActiveNumSamples() - m_samplePos;

	readSamples(buffer, samplesToRead);

	m_samplePos += samplesToRead;

	return samplesToRead;

}

void OpenEphysFileSource::processChannelData(int16* inBuffer, float* outBuffer, int channel, int64 numSamples)
{

	// Convert data from inBuffer to outBuffer based on numSamples for each channel

	for (int i = 0; i < numSamples; i++)
	{
		int16 hibyte = (*(inBuffer + (numActiveChannels * i) + channel) & 0x00ff) << 8;
		int16 lobyte = (*(inBuffer + (numActiveChannels * i) + channel) & 0xff00) >> 8;
		*(outBuffer + i) = ( lobyte | hibyte ) * bitVolts[channel];
	}
}

void OpenEphysFileSource::processEventData(EventInfo &eventInfo, int64 start, int64 stop) 
{ 

	// Find all event data within this start/stop interval

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

	/* Organize samples into a vector that mimics BinaryFormat */
	std::vector<int16> samples;

	/* Read rest of previous block */
	if (samplesLeftInBlock > 0) 
	{
		for (int i = 0; i < samplesLeftInBlock; i++) 
			for (int j = 0; j < numActiveChannels; j++)
				samples.push_back(*(static_cast<int16*>(dataFiles[j]->getData()) + 518 + blockIdx*1035 + (1024 - samplesLeftInBlock) + i));

		totalSamplesRead += samplesLeftInBlock;

		samplesToRead -= samplesLeftInBlock;
		samplesLeftInBlock = 0;
		blockIdx = (blockIdx + 1) % (totalBlocks);

	} 

	/* Read full blocks */
	while (samplesToRead >= 1024)
	{
		for (int i = 0; i < 1024; i++)
			for (int j = 0; j < numActiveChannels; j++)
				samples.push_back(*(static_cast<int16*>(dataFiles[j]->getData()) + 518 + blockIdx*1035 + i));

		totalSamplesRead += 1024;

		blockIdx = (blockIdx + 1) % (totalBlocks);
		samplesToRead -= 1024;

	}

	/* Read only some of next block */
	if (samplesToRead > 0)
	{
		for (int i = 0; i < samplesToRead; i++) 
			for (int j = 0; j < numActiveChannels; j++)
				samples.push_back(*(static_cast<int16*>(dataFiles[j]->getData()) + 518 + blockIdx*1035 + i));

		totalSamplesRead += samplesToRead;

		samplesLeftInBlock = 1024 - samplesToRead;

	}

	memcpy(buffer, &samples[0], samples.size()*sizeof(int16));

}
