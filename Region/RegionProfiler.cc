// RegionProfiler.cc: implementation of RegionProfiler class to create x, y, z region profiles

#include "RegionProfiler.h"

using namespace carta;

// ***** spatial *****

bool RegionProfiler::SetSpatialRequirements(const std::vector<std::string>& profiles, const int num_stokes) {
    // Set new spatial requirements for this region
    // process profile strings into pairs <axis, stokes>
    std::vector<SpatialProfile> last_profiles = _spatial_profiles; // for diff
    // Replace spatial profiles with requested profiles
    _spatial_profiles.clear();
    for (const auto& profile : profiles) {
        if (profile.empty() || profile.size() > 2) // ignore invalid profile string
            continue;
        // convert string to pair<axisIndex, stokesIndex>;
        std::pair<int, int> axis_stokes_index = GetAxisStokes(profile);
        if ((axis_stokes_index.first < 0) || (axis_stokes_index.first > 1)) { // invalid axis
            continue;
        }
        if (axis_stokes_index.second > (num_stokes - 1)) { // invalid stokes
            continue;
        }
	SpatialProfile new_spatial_profile;
	new_spatial_profile.profile = profile;
	new_spatial_profile.profile_axes = axis_stokes_index;
	new_spatial_profile.profile_sent = false;
        _spatial_profiles.push_back(new_spatial_profile);
    }
    bool valid(false);
    if (profiles.size() == _spatial_profiles.size()) {
        // Determine diff for required data streams
        DiffSpatialRequirements(last_profiles);
	valid = true;
    }
    return valid;
}

std::pair<int, int> RegionProfiler::GetAxisStokes(std::string profile) {
    // converts profile string into <axis, stokes> pair
    int axis_index(-1), stokes_index(-1);
    // axis
    char axis_char(profile.back());
    if (axis_char == 'x')
        axis_index = 0;
    else if (axis_char == 'y')
        axis_index = 1;
    else if (axis_char == 'z')
        axis_index = 2;
    // stokes
    if (profile.size() == 2) {
        char stokes_char(profile.front());
        if (stokes_char == 'I')
            stokes_index = 0;
        else if (stokes_char == 'Q')
            stokes_index = 1;
        else if (stokes_char == 'U')
            stokes_index = 2;
        else if (stokes_char == 'V')
            stokes_index = 3;
    }
    return std::make_pair(axis_index, stokes_index);
}

void RegionProfiler::DiffSpatialRequirements(std::vector<SpatialProfile>& last_profiles) {
    // Determine which current profiles are new (have unsent data streams)
    for (size_t i = 0; i < NumSpatialProfiles(); ++i) {
        bool found(false);
        for (size_t j = 0; j < last_profiles.size(); ++j) {
            if (_spatial_profiles[i].profile == last_profiles[j].profile) {
                found = true;
		break;
            }
        }
        _spatial_profiles[i].profile_sent = found;
    }
}

size_t RegionProfiler::NumSpatialProfiles() {
    return _spatial_profiles.size();
}

std::pair<int, int> RegionProfiler::GetSpatialProfileAxes(int profile_index) {
    if (profile_index < _spatial_profiles.size()) {
        return _spatial_profiles[profile_index].profile_axes;
    } else {
        return {};
    }
}

std::string RegionProfiler::GetSpatialCoordinate(int profile_index) {
    if (profile_index < _spatial_profiles.size()) {
        return _spatial_profiles[profile_index].profile;
    } else {
        return std::string();
    }
}

bool RegionProfiler::GetSpatialProfileSent(int profile_index) {
    if (profile_index < _spatial_profiles.size()) {
        return _spatial_profiles[profile_index].profile_sent;
    } else {
        return false;
    }
}

void RegionProfiler::SetSpatialProfileSent(int profile_index, bool sent) {
    _spatial_profiles[profile_index].profile_sent = sent;
}

void RegionProfiler::SetAllSpatialProfilesUnsent() {
    for (size_t i=0; i<NumSpatialProfiles(); ++i) {
        _spatial_profiles[i].profile_sent = false;
    }
}
        
// ***** spectral *****

bool RegionProfiler::SetSpectralRequirements(
    const std::vector<CARTA::SetSpectralRequirements_SpectralConfig>& configs, const int num_stokes) {
    // parse stokes into index
    _spectral_configs.clear();
    _spectral_stokes.clear();
    for (const auto& config : configs) {
        std::string coordinate(config.coordinate());
        if (coordinate.empty() || coordinate.size() > 2) // ignore invalid profile string
            continue;
        std::pair<int, int> axis_stokes = GetAxisStokes(coordinate);
        if (axis_stokes.first != 2) // invalid axis
            continue;
        if (axis_stokes.second > (num_stokes - 1)) // invalid stokes
            continue;
        _spectral_configs.push_back(config);
        _spectral_stokes.push_back(axis_stokes.second);
    }
    return (configs.size() == _spectral_configs.size());
}

size_t RegionProfiler::NumSpectralProfiles() {
    return _spectral_configs.size();
}

bool RegionProfiler::GetSpectralConfigStokes(int& stokes, int profile_index) {
    // return Stokes int value at given index; return false if index out of range
    bool index_ok(false);
    if (profile_index < _spectral_stokes.size()) {
        stokes = _spectral_stokes[profile_index];
        index_ok = true;
    }
    return index_ok;
}

std::string RegionProfiler::GetSpectralCoordinate(int profile_index) {
    if (profile_index < _spectral_configs.size())
        return _spectral_configs[profile_index].coordinate();
    else
        return std::string();
}

bool RegionProfiler::GetSpectralConfig(CARTA::SetSpectralRequirements_SpectralConfig& config, int profile_index) {
    // return SpectralConfig at given index; return false if index out of range
    bool index_ok(false);
    if (profile_index < _spectral_configs.size()) {
        config = _spectral_configs[profile_index];
        index_ok = true;
    }
    return index_ok;
}
