//
// Created by zanet on 4/16/2025.
//

#ifndef LUASERVICE_HH
#define LUASERVICE_HH
#include <Common/Service.hh>


namespace Mikoto {
  class LuaService final : public IService<LuaService> {
  public:
    auto Init() -> void override;
    auto Shutdown() -> void override;

  private:

  };
}



#endif //LUASERVICE_HH
