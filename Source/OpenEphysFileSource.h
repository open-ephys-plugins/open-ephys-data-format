/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2024 Open Ephys

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

#ifndef OPENEPHYSFILESOURCE_H_INCLUDED
#define OPENEPHYSFILESOURCE_H_INCLUDED

#include <FileSourceHeaders.h>

/**

Reads data from a directory that conforms to the standards
of the Open Ephys Data Format

The files are indexed by a "structure.oebin" file, which
is what is loaded into the File Reader.

*/
class OpenEphysFileSource : public FileSource
{
public:
    /** Constructor */
    OpenEphysFileSource();

    /** Destructor */
    ~OpenEphysFileSource() {}

    /** Attempt to open a file, and return true if successful */
    bool open (File file) override;

    /** Add info about available recordings */
    void fillRecordInfo() override;

    /** Seek to a specific sample number */
    void seekTo (int64 sample) override;

    /** Read in nSamples of continuous data into a buffer */
    int readData (float* buffer, int nSamples) override;

    /** Add info about events occurring in an interval */
    void processEventData (EventInfo& info, int64 startSampleNumber, int64 stopSampleNumber) override;

    /** Update the current recording to read from */
    void updateActiveRecord (int index) override;

private:
    /** Helper function for reading in int16 data */
    void readSamples (float* buffer, int64 samplesToRead);

    struct ChannelInfo
    {
        int id;
        String name;
        double bitVolts;
        String filename;
        long int startPos;
    };

    struct StreamInfo
    {
        int sourceId;
        String name;
        float sampleRate;
        std::vector<ChannelInfo> channels;
        int64 startPos;
        int64 startSampleNumber;
        int64 numSamples;
    };

    struct Recording
    {
        int id;
        std::map<String, StreamInfo> streams;
    };

    OwnedArray<MemoryMappedFile> dataFiles;

    std::map<int, Recording> recordings;
    std::map<int, EventInfo> events;

    File m_rootPath;
    int64 m_samplePos;

    int64 totalSamplesRead;
    int64 totalSamples;
    int64 totalBlocks;

    int64 blockIdx;
    int64 samplesLeftInBlock;

    int numActiveChannels;
    Array<float> bitVolts;

    const unsigned int EVENT_HEADER_SIZE_IN_BYTES = 1024;
    const unsigned int BYTES_PER_EVENT = 16;
};

#endif
