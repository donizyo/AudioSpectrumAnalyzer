// AudioSpectrumAnalyzer.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#define WAVE_FORMAT_PCM (0x0001)        // Microsoft Pulse Code Modulation (PCM) format
#define IBM_FORMAT_MULAW (0x0101)       // IBM mu-law format
#define IBM_FORMAT_ALAW (0x0102)        // IBM a-law format
#define IBM_FORMAT_ADPCM (0x0103)       // IBM AVC Adaptive Differential Pulse Code Modulation format

#define CHUNKID_RIFF (0x52494646)
#define CHUNKID_RIFX (0x52494658)
#define CHUNKID_LIST (0x4c495354)

#define FORMAT_WAVE (0x57415645)
#define FORMAT_PAL (0x50414c20)
#define FORMAT_RDIB (0x52444942)
#define FORMAT_RMID (0x524d4944)
#define FORMAT_RMMP (0x524d4d50)

#define SUBCHUNKID_FMT (0x666d7420)
#define SUBCHUNKID_DATA (0x64617461)


namespace Audio {

    struct Buff
    {
        char buff[8];
        std::ifstream fs;
        int byte_counter;

        Buff(const char * filename)
        {
            fs = std::ifstream(filename, std::ifstream::in);
            byte_counter = 0;
        }

        bool ok() const
        {
            return fs.is_open();
        }

        size_t get_file_size()
        {
            auto current_position = fs.tellg();
            fs.seekg(0, std::ifstream::end);
            auto file_size = fs.tellg();
            fs.seekg(current_position);
            return file_size;
        }

        //====================
        void cls()
        {
            memset(buff, 0, sizeof buff);
        }

        void seek(std::streampos delta)
        {
            auto pos = fs.tellg();
            pos += delta;
            fs.seekg(pos);
        }

        std::streampos cur()
        {
            return fs.tellg();
        }

        //====================
        void r4()
        {
            cls();
            fs.get(buff, 5);
            update_counter(4);
        }
        void r2()
        {
            cls();
            fs.get(buff, 3);
            update_counter(2);
        }

        //====================
        int le4()
        {
            r4();
#if DEBUG
            std::cout << std::hex
                << "$ Test> buff[0] = " << (int)(buff[0] & 0xff) << std::endl
                << "$ Test> buff[1] = " << (int)(buff[1] & 0xff) << std::endl
                << "$ Test> buff[2] = " << (int)(buff[2] & 0xff) << std::endl
                << "$ Test> buff[3] = " << (int)(buff[3] & 0xff) << std::endl;
#endif
            return (buff[0] & 0xff)
                | ((buff[1] & 0xff) << 8)
                | ((buff[2] & 0xff) << 16)
                | ((buff[3] & 0xff) << 24);
        }
        int be4()
        {
            r4();
#if DEBUG
            std::cout << std::hex
                << "$ Test> buff[0] = " << (int)(buff[0] & 0xff) << std::endl
                << "$ Test> buff[1] = " << (int)(buff[1] & 0xff) << std::endl
                << "$ Test> buff[2] = " << (int)(buff[2] & 0xff) << std::endl
                << "$ Test> buff[3] = " << (int)(buff[3] & 0xff) << std::endl;
#endif
            return (buff[3] & 0xff)
                | ((buff[2] & 0xff) << 8)
                | ((buff[1] & 0xff) << 16)
                | ((buff[0] & 0xff) << 24);
        }
        int le2()
        {
            r2();
            return buff[0] | (buff[1] << 8);
        }

        void reset_counter()
        {
            byte_counter = 0;
        }

        void update_counter(int delta)
        {
            byte_counter += delta;
        }

        void skip_leftover(int chunk_size)
        {
            const int leftover = chunk_size - byte_counter;
            std::cout << "[INFO] skip_leftover(chunk_size="
                << std::dec
                << chunk_size << ") -> "
                << leftover << " byte(s)." << std::endl;
            if (leftover > 0)
                fs.seekg(leftover, std::ifstream::cur);
            else if (leftover < 0)
                throw; // assertion error
        }
    };

