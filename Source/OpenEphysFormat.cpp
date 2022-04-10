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

#include "OpenEphysFormat.h"

#include "FileHeaders.h"

OpenEphysFormat::OpenEphysFormat() : 
	recordingNumber(0), 
	experimentNumber(0), 
	zeroBuffer(1, 50000),
    zeroBufferDouble(1, 50000),
	messageFile(nullptr)
{ 
	continuousDataIntegerBuffer.malloc(10000);
	continuousDataFloatBuffer.malloc(10000);
	recordMarker.malloc(10);

	for (int i = 0; i < 9; i++)
	{
		recordMarker[i] = i;
	}
	recordMarker[9] = 255;

	zeroBuffer.clear();
    zeroBufferDouble.clear();
}
	
OpenEphysFormat::~OpenEphysFormat()
{
}


RecordEngineManager* OpenEphysFormat::getEngineManager()
{
	RecordEngineManager* man = new RecordEngineManager("OPENEPHYS", "Open Ephys",
		&(engineFactory<OpenEphysFormat>));
	
	return man;
}

String OpenEphysFormat::getEngineId() const 
{
	return "OPENEPHYS";
}


void OpenEphysFormat::openFiles(File rootFolder, int experimentNumber, int recordingNumber)
{
	fileArray.clear();
    timestampFileArray.clear();
    eventFileArray.clear();
    eventFileMap.clear();
    firstChannelsInStream.clear();
	spikeFileArray.clear();
	blockIndex.clear();
    streamInfoArray.clear();
	samplesSinceLastTimestamp.clear();

    // set
	this->recordingNumber = recordingNumber;
	this->experimentNumber = experimentNumber;

	openMessageFile(rootFolder); // global message file
    
    uint16 activeStreamId = 0;

	for (int i = 0; i < getNumRecordedContinuousChannels(); i++)
	{
		const ContinuousChannel* ch = getContinuousChannel(getGlobalIndex(i));
        
        if (ch->getStreamId() != activeStreamId)
        {
            firstChannelsInStream.add(ch);
            activeStreamId = ch->getStreamId();
            String timestampFileName = openTimestampFile(rootFolder, ch);
            
            StreamInfo* info = new StreamInfo();
            info->streamId = ch->getStreamId();
            info->sourceNodeId = ch->getSourceNodeId();
            info->name = ch->getStreamName();
            info->sampleRate = ch->getSampleRate();
            info->sourceNodeName = ch->getSourceNodeName();
            info->timestampFileName = timestampFileName;
            streamInfoArray.add(info);
        }
        
		String filename = openContinuousFile(rootFolder, ch, getGlobalIndex(i));
		blockIndex.add(0);
		samplesSinceLastTimestamp.add(0);
        
        ChannelInfo* c = new ChannelInfo();
        c->filename = filename;
        c->name = ch->getName();
        c->startPos = ftell(fileArray.getLast());
        c->bitVolts = dynamic_cast<const ContinuousChannel*>(ch)->getBitVolts();
        streamInfoArray.getLast()->channels.add(c);
	}
    
    activeStreamId = 0;
    
    for (int i = 0; i < getNumRecordedEventChannels(); i++)
    {
        const EventChannel* ch = getEventChannel(i);
        
        if (ch->getStreamId() != activeStreamId)
        {
            activeStreamId = ch->getStreamId();
            String eventFileName = openEventFile(rootFolder, ch);

            bool streamExists = false;
            
            for (auto streamInfo : streamInfoArray)
            {
                if (streamInfo->streamId == activeStreamId)
                {
                    streamInfo->eventFileName = eventFileName;
                    streamExists = true;
                    break;
                }
            }
            
            if (!streamExists)
            {
                StreamInfo* info = new StreamInfo();
                info->sourceNodeId = ch->getSourceNodeId();
                info->name = ch->getStreamName();
                info->sampleRate = ch->getSampleRate();
                info->sourceNodeName = ch->getSourceNodeName();
                info->eventFileName = eventFileName;
                streamInfoArray.add(info);
            }

        }
    }
    
    activeStreamId = 0;

    for (int i = 0; i < getNumRecordedSpikeChannels(); i++)
	{
        const SpikeChannel* ch = getSpikeChannel(i);
        
		spikeFileArray.add(nullptr);
		String filename = openSpikeFile(rootFolder, ch, i);
        
        SpikeChannelInfo* c = new SpikeChannelInfo();
        c->filename = filename;
        c->name = ch->getName();
        c->startPos = ftell(spikeFileArray.getLast());
        c->bitVolts = ch->getChannelBitVolts(0);
        c->num_samples = ch->getTotalSamples();
        c->num_channels = ch->getNumChannels();
        
        if (ch->getStreamId() != activeStreamId)
        {
            activeStreamId = ch->getStreamId();

            bool streamExists = false;
            
            for (auto streamInfo : streamInfoArray)
            {
                if (streamInfo->streamId == activeStreamId)
                {
                    streamInfo->spikeChannels.add(c);
                    streamExists = true;
                    break;
                }
            }
            
            if (!streamExists)
            {
                StreamInfo* info = new StreamInfo();
                info->sourceNodeId = ch->getSourceNodeId();
                info->name = ch->getStreamName();
                info->sampleRate = ch->getSampleRate();
                info->sourceNodeName = ch->getSourceNodeName();
                info->spikeChannels.add(c);
                streamInfoArray.add(info);
            }

        }
	}
}

