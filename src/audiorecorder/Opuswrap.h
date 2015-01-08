#include "opus.h"
#include "opus_types.h"
#include <vector>

class Opuswrap
{
public:

	static bool code(bool encode, bool decode, opus_int32 sampling_rate, int channels, opus_int32 bitrate_bps, std::vector<unsigned char>& input, std::vector<unsigned char>& output, std::string& errMessage);

};