    // @see: http://soundfile.sapp.org/doc/WaveFormat/
    class WaveFile
    {
    private:
        // The "RIFF" chunk descriptor
        // The Format of concern here is "WAVE",
        // which requires two sub-chunks: "fmt" and "data"
        int ChunkID;                // big endian
        int ChunkSize;              // little endian
        int Format;                 // big endian
        // The "fmt" sub-chunk
        // describes the format of the sound
        // information in the data sub-chunk
        int Subchunk1ID;            // big endian
        int Subchunk1Size;          // little endian
        int AudioFormat;            // little endian
        int NumChannels;            // little endian
        int SampleRate;             // little endian
        int ByteRate;               // little endian
        int BlockAlign;             // little endian
        int BitsPerSample;          // little endian
        // The following two fields exist
        // only when `AudioFormat` doesn't equal 1
        int ExtraParamSize;         // little endian
        std::vector<char> ExtraParams;
        // The "data" sub-chunk
        // indicates the size of the sound information
        // and contains the raw sound data
        int Subchunk2ID;            // big endian
        int Subchunk2Size;          // little endian
        std::vector<char> data;     // little endian

        static bool isBadChunk(int chunkId)
        {
            int bit0 = ((chunkId) & 0xff);
            int bit1 = ((chunkId >> 8) & 0xff);
            int bit2 = ((chunkId >> 16) & 0xff);
            int bit3 = ((chunkId >> 24) & 0xff);
            if ((bit0 == 0x20
                    || bit0 >= 0x61 && bit0 <= 0x7a
                    || bit0 >= 0x41 && bit0 <= 0x5a
                    || bit0 >= 0x30 && bit0 <= 0x39)
                && (bit1 == 0x20
                    || bit1 >= 0x61 && bit1 <= 0x7a
                    || bit1 >= 0x41 && bit1 <= 0x5a
                    || bit1 >= 0x30 && bit1 <= 0x39)
                && (bit2 == 0x20
                    || bit2 >= 0x61 && bit2 <= 0x7a
                    || bit2 >= 0x41 && bit2 <= 0x5a
                    || bit2 >= 0x30 && bit2 <= 0x39)
                && (bit3 == 0x20
                    || bit3 >= 0x61 && bit3 <= 0x7a
                    || bit3 >= 0x41 && bit3 <= 0x5a
                    || bit3 >= 0x30 && bit3 <= 0x39))
                return false;
            return true;
        }

        static bool nextChunk(Buff &buff, int &index, int &chunkID, int &chunkSize)
        {
            auto location = buff.cur();
            chunkID = buff.be4();
            chunkSize = buff.le4();

            std::cout << "[INFO] Parsing subchunk ["
                << std::dec
                << index << "] @"
                << location
                << std::endl
                << "  (id: "
                << std::hex
                << chunkID
                << " '"
                << char(((chunkID >> 24) & 0xff))
                << char(((chunkID >> 16) & 0xff))
                << char(((chunkID >> 8) & 0xff))
                << char((chunkID & 0xff)) << "'"
                << ", size: "
                << std::dec
                << chunkSize << " or "
                << std::hex
                << chunkSize
                << ") ..."
                << std::endl;

            switch (chunkID)
            {
            case SUBCHUNKID_FMT:
            case SUBCHUNKID_DATA:
                break;
            default:
                if (isBadChunk(chunkID))
                {
                    auto pos = buff.cur();
                    pos -= 8;
                    std::cerr << "[ERROR] Bad chunk detected @"
                        << std::dec << index << "!"
                        << std::hex << " SubchunkID: "
                        << chunkID << " '"
                        << char(((chunkID >> 24) & 0xff))
                        << char(((chunkID >> 16) & 0xff))
                        << char(((chunkID >> 8) & 0xff))
                        << char((chunkID & 0xff)) << "'"
                        << std::endl
                        << "\tChunk location: "
                        << std::dec
                        << pos
                        << std::endl;
                    return false;
                }
                // skip unknown chunk
                buff.seek(chunkSize);
                break;
            }

            return true;
        }