String OpenEphysFormat::openTimestampFile(File rootFolder, const ChannelInfoObject *channel)
{
    FILE* tsFile;
    
    String basePath = rootFolder.getFullPathName() + rootFolder.getSeparatorString();
    
    String filename = String(channel->getSourceNodeId());
    filename += "_";

    // need to indicate stream somehow
    filename += String(channel->getStreamName().removeCharacters(" ").replaceCharacter('_','-'));
    filename += ".timestamps";
    
    String fullpath = basePath + filename;
    
    File f = File(fullpath);
    
    diskWriteLock.enter();

    bool fileExists = f.exists();
    
    if (!fileExists)
    {
        LOGD("OPENING FILE: ", filename);
        tsFile = fopen(fullpath.toUTF8(), "ab");
        timestampFileArray.add(tsFile);
    }
    else
    {
        fseek(tsFile, 0, SEEK_END);
    }

    diskWriteLock.exit();
    
    return filename;
    
}

String OpenEphysFormat::openEventFile(File rootFolder, const ChannelInfoObject* channel)
{
    FILE* eventFile;
    
    String basePath = rootFolder.getFullPathName() + rootFolder.getSeparatorString();
    
    String filename = String(channel->getSourceNodeId());
    filename += "_";

    // need to indicate stream somehow
    filename += String(channel->getStreamName().removeCharacters(" ").replaceCharacter('_','-'));
    filename += ".events";
    
    String fullPath = basePath + filename;
    
    File f = File(fullPath);

    bool fileExists = f.exists();
    
    diskWriteLock.enter();
    
    if (!fileExists)
    {
        LOGD("OPENING FILE: ", filename);
        eventFile = fopen(fullPath.toUTF8(), "ab");
        
        LOGD("Writing header.");
        String header = generateHeader(channel, generateDateString());
        LOGD("File ID: ", eventFile, ", number of bytes: ", header.getNumBytesAsUTF8());

        fwrite(header.toUTF8(), 1, header.getNumBytesAsUTF8(), eventFile);
        
        eventFileArray.add(eventFile);
        eventFileMap[channel->getStreamId()] = eventFile;
    }
    else
    {
        fseek(eventFile, 0, SEEK_END);
    }
    
    
    diskWriteLock.exit();
    
    return filename;
    
}

String OpenEphysFormat::openContinuousFile(File rootFolder, const ChannelInfoObject* ch, int channelIndex)
{
	FILE* chFile;

	String fullPath(rootFolder.getFullPathName() + rootFolder.getSeparatorString());
	String fileName;

	recordPath = fullPath;

    String fname = getFileName(channelIndex);

    fileName += fname;
	fullPath += fileName;
	LOGD("OPENING FILE: ", fullPath);

	File f = File(fullPath);

	bool fileExists = f.exists();

	diskWriteLock.enter();

	chFile = fopen(fullPath.toUTF8(), "ab");

	if (!fileExists)
	{

		// create and write header
		LOGD("Writing header.");
		String header = generateHeader(ch, generateDateString());
		LOGD("File ID: ", chFile, ", number of bytes: ", header.getNumBytesAsUTF8());

		fwrite(header.toUTF8(), 1, header.getNumBytesAsUTF8(), chFile);

		LOGD("Wrote header.");
	}
	else
	{
		LOGD("File already exists, just opening.");
		fseek(chFile, 0, SEEK_END);
	}

    fileArray.add(chFile);

	diskWriteLock.exit();
    
    return fname;

}

