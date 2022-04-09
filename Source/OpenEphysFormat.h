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

#ifndef OPENEPHYSFORMAT_H_DEFINED
#define OPENEPHYSFORMAT_H_DEFINED

#include <RecordingLib.h>

#include <stdio.h>
#include <map>

#include "Definitions.h"

class OpenEphysFormat : public RecordEngine
{
public:

	/** Constructor */
	OpenEphysFormat();
	
	/** Destructor */
    ~OpenEphysFormat();

	/** Launches the manager for this Record Engine, and instantiates any parameters */
	static RecordEngineManager* getEngineManager();

	/** Returns a string that can be used to identify this record engine*/
	String getEngineId() const;

	/** Called when recording starts to open all needed files */
	void openFiles(File rootFolder, int experimentNumber, int recordingNumber);

	/** Called when recording stops to close all files and do all the necessary cleanup */
	void closeFiles();

	/** Write continuous data for a channel, including synchronized float timestamps for each sample */
	void writeContinuousData(int writeChannel, 
						       int realChannel, 
							   const float* dataBuffer, 
							   const double* timestampBuffer, 
							   int size);

	/** Write a single event to disk (TTL or TEXT) */
	void writeEvent(int eventChannel, 
				    const EventPacket& event);

	/** Write a spike to disk */
	void writeSpike(int electrodeIndex,
		const Spike* spike);

	/** Write the timestamp sync text messages to disk*/
	void writeTimestampSyncText(uint64 streamId, 
								int64 timestamp, 
								float sourceSampleRate, 
								String text);

private:

	/** Generates the name for a continuous data file, given its channel index*/
	String getFileName(int channelIndex);

	/** Opens a file for writing */
	void openFile(File rootFolder, const ChannelInfoObject* ch, int channelIndex);
    
    /** Opens a spike file for writing */
    void openTimestampFile(File rootFolder, const ChannelInfoObject* channel);

	/** Opens a spike file for writing */
	void openSpikeFile(File rootFolder, const SpikeChannel* elec, int channelIndex);

	/** Opens messages.events for writing */
	void openMessageFile(File rootFolder);
	
	/** Writes a block of continuous data*/
    void writeContinuousBuffer(const float* data, const double* timestamps, int nSamples, int channel);

	/** Writes the timestamp sync texts info */
	void writeTimestampAndSampleCount(FILE* file, int channel);
    
    /** Writes the synchronized timestamp for one stream / block combo */
    void writeSynchronizedTimestamp(FILE* file, const double* ts);

	/** Write a 10-byte marker indicating the end of a record */
	void writeRecordMarker(FILE* file);

	/** Writes a TTL event from an EventPacket */
	void writeTTLEvent(int eventIndex, const EventPacket& packet);

	/** Writes a TEXT event to messages.events*/
	void writeMessage(String message, uint16 processorID, int64 timestamp);

	/** Writes the channel metadata XML*/
	void writeXml();

	Array<int> blockIndex;
	Array<int> samplesSinceLastTimestamp;
	uint16 recordingNumber;
	int experimentNumber;

	/** Holds data that has been converted from float to int16 before saving. */
	HeapBlock<int16> continuousDataIntegerBuffer;

	/** Holds data that has been converted from float to int16 before saving. */
	HeapBlock<float> continuousDataFloatBuffer;

	/** Used to indicate the end of each record */
	HeapBlock<uint8> recordMarker;

	AudioBuffer<float> zeroBuffer;
    AudioBuffer<double> zeroBufferDouble;

	FILE* eventFile;
	FILE* messageFile;
	Array<FILE*> fileArray;
    Array<FILE*> timestampFileArray;
    Array<const ContinuousChannel*> firstChannelsInStream;
	Array<FILE*> spikeFileArray;

	CriticalSection diskWriteLock;

	struct ChannelInfo
	{
		String name;
		String filename;
		float bitVolts;
		long int startPos;
	};
	struct ProcInfo
	{
		int id;
		float sampleRate;
		OwnedArray<ChannelInfo> channels;
	};

	OwnedArray<ProcInfo> processorArray;
	int lastProcId;
	String recordPath;
	int64 startTimestamp;
	int procIndex;
	Array<int> originalChannelIndexes;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEphysFormat);
};

#endif
