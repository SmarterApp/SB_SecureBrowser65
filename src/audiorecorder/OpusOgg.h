#include "opus.h"
#include "opus_types.h"
#include <vector>

class OpusOgg
{
public:

	static bool encode(opus_int32 sampling_rate, int channels, opus_int32 bitrate_bps, std::vector<unsigned char>& input, std::vector<unsigned char>& output, std::string& errMessage);

	static bool decode(std::vector<unsigned char>& inputVec, std::vector<unsigned char>& outputVec, std::string& errMessage, int &sampleRate, int &channels);

};
