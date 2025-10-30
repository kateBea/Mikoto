//
// Created by kate on 10/30/25.
//

#ifndef NETWORK_UTILITIES_HH
#define NETWORK_UTILITIES_HH

#include <string>
#include <string_view>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto::Network {

    MKT_NODISCARD auto GetBody(std::string_view apiResponse) -> std::string;

} // Mikoto

#endif