        static const char * format2str(int AudioFormat)
        {
            switch (AudioFormat)
            {
            case WAVE_FORMAT_PCM:
                return "PCM";
            case IBM_FORMAT_MULAW:
                return "MULAW";
            case IBM_FORMAT_ALAW:
                return "ALAW";
            case IBM_FORMAT_ADPCM:
                return "ADPCM";
            default:
                return "UNKNOWN";
            }
        }

        static const char * numChan2str(int NumChannels)
        {
            switch (NumChannels)
            {
            case 1:
                return "Mono";
            case 2:
                return "Stereo";
            default:
                return "Unknown";
            }
        }
    public:
        class Chunk
        {
        private:
            int chunkID;
            int chunkSize;
        public:
            Chunk()
            {
                this->chunkID = 0;
                this->chunkSize = 0;
            }

            int const& GetChunkID() const { return this->chunkID; }
            void SetChunkID(int const& newvalue) { this->chunkID = newvalue; }

            int const& GetChunkSize() const { return this->chunkSize; }
            void SetChunkSize(int const& newvalue) { this->chunkSize = newvalue; }
        };

        WaveFile(const char * filename)
        {
            Buff buff(filename);
            int id;
            int size;
            int index;

            //====================
            this->ChunkID = 0;
            this->ChunkSize = 0;
            this->Format = 0;
            this->Subchunk1ID = 0;
            this->Subchunk1Size = 0;
            this->AudioFormat = 0;
            this->NumChannels = 0;
            this->SampleRate = 0;
            this->ByteRate = 0;
            this->BlockAlign = 0;
            this->BitsPerSample = 0;
            this->ExtraParamSize = 0;
            this->Subchunk2ID = 0;
            this->Subchunk2Size = 0;

            //====================
            if (!buff.ok())
            {
                std::cerr << "Failed to open file!" << std::endl;
                return;
            }

            std::cout << "File size: " << buff.get_file_size() << std::endl;

            //====================
            this->ChunkID = buff.be4();
            switch (this->ChunkID)
            {
            case CHUNKID_RIFF:
                std::cout << "ChunkID: "
                    << std::hex << this->ChunkID << " '"
                    << char(((this->ChunkID >> 24) & 0xff))
                    << char(((this->ChunkID >> 16) & 0xff))
                    << char(((this->ChunkID >> 8) & 0xff))
                    << char((this->ChunkID & 0xff)) << "'"
                    << std::endl;
                break;
            case CHUNKID_RIFX:
            case CHUNKID_LIST:
                std::cerr << "ChunkID unsupported!"
                    << std::endl;
                return;
            default:
                if (isBadChunk(this->ChunkID))
                {
                    std::cerr << "Invalid file format [ChunkID: "
                        << std::hex << this->ChunkID << "]!"
                        << std::endl;
                }
                else
                {
                    std::cerr << "Unexpected chunk [ChunkID: "
                        << std::hex << this->ChunkID << "]!"
                        << std::endl;
                }
                return;
            }

            this->ChunkSize = buff.le4();
            std::cout << "ChunkSize: " << std::dec << this->ChunkSize << std::endl;

            this->Format = buff.be4();
            switch (this->Format)
            {
            case FORMAT_WAVE:
                std::cout << "Format: "
                    << std::hex
                    << this->Format
                    << " 'WAVE'"
                    << std::endl;
                break;
            case FORMAT_PAL:
            case FORMAT_RDIB:
            case FORMAT_RMID:
            case FORMAT_RMMP:
                std::cerr << "Format unsupported!"
                    << std::endl;
                return;
            default:
                std::cerr << "Invalid file format [Format: "
                    << std::hex
                    << this->Format << "]!"
                    << std::endl;
                return;
            }

            //====================
            for (index = 0;
                nextChunk(buff, index, id, size);
                ++index)
            {
                switch (id)
                {
                case SUBCHUNKID_FMT:
                    this->Subchunk1ID = id;
                    this->Subchunk1Size = size;

                    buff.reset_counter();
                    //
                    this->AudioFormat = buff.le2();
                    this->NumChannels = buff.le2();
                    this->SampleRate = buff.le4();
                    this->ByteRate = buff.le4();
                    this->BlockAlign = buff.le2();
                    this->BitsPerSample = buff.le2();
                    // optional
                    if (this->Subchunk1Size > 16)
                        this->ExtraParamSize = buff.le2();
                    // exception
                    if (this->ExtraParamSize > 0)
                    {
                        std::cerr << "ExtraParams unsupported!"
                            << std::endl;
                        return;
                    }

                    buff.skip_leftover(size);
                    break;
                case SUBCHUNKID_DATA:
                    this->Subchunk2ID = id;
                    this->Subchunk2Size = size;

                    buff.reset_counter();
                    buff.skip_leftover(size);
                    break;
                default:
                    std::cerr << "Unknown subchunk ID ["
                        << std::hex
                        << id
                        << "]!"
                        << std::endl;

                    buff.reset_counter();
                    buff.skip_leftover(size);
                    break;
                }
            }

            //====================
            std::cout << "Subchunk count: " << std::dec << index << std::endl;
            std::cout << std::endl;
            //====================
            std::cout << "Subchunk1ID: "
                << std::hex
                << this->Subchunk1ID
                << " '"
                << char(((this->Subchunk1ID >> 24) & 0xff))
                << char(((this->Subchunk1ID >> 16) & 0xff))
                << char(((this->Subchunk1ID >> 8) & 0xff))
                << char((this->Subchunk1ID & 0xff)) << "'"
                << std::endl
                << "Subchunk1Size: "
                << std::dec
                << this->Subchunk1Size
                << std::endl
                << "AudioFormat: "
                << std::hex
                << this->AudioFormat
                << ' '
                << format2str(this->AudioFormat)
                << std::endl
                << "NumChannels: "
                << std::dec
                << this->NumChannels
                << ' '
                << numChan2str(this->NumChannels)
                << std::endl
                << "SampleRate: "
                << (this->SampleRate / 1000)
                << " kHz"
                << std::endl
                << "ByteRate: "
                << (this->ByteRate / 1000)
                << " kBps"
                << std::endl
                << "BlockAlign: "
                << this->BlockAlign
                << " bytes"
                << std::endl
                << "BitsPerSample: "
                << this->BitsPerSample
                << " bits"
                << std::endl
                << "ExtraParamSize: "
                << this->ExtraParamSize
                << std::endl
                << std::endl;
            //====================
            std::cout << "Subchunk2ID: "
                << std::hex
                << this->Subchunk2ID
                << " '"
                << char(((this->Subchunk2ID >> 24) & 0xff))
                << char(((this->Subchunk2ID >> 16) & 0xff))
                << char(((this->Subchunk2ID >> 8) & 0xff))
                << char((this->Subchunk2ID & 0xff)) << "'"
                << std::endl
                << "Subchunk2Size: "
                << std::dec
                << this->Subchunk2Size
                << std::endl;
            //====================
            std::cout << std::endl;

            assert(this->ByteRate == this->SampleRate * this->NumChannels * this->BitsPerSample / 8);
            assert(this->BlockAlign == this->NumChannels * this->BitsPerSample / 8);
            assert(this->ChunkSize + 8 == buff.get_file_size());
            //assert(this->ChunkSize == 4 + (8 + this->Subchunk1Size) + (8 + this->Subchunk2Size));
        }
    };
}



int main()
{
    const char * path = "E:\\Development\\Visual_Studio_Projects\\AudioSpectrumAnalyzer\\Test\\" "ah.wav";
    std::cout << std::showbase;
    std::cerr << std::showbase;
    Audio::WaveFile waveFile(path);

    std::cout << "Done." << std::endl;
    system("pause");
    return 0;
}

// http://truelogic.org/wordpress/2015/09/04/parsing-a-wav-file-in-c/