String OpenEphysFormat::openSpikeFile(File rootFolder, const SpikeChannel* elec, int channelIndex)
{

	FILE* spFile;
	String fullPath(rootFolder.getFullPathName() + rootFolder.getSeparatorString());
    
	String filename = elec->getName().removeCharacters(" ");
    filename += "_";

	// need to indicate stream somehow
    filename += String(elec->getStreamName().removeCharacters(" ").replaceCharacter('_', '-'));
    filename += "_";

	if (experimentNumber > 1)
	{
        filename += "_" + String(experimentNumber);
	}

    filename += ".spikes";
    
    fullPath += filename;

	LOGD("OPENING FILE: ", fullPath);

	File f = File(fullPath);

	bool fileExists = f.exists();

	diskWriteLock.enter();

	spFile = fopen(fullPath.toUTF8(), "ab");

	if (!fileExists)
	{

		String header = generateHeader(elec, generateDateString());
		fwrite(header.toUTF8(), 1, header.getNumBytesAsUTF8(), spFile);
		LOGD("Wrote header.");
	}
	diskWriteLock.exit();
	spikeFileArray.set(channelIndex, spFile);
	LOGD("Added file.");
    
    return filename;

}

void OpenEphysFormat::openMessageFile(File rootFolder)
{
	FILE* mFile;
	String fullPath(rootFolder.getFullPathName() + rootFolder.getSeparatorString());

	fullPath += "messages";

	if (experimentNumber > 1)
	{
		fullPath += "_" + String(experimentNumber);
	}

	fullPath += ".events";

	LOGD("OPENING FILE: ", fullPath);

	File f = File(fullPath);

	diskWriteLock.enter();

	mFile = fopen(fullPath.toUTF8(), "ab");

	diskWriteLock.exit();
	messageFile = mFile;

}

String OpenEphysFormat::getFileName(int channelIndex)
{
	String filename;

	const ContinuousChannel* ch = getContinuousChannel(channelIndex);

	filename += String(ch->getSourceNodeId());
	filename += "_";

	// need to indicate stream somehow
	filename += String(ch->getStreamName().removeCharacters(" ").replaceCharacter('_','-'));
	filename += "_";
	
	filename += ch->getName().removeCharacters(" ").replaceCharacter('_', '-');

	if (experimentNumber > 1)
	{
		filename += "_" + String(experimentNumber);
	}

	filename += ".continuous";

	return filename;
}


void OpenEphysFormat::closeFiles()
{
	for (int i = 0; i < fileArray.size(); i++)
	{
		if (fileArray[i] != nullptr)
		{
			if (blockIndex[i] < BLOCK_LENGTH)
			{
				// fill out the rest of the current buffer
				writeContinuousBuffer(zeroBuffer.getReadPointer(0),
                                      zeroBufferDouble.getReadPointer(0),
                                      BLOCK_LENGTH - blockIndex[i], i);
				diskWriteLock.enter();
				fclose(fileArray[i]);
				diskWriteLock.exit();
			}
		}
	}
	fileArray.clear();

	blockIndex.clear();
	samplesSinceLastTimestamp.clear();
    
    for (int i = 0; i < timestampFileArray.size(); i++)
    {
        if (timestampFileArray[i] != nullptr)
        {
            diskWriteLock.enter();
            fclose(timestampFileArray[i]);
            timestampFileArray.set(i, nullptr);
            diskWriteLock.exit();
        }
    }
	
    for (int i = 0; i < spikeFileArray.size(); i++)
	{
		if (spikeFileArray[i] != nullptr)
		{
			diskWriteLock.enter();
			fclose(spikeFileArray[i]);
			spikeFileArray.set(i, nullptr);
			diskWriteLock.exit();
		}
	}
	
    for (int i = 0; i < eventFileArray.size(); i++)
	{
		diskWriteLock.enter();
		fclose(eventFileArray[i]);
        eventFileArray.set(i, nullptr);
		diskWriteLock.exit();
	}
    
	if (messageFile != nullptr)
	{
		diskWriteLock.enter();
		fclose(messageFile);
		messageFile = nullptr;
		diskWriteLock.exit();
	}

	writeXml();
}

void OpenEphysFormat::writeContinuousData(int writeChannel, 
                                           int realChannel,
                                           const float* buffer,
                                           const double* timestampBuffer,
                                           int size)
{
	int samplesWritten = 0;

	samplesSinceLastTimestamp.set(writeChannel, 0);

	int nSamples = size;

	while (samplesWritten < nSamples) // there are still unwritten samples in this buffer
	{
		int numSamplesToWrite = nSamples - samplesWritten;

		if (blockIndex[writeChannel] + numSamplesToWrite < BLOCK_LENGTH) // we still have space in this block
		{

			// write buffer to disk!
			writeContinuousBuffer(buffer + samplesWritten,
                timestampBuffer + samplesWritten,
				numSamplesToWrite,
				writeChannel);

			samplesSinceLastTimestamp.set(writeChannel, samplesSinceLastTimestamp[writeChannel] + numSamplesToWrite);
			blockIndex.set(writeChannel, blockIndex[writeChannel] + numSamplesToWrite);
			samplesWritten += numSamplesToWrite;

		}
		else   // there's not enough space left in this block for all remaining samples
		{

			numSamplesToWrite = BLOCK_LENGTH - blockIndex[writeChannel];

			// write buffer to disk!
			writeContinuousBuffer(buffer + samplesWritten,
                timestampBuffer + samplesWritten,
				numSamplesToWrite,
				writeChannel);

			// update our variables
			samplesWritten += numSamplesToWrite;
			samplesSinceLastTimestamp.set(writeChannel, samplesSinceLastTimestamp[writeChannel] + numSamplesToWrite);
			blockIndex.set(writeChannel, 0); // back to the beginning of the block
		}
	}

}

void OpenEphysFormat::writeEvent(int eventChannel, const EventPacket& event)
{

	if (Event::getEventType(event) == EventChannel::TEXT)
	{
		TextEventPtr ev = TextEvent::deserialize(event, getEventChannel(eventChannel));
		if (ev == nullptr) return;
		writeMessage(ev->getText(), ev->getProcessorId(), ev->getTimestamp());
    } else {
        
        const EventChannel* info = getEventChannel(eventChannel);
        writeTTLEvent(info, event);
    }
}


void OpenEphysFormat::writeSpike(int electrodeIndex, const Spike* spike)
{

	if (spikeFileArray[electrodeIndex] == nullptr)
		return;

	HeapBlock<char> spikeBuffer;
	const SpikeChannel* channel = getSpikeChannel(electrodeIndex);

	int totalSamples = channel->getTotalSamples() * channel->getNumChannels();
	int numChannels = channel->getNumChannels();
	int chanSamples = channel->getTotalSamples();

	int totalBytes = totalSamples * 2 + // account for samples
		numChannels * 4 +               // acount for gain
		numChannels * 2 +               // account for thresholds
		42;							    // 42, from SpikeObject.h
	spikeBuffer.malloc(totalBytes);
	*(spikeBuffer.getData()) = static_cast<char>(channel->getChannelType());
	*reinterpret_cast<int64*>(spikeBuffer.getData() + 1) = spike->getTimestamp();
	*reinterpret_cast<int64*>(spikeBuffer.getData() + 9) = 0; //Legacy unused value
	*reinterpret_cast<uint16*>(spikeBuffer.getData() + 17) = spike->getProcessorId();
	*reinterpret_cast<uint16*>(spikeBuffer.getData() + 19) = numChannels;
	*reinterpret_cast<uint16*>(spikeBuffer.getData() + 21) = chanSamples;
	*reinterpret_cast<uint16*>(spikeBuffer.getData() + 23) = spike->getSortedId();
	*reinterpret_cast<uint16*>(spikeBuffer.getData() + 25) = electrodeIndex; //Legacy value
	*reinterpret_cast<uint16*>(spikeBuffer.getData() + 27) = 0; //Legacy unused value
	zeromem(spikeBuffer.getData() + 29, 3 * sizeof(uint8));
	zeromem(spikeBuffer.getData() + 32, 2 * sizeof(float));
	*reinterpret_cast<uint16*>(spikeBuffer.getData() + 40) = channel->getSampleRate();

	int ptrIdx = 0;
	uint16* dataIntPtr = reinterpret_cast<uint16*>(spikeBuffer.getData() + 42);
	const float* spikeDataPtr = spike->getDataPointer();
	for (int i = 0; i < numChannels; i++)
	{
		const float bitVolts = channel->getChannelBitVolts(i);
		for (int j = 0; j < chanSamples; j++)
		{
			*(dataIntPtr + ptrIdx) = uint16(*(spikeDataPtr + ptrIdx) / bitVolts + 32768);
			ptrIdx++;
		}
	}
	ptrIdx = totalSamples * 2 + 42;
	for (int i = 0; i < numChannels; i++)
	{
		//To get the same value as the original version
		*reinterpret_cast<float*>(spikeBuffer.getData() + ptrIdx) = (int)(1.0f / channel->getChannelBitVolts(i)) * 1000;
		ptrIdx += sizeof(float);
	}
	for (int i = 0; i < numChannels; i++)
	{
		*reinterpret_cast<int16*>(spikeBuffer.getData() + ptrIdx) = spike->getThreshold(i);
		ptrIdx += sizeof(int16);
	}

	diskWriteLock.enter();

	fwrite(spikeBuffer, 1, totalBytes, spikeFileArray[electrodeIndex]);

	fwrite(&recordingNumber,             // ptr
		2,                               // size of each element
		1,                               // count
		spikeFileArray[electrodeIndex]); // ptr to FILE object

	diskWriteLock.exit();

}


void OpenEphysFormat::writeTimestampSyncText(
	uint64 streamId, 
	int64 timestamp, 
	float sourceSampleRate, 
	String text)
{
	writeMessage(text, streamId, timestamp);
}


void OpenEphysFormat::writeMessage(String message, uint16 processorID, int64 timestamp)
{
	if (messageFile == nullptr)
		return;

	int msgLength = message.getNumBytesAsUTF8();

	String timestampText(timestamp);

	diskWriteLock.enter();
	fwrite(timestampText.toUTF8(), 1, timestampText.length(), messageFile);
	fwrite(", ", 1, 2, messageFile);
	fwrite(message.toUTF8(), 1, msgLength, messageFile);
	fwrite("\n", 1, 1, messageFile);
	diskWriteLock.exit();

}


void OpenEphysFormat::writeTTLEvent(const EventChannel* info, const EventPacket& packet)
{

	if (eventFileMap.count(info->getStreamId()) == 0)
		return;

	uint8 data[16];

	int16 samplePos = 0;

	EventPtr ev = Event::deserialize(packet, info);
	if (!ev) return;
	*reinterpret_cast<int64*>(data) = ev->getTimestamp();
	*reinterpret_cast<int16*>(data + 8) = samplePos;
	*(data + 10) = static_cast<uint8>(ev->getEventType());
	*(data + 11) = static_cast<uint8>(ev->getProcessorId());
	*(data + 12) = (ev->getEventType() == EventChannel::TTL) ? (dynamic_cast<TTLEvent*>(ev.get())->getState() ? 1 : 0) : 0;
	*(data + 13) = (ev->getEventType() == EventChannel::TTL) ? (dynamic_cast<TTLEvent*>(ev.get())->getLine() ? 1 : 0) : 0;
	*reinterpret_cast<uint16*>(data + 14) = recordingNumber;

	diskWriteLock.enter();

	fwrite(&data,			// ptr
		sizeof(uint8),   	// size of each element
		16, 		  		// count
		eventFileMap[info->getStreamId()]);   		// ptr to FILE object

	diskWriteLock.exit();
}



void OpenEphysFormat::writeContinuousBuffer(const float* data, const double* timestamps, int nSamples, int writeChannel)
{
	// check to see if the file exists
	if (fileArray[writeChannel] == nullptr)
		return;

	// scale the data back into the range of int16
    const ContinuousChannel* ch = getContinuousChannel(getGlobalIndex(writeChannel));
	float scaleFactor = float(0x7fff) * ch->getBitVolts();

	for (int n = 0; n < nSamples; n++)
	{
		*(continuousDataFloatBuffer + n) = *(data + n) / scaleFactor;
	}
	AudioDataConverters::convertFloatToInt16BE(continuousDataFloatBuffer, continuousDataIntegerBuffer, nSamples);

	if (blockIndex[writeChannel] == 0)
	{
		writeTimestampAndSampleCount(fileArray[writeChannel], writeChannel);
	}

	diskWriteLock.enter();

	size_t count = fwrite(continuousDataIntegerBuffer,      // ptr
		2,                                // size of each element
		nSamples,                         // count
		fileArray[writeChannel]); // ptr to FILE object

	LOGB(writeChannel, " : ", nSamples, " : ", count);

	jassert(count == nSamples); // make sure all the data was written
	(void)count;  // Suppress unused variable warning in release builds

	diskWriteLock.exit();

	if (blockIndex[writeChannel] + nSamples == BLOCK_LENGTH)
	{
		writeRecordMarker(fileArray[writeChannel]);
        
        int index = firstChannelsInStream.indexOf(ch);
        
        if (index > -1)
        {
            writeSynchronizedTimestamp(timestampFileArray[index], timestamps + nSamples);
        }
	}
}

void OpenEphysFormat::writeSynchronizedTimestamp(FILE* file, const double* ts)
{
    
    diskWriteLock.enter();
    
    fwrite(ts,         // ptr
        8,              // size of each element
        1,              // count
        file);          // ptr to FILE object
    
    diskWriteLock.exit();
}

void OpenEphysFormat::writeTimestampAndSampleCount(FILE* file, int channel)
{
	diskWriteLock.enter();

	uint16 samps = BLOCK_LENGTH;

	int64 ts = getTimestamp(channel) + samplesSinceLastTimestamp[channel];

	fwrite(&ts,                       // ptr
		8,                               // size of each element
		1,                               // count
		file); // ptr to FILE object

	fwrite(&samps,                           // ptr
		2,                               // size of each element
		1,                               // count
		file); // ptr to FILE object

	fwrite(&recordingNumber,                         // ptr
		2,                               // size of each element
		1,                               // count
		file); // ptr to FILE object

	diskWriteLock.exit();
}

void OpenEphysFormat::writeRecordMarker(FILE* file)
{

	diskWriteLock.enter();

	fwrite(recordMarker,     // ptr
		1,                   // size of each element
		10,                  // count
		file);               // ptr to FILE object

	diskWriteLock.exit();
}


void OpenEphysFormat::writeXml()
{
	String name = recordPath + "structure";
	
    if (experimentNumber > 1)
	{
		name += String(experimentNumber);
	}
	name += ".openephys";

	File file(name);
	XmlDocument doc(file);
	std::unique_ptr<XmlElement> xml = doc.getDocumentElement();

	if (!xml || !xml->hasTagName("EXPERIMENT"))
	{
		xml = std::make_unique<XmlElement>("EXPERIMENT");
		xml->setAttribute("format_version", VERSION_STRING);
		xml->setAttribute("number", experimentNumber);
	}
	
	XmlElement* recordingXml = new XmlElement("RECORDING");
    recordingXml->setAttribute("number", recordingNumber + 1);
	
	for (auto streamInfo : streamInfoArray)
	{
		XmlElement* streamXml = new XmlElement("STREAM");
        streamXml->setAttribute("name", streamInfo->name);
        streamXml->setAttribute("source_node_id", streamInfo->sourceNodeId);
        streamXml->setAttribute("source_node_name", streamInfo->sourceNodeName);
        streamXml->setAttribute("sample_rate", streamInfo->sampleRate);
		
		for (auto channelInfo : streamInfo->channels)
		{
			XmlElement* channelXml = new XmlElement("CHANNEL");
            channelXml->setAttribute("name", channelInfo->name);
            channelXml->setAttribute("bitVolts", channelInfo->bitVolts);
            channelXml->setAttribute("filename", channelInfo->filename);
            channelXml->setAttribute("position", (double)(channelInfo->startPos));  //As long as the file doesnt exceed 2^53 bytes, this will have integer precission. Better than limiting to 32bits.
            streamXml->addChildElement(channelXml);
		}
        
        for (auto channelInfo : streamInfo->spikeChannels)
        {
            XmlElement* channelXml = new XmlElement("SPIKECHANNEL");
            channelXml->setAttribute("name", channelInfo->name);
            channelXml->setAttribute("bitVolts", channelInfo->bitVolts);
            channelXml->setAttribute("filename", channelInfo->filename);
            channelXml->setAttribute("num_channels", channelInfo->num_channels);
            channelXml->setAttribute("num_samples", channelInfo->num_samples);
            channelXml->setAttribute("position", (double)(channelInfo->startPos));  //As long as the file doesnt exceed 2^53 bytes, this will have integer precission. Better than limiting to 32bits.
            streamXml->addChildElement(channelXml);
        }
        
        if (streamInfo->eventFileName.length() > 0)
        {
            XmlElement* eventChannelXml = new XmlElement("EVENTS");
            eventChannelXml->setAttribute("filename", streamInfo->eventFileName);
            streamXml->addChildElement(eventChannelXml);
        }
        
        if (streamInfo->timestampFileName.length() > 0)
        {
            XmlElement* timestampChannelXml = new XmlElement("TIMESTAMPS");
            timestampChannelXml->setAttribute("filename", streamInfo->timestampFileName);
            streamXml->addChildElement(timestampChannelXml);
        }
        
        
        recordingXml->addChildElement(streamXml);
	}
    
	xml->addChildElement(recordingXml);
	xml->writeTo(file);

}

void OpenEphysFormat::setParameter(EngineParameter& parameter)
{
    //NOT USED
    
}
